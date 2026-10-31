#include "debug_tools_panel.h"

#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <system_error>

namespace s8::overlay {
namespace {

constexpr std::array<const char*, 6> VALUE_TYPES{"1바이트", "2바이트", "4바이트", "8바이트", "float", "double"};

const char* status_text(domain::DebugStatus status) {
    switch (status) {
    case domain::DebugStatus::Ready: return "준비됨";
    case domain::DebugStatus::Scanning: return "검색 중";
    case domain::DebugStatus::Complete: return "검색 완료";
    case domain::DebugStatus::Cancelled: return "중지됨 · 처리된 결과 유지";
    case domain::DebugStatus::SessionChanged: return "게임이 바뀌어 이전 주소와 작업을 초기화했습니다.";
    case domain::DebugStatus::InvalidRequest: return "입력 범위나 현재 작업 상태를 확인하세요.";
    case domain::DebugStatus::ReadFailed: return "이 주소에서 읽을 수 있는 메모리가 없습니다.";
    case domain::DebugStatus::WriteFailed: return "바이트 쓰기 또는 쓰기 검증에 실패했습니다.";
    case domain::DebugStatus::Conflict: return "읽은 뒤 값이 변경되어 쓰기를 취소했습니다. 새 값을 확인하세요.";
    }
    return "";
}

bool submit(DebugToolsPanel& panel, services::DebugBridge& bridge, domain::GameEpoch epoch,
             domain::DebugRequest request) {
    request.id = ++panel.next_request;
    request.epoch = epoch;
    if (!bridge.push(request)) {
        panel.local_error = "요청 큐가 닫혔거나 가득 찼습니다.";
        return false;
    }
    panel.pending_request = request.id;
    panel.local_error = nullptr;
    return true;
}

bool parse_address(const DebugToolsPanel& panel, std::uintptr_t& address) {
    std::string_view input(panel.address.data());

    if (input.starts_with("0x") || input.starts_with("0X")) {
        input.remove_prefix(2);
    }
    const std::from_chars_result result = std::from_chars(input.data(), input.data() + input.size(), address, 16);

    return !input.empty() && result.ec == std::errc{} && result.ptr == input.data() + input.size() && address;
}

void choose_address(DebugToolsPanel& panel, std::uintptr_t address) {
    std::snprintf(panel.address.data(), panel.address.size(), "%llX", static_cast<unsigned long long>(address));
    panel.last_refresh = 0;
    panel.selected_byte = std::numeric_limits<std::size_t>::max();
}

void request_view(DebugToolsPanel& panel, services::DebugBridge& bridge,
                   domain::GameEpoch epoch, domain::DebugAction action = domain::DebugAction::Read) {
    std::uintptr_t address = 0;

    if (!parse_address(panel, address)) {
        panel.local_error = "16진수 메모리 주소를 입력하세요.";
        return;
    }
    domain::DebugRequest request;

    request.action = action;
    request.address = address;
    request.count = static_cast<std::size_t>(std::clamp(panel.rows, 1, 256));
    if (submit(panel, bridge, epoch, request)) {
        panel.last_refresh = ImGui::GetTime();
    }
}

void copy_memory(DebugToolsPanel& panel) {
    std::size_t used = 0;

    for (std::size_t row = 0; row < panel.snapshot.view_size / 16; ++row) {
        std::array<char, 49> hex{};
        std::array<char, 17> ascii{};

        for (std::size_t column = 0; column < 16; ++column) {
            const std::size_t offset = row * 16 + column;
            const unsigned byte = std::to_integer<unsigned>(panel.snapshot.bytes[offset]);

            if (panel.snapshot.readable[offset]) {
                std::snprintf(hex.data() + column * 3, 4, "%02X ", byte);
                ascii[column] = byte >= 32 && byte <= 126 ? static_cast<char>(byte) : '.';
            } else {
                std::memcpy(hex.data() + column * 3, "?? ", 3);
                ascii[column] = '?';
            }
        }
        const int written = std::snprintf(panel.clipboard.data() + used, panel.clipboard.size() - used,
            "%016llX  %s |%s|\n", static_cast<unsigned long long>(panel.snapshot.view_address + row * 16),
            hex.data(), ascii.data());

        if (written <= 0 || static_cast<std::size_t>(written) >= panel.clipboard.size() - used) {
            break;
        }
        used += static_cast<std::size_t>(written);
    }
    panel.clipboard[used] = '\0';
    ImGui::SetClipboardText(panel.clipboard.data());
}

void draw_notes(DebugToolsPanel& panel) {
    if (ImGui::Button("전체 복사")) {
        ImGui::SetClipboardText(panel.notes.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("내용 지우기")) {
        panel.notes.front() = '\0';
    }
    ImGui::SameLine();
    ImGui::TextDisabled("32767바이트 · 게임 종료까지 보관");
    ImGui::InputTextMultiline("##notes", panel.notes.data(), panel.notes.size(), ImVec2(-1, -1));
}

void draw_memory(DebugToolsPanel& panel, services::DebugBridge& bridge, domain::GameEpoch epoch) {
    ImGui::SetNextItemWidth(260);
    const bool address_changed = ImGui::InputText("주소 (HEX)", panel.address.data(), panel.address.size(),
                                                 ImGuiInputTextFlags_CharsHexadecimal);
    if (address_changed) {
        panel.selected_byte = std::numeric_limits<std::size_t>::max();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(95);
    ImGui::InputInt("행", &panel.rows);
    panel.rows = std::clamp(panel.rows, 1, 256);
    if (ImGui::Button("실행 파일")) {
        choose_address(panel, panel.snapshot.module);
    }
    ImGui::SameLine();
    if (ImGui::Button("gameBase")) {
        choose_address(panel, panel.snapshot.game_base);
    }
    ImGui::SameLine();
    if (ImGui::Button("주인공")) {
        choose_address(panel, panel.snapshot.player);
    }
    ImGui::SameLine();
    if (ImGui::Button("읽기")) {
        request_view(panel, bridge, epoch);
    }
    ImGui::SameLine();
    ImGui::Checkbox("자동 갱신 (250ms)", &panel.auto_refresh);

    if (ImGui::Button("비교 기준 캡처")) {
        request_view(panel, bridge, epoch, domain::DebugAction::Capture);
    }
    ImGui::SameLine();
    if (ImGui::Button("비교 기준 지우기")) {
        domain::DebugRequest request;

        request.action = domain::DebugAction::ClearCapture;
        submit(panel, bridge, epoch, request);
    }
    ImGui::SameLine();
    if (ImGui::Button("표시 메모리 복사")) {
        copy_memory(panel);
    }

    if (!address_changed && panel.auto_refresh && !panel.pending_request &&
        panel.address.front() && ImGui::GetTime() - panel.last_refresh >= 0.25) {
        request_view(panel, bridge, epoch);
    }
    const bool current_epoch = panel.snapshot.epoch.value == epoch.value;
    const bool selected = current_epoch && panel.selected_byte < panel.snapshot.view_size &&
        panel.snapshot.readable[panel.selected_byte];

    ImGui::BeginDisabled(!selected || panel.pending_request != 0);
    ImGui::SetNextItemWidth(90);
    ImGui::InputScalar("선택 바이트", ImGuiDataType_U8, &panel.replacement, nullptr, nullptr, "%02X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    if (ImGui::Button("바이트 적용") && selected) {
        domain::DebugRequest request;

        request.action = domain::DebugAction::Write;
        request.address = panel.snapshot.view_address + panel.selected_byte;
        request.expected = panel.selected_expected;
        request.replacement = static_cast<std::byte>(panel.replacement);
        submit(panel, bridge, epoch, request);
    }
    ImGui::EndDisabled();

    const bool visible = ImGui::BeginChild("##memory_rows", ImVec2(0, 0), true,
                                          ImGuiWindowFlags_HorizontalScrollbar);

    if (visible) {
        ImGuiListClipper clipper;

        clipper.Begin(static_cast<int>(panel.snapshot.view_size / 16));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                std::array<char, 17> ascii{};

                ImGui::Text("%016llX", static_cast<unsigned long long>(panel.snapshot.view_address + row * 16));
                for (std::size_t column = 0; column < 16; ++column) {
                    const std::size_t offset = static_cast<std::size_t>(row) * 16 + column;
                    const std::uintptr_t address = panel.snapshot.view_address + offset;
                    const bool captured = address >= panel.snapshot.capture_address &&
                        address - panel.snapshot.capture_address < panel.snapshot.capture_size;
                    const std::size_t capture_offset = captured ? address - panel.snapshot.capture_address : 0;
                    const bool changed = captured && panel.snapshot.readable[offset] &&
                        panel.snapshot.capture_readable[capture_offset] &&
                        panel.snapshot.bytes[offset] != panel.snapshot.capture[capture_offset];
                    const unsigned byte = std::to_integer<unsigned>(panel.snapshot.bytes[offset]);
                    std::array<char, 3> label{};

                    ascii[column] = !panel.snapshot.readable[offset] ? '?' :
                        byte >= 32 && byte <= 126 ? static_cast<char>(byte) : '.';
                    ImGui::SameLine();
                    ImGui::PushID(static_cast<int>(offset));
                    if (changed) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.1f, 1.0f));
                    }
                    if (panel.snapshot.readable[offset]) {
                        std::snprintf(label.data(), label.size(), "%02X", byte);
                        if (ImGui::SmallButton(label.data())) {
                            panel.selected_byte = offset;
                            panel.replacement = static_cast<std::uint8_t>(byte);
                            panel.selected_expected = panel.snapshot.bytes[offset];
                        }
                    } else {
                        ImGui::TextDisabled("??");
                    }
                    if (changed) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::PopID();
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(ascii.data());
            }
        }
    }
    ImGui::EndChild();
}

void start_search(DebugToolsPanel& panel, services::DebugBridge& bridge,
                  domain::GameEpoch epoch, bool refine) {
    domain::MemoryValue value;

    if (!domain::parse_memory_value(static_cast<domain::MemoryValueType>(panel.value_type),
                                    panel.search_value.data(), value)) {
        panel.local_error = "선택한 자료형에 맞는 숫자를 입력하세요. 정수는 0x 접두사로 HEX를 사용할 수 있습니다.";
        return;
    }
    domain::DebugRequest request;

    request.action = refine ? domain::DebugAction::Refine : domain::DebugAction::Search;
    request.value = value;
    request.offset = panel.offset;
    request.radius = panel.nearby ? static_cast<std::uint16_t>(std::clamp(panel.radius, 0, 4096)) : 0;
    submit(panel, bridge, epoch, request);
}

void draw_search(DebugToolsPanel& panel, services::DebugBridge& bridge, domain::GameEpoch epoch) {
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("자료형", &panel.value_type, VALUE_TYPES.data(), static_cast<int>(VALUE_TYPES.size()));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("값", panel.search_value.data(), panel.search_value.size());
    ImGui::BeginDisabled(panel.snapshot.scanning || panel.pending_request != 0);
    if (ImGui::Button("처음 검색")) {
        start_search(panel, bridge, epoch, false);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(panel.snapshot.matches == 0);
    if (ImGui::Button("다음 검색")) {
        start_search(panel, bridge, epoch, true);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!panel.snapshot.scanning || panel.pending_request != 0);
    if (ImGui::Button("검색 중지")) {
        domain::DebugRequest request;

        request.action = domain::DebugAction::Cancel;
        submit(panel, bridge, epoch, request);
    }
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("다음 검색 오프셋", &panel.offset);
    ImGui::SameLine();
    ImGui::Checkbox("주변 검색", &panel.nearby);
    if (panel.nearby) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("반경", &panel.radius);
        panel.radius = std::clamp(panel.radius, 0, 4096);
    }

    ImGui::Text("%s · 결과 %llu개 · 읽음 %.2f MiB", status_text(panel.snapshot.status),
        static_cast<unsigned long long>(panel.snapshot.matches),
        static_cast<double>(panel.snapshot.scanned_bytes) / (1024.0 * 1024.0));
    if (panel.snapshot.result_limit_reached) {
        ImGui::TextDisabled("100001개 한도에 도달했습니다. 다음 검색으로 결과를 좁힐 수 있습니다.");
    }
    const std::size_t pages = panel.snapshot.matches
        ? (panel.snapshot.matches - 1) / domain::DEBUG_RESULT_PAGE + 1 : 1;

    ImGui::BeginDisabled(panel.snapshot.page == 0 || panel.pending_request != 0);
    if (ImGui::Button("이전 페이지")) {
        domain::DebugRequest request;

        request.action = domain::DebugAction::ResultPage;
        request.count = panel.snapshot.page - 1;
        submit(panel, bridge, epoch, request);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%llu / %llu", static_cast<unsigned long long>(panel.snapshot.page + 1),
                static_cast<unsigned long long>(pages));
    ImGui::SameLine();
    ImGui::BeginDisabled(panel.snapshot.page + 1 >= pages || panel.pending_request != 0);
    if (ImGui::Button("다음 페이지")) {
        domain::DebugRequest request;

        request.action = domain::DebugAction::ResultPage;
        request.count = panel.snapshot.page + 1;
        submit(panel, bridge, epoch, request);
    }
    ImGui::EndDisabled();

    const bool visible = ImGui::BeginChild("##scan_results", ImVec2(0, 0), true);

    if (visible) {
        ImGuiListClipper clipper;

        clipper.Begin(static_cast<int>(panel.snapshot.page_count));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const domain::MemoryMatch& match = panel.snapshot.results[static_cast<std::size_t>(row)];
                std::array<char, 96> label{};
                std::uint64_t integer = 0;

                std::memcpy(&integer, match.value.data(), domain::memory_value_size(panel.snapshot.scan_type));
                if (panel.snapshot.scan_type == domain::MemoryValueType::Float) {
                    float number = 0;

                    std::memcpy(&number, match.value.data(), sizeof(number));
                    std::snprintf(label.data(), label.size(), "%016llX    %.9g",
                        static_cast<unsigned long long>(match.address), static_cast<double>(number));
                } else if (panel.snapshot.scan_type == domain::MemoryValueType::Double) {
                    double number = 0;

                    std::memcpy(&number, match.value.data(), sizeof(number));
                    std::snprintf(label.data(), label.size(), "%016llX    %.17g",
                        static_cast<unsigned long long>(match.address), number);
                } else {
                    std::snprintf(label.data(), label.size(), "%016llX    %llu (0x%llX)",
                        static_cast<unsigned long long>(match.address), static_cast<unsigned long long>(integer),
                        static_cast<unsigned long long>(integer));
                }
                ImGui::PushID(row);
                if (ImGui::Selectable(label.data())) {
                    choose_address(panel, match.address);
                    panel.memory_selected = true;
                    request_view(panel, bridge, epoch);
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

} // namespace

void draw_debug_tools(DebugToolsPanel& panel, services::DebugBridge& bridge, domain::GameEpoch epoch) {
    if (!panel.open) {
        return;
    }
    if (bridge.snapshot_since(panel.snapshot.revision, panel.snapshot) &&
        panel.snapshot.completed_request >= panel.pending_request) {
        panel.pending_request = 0;
    }

    ImGui::SetNextWindowSize(ImVec2(980, 620), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("메모장 · 메모리 도구###EditorUtilities", &panel.open);

    if (visible) {
        if (panel.local_error) {
            ImGui::TextWrapped("%s", panel.local_error);
        } else if (panel.snapshot.request_status != domain::DebugStatus::Ready) {
            ImGui::TextWrapped("%s", status_text(panel.snapshot.request_status));
        }
        if (ImGui::BeginTabBar("##utility_tabs")) {
            if (ImGui::BeginTabItem("메모장")) {
                draw_notes(panel);
                ImGui::EndTabItem();
            }
            const ImGuiTabItemFlags flags = panel.memory_selected ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

            if (ImGui::BeginTabItem("메모리 편집", nullptr, flags)) {
                panel.memory_selected = false;
                draw_memory(panel, bridge, epoch);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("메모리 검색")) {
                draw_search(panel, bridge, epoch);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

} // namespace s8::overlay
