#include "notifications.h"
#include "../../third_party/imgui_legacy/imgui.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>
#include <system_error>
#include <utility>

namespace s8::overlay {
namespace {

void remove_front_notification(Notifications& state) {
    if (state.active_count != 0) {
        state.active[state.active_begin].message.clear();
        state.active_begin = (state.active_begin + 1) % state.active.size();
        --state.active_count;
    }
}

void expire_notifications(Notifications& state, std::chrono::steady_clock::time_point now,
    domain::GameEpoch current_epoch) {
    const std::size_t previous_count = state.active_count;
    std::size_t kept = 0;
    for (std::size_t offset = 0; offset < previous_count; ++offset) {
        const std::size_t source = (state.active_begin + offset) % state.active.size();
        const Notification& notice = state.active[source];
        if ((notice.session_epoch && *notice.session_epoch != current_epoch) ||
            std::chrono::duration<float>(now - notice.created).count() >= notice.lifetime_seconds) continue;
        const std::size_t destination = (state.active_begin + kept) % state.active.size();
        if (destination != source) state.active[destination] = std::move(state.active[source]);
        ++kept;
    }
    for (std::size_t offset = kept; offset < previous_count; ++offset) {
        state.active[(state.active_begin + offset) % state.active.size()].message.clear();
    }
    state.active_count = kept;
    if (kept == 0) state.active_begin = 0;
}

struct ToastText {
    std::array<std::string, 3> lines;
    std::size_t count = 0;
};

ToastText wrap_toast_text(const ImFont& font, float font_size, float width, const std::string& message) {
    ToastText result;
    const char* cursor = message.data();
    const char* end = cursor + message.size();
    while (cursor < end && result.count < result.lines.size()) {
        const char* newline = std::find(cursor, end, '\n');
        const char* line_end = font.CalcWordWrapPositionA(font_size / font.FontSize, cursor, newline, width);
        // ImGui may return one byte when even a single UTF-8 glyph cannot fit.
        while (line_end < newline && (static_cast<unsigned char>(*line_end) & 0xc0) == 0x80) ++line_end;
        std::string& line = result.lines[result.count++];
        line.assign(cursor, line_end);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r')) line.pop_back();
        cursor = line_end;
        if (cursor == newline && cursor < end) ++cursor;
        while (cursor < end && (*cursor == ' ' || *cursor == '\r')) ++cursor;
        if (result.count == result.lines.size() && cursor < end) {
            constexpr const char* ellipsis = "...";
            const float suffix = font.CalcTextSizeA(font_size, std::numeric_limits<float>::max(), 0, ellipsis).x;
            while (!line.empty() && font.CalcTextSizeA(font_size, std::numeric_limits<float>::max(), 0, line.c_str()).x + suffix > width) {
                std::size_t last = line.size() - 1;
                while (last > 0 && (static_cast<unsigned char>(line[last]) & 0xc0) == 0x80) --last;
                line.resize(last);
            }
            line += ellipsis;
        }
    }
    if (result.count == 0) result.count = 1;
    return result;
}

void draw_paper_strip(ImDrawList& draw, const UiAtlasTexture& paper, ImVec2 minimum,
    ImVec2 maximum, float edge, float opacity) {
    const float width = maximum.x - minimum.x;
    edge = std::min(edge, width * 0.25f);
    const std::array<float, 4> xs{minimum.x, minimum.x + edge, maximum.x - edge, maximum.x};
    const std::array<float, 4> alphas{0, opacity, opacity, 0};
    for (std::size_t segment = 0; segment < 3; ++segment) {
        const ImVec2 left(xs[segment], minimum.y), right(xs[segment + 1], maximum.y);
        if (paper.texture && paper.width >= 1280 && paper.height >= 368) {
            const float u0 = (0.5f + (xs[segment] - minimum.x) / width * 1279.0f) / paper.width;
            const float u1 = (0.5f + (xs[segment + 1] - minimum.x) / width * 1279.0f) / paper.width;
            const int vertex_begin = draw.VtxBuffer.Size;
            draw.AddImage(paper.texture, left, right, ImVec2(u0, 304.5f / paper.height),
                ImVec2(u1, 367.5f / paper.height), IM_COL32_WHITE);
            for (int vertex = vertex_begin; vertex < draw.VtxBuffer.Size; ++vertex) {
                ImDrawVert& value = draw.VtxBuffer[vertex];
                const float t = std::clamp((value.pos.x - left.x) / (right.x - left.x), 0.0f, 1.0f);
                const auto alpha = static_cast<ImU32>(255 * (alphas[segment] + (alphas[segment + 1] - alphas[segment]) * t));
                value.col = (value.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
            }
        } else {
            const ImU32 a = IM_COL32(235, 229, 209, static_cast<int>(255 * alphas[segment]));
            const ImU32 b = IM_COL32(235, 229, 209, static_cast<int>(255 * alphas[segment + 1]));
            draw.AddRectFilledMultiColor(left, right, a, b, b, a);
        }
    }
}

} // namespace

float notification_duration_seconds(float speed) noexcept {
    if (!std::isfinite(speed) || speed <= 0) speed = 100.0f;
    return std::clamp(600.0f / speed, 2.5f, 15.0f);
}

void append_notification_history(Notifications& state, std::string_view message) {
    const std::size_t slot = (state.history_begin + state.history_count) % state.history.size();

    state.history[slot].assign(message);
    if (state.history_count == state.history.size()) {
        state.history_begin = (state.history_begin + 1) % state.history.size();
    } else {
        ++state.history_count;
    }
}

void enqueue_notification(Notifications& state, std::string_view message, float speed,
    std::optional<domain::GameEpoch> session_epoch) {
    if (state.active_count == state.active.size()) remove_front_notification(state);
    Notification& slot = state.active[(state.active_begin + state.active_count) % state.active.size()];
    slot.message.assign(message);
    slot.lifetime_seconds = notification_duration_seconds(speed);
    slot.created = std::chrono::steady_clock::now();
    slot.session_epoch = session_epoch;
    ++state.active_count;
    append_notification_history(state, slot.message);
}

void clear_active_notifications(Notifications& state) {
    for (Notification& notice : state.active) {
        notice.message.clear();
    }
    state.active_begin = 0;
    state.active_count = 0;
}

void draw_notifications(Notifications& state, services::UiBridge& bridge, float speed,
    const UiAtlasTextures& atlases, domain::GameEpoch current_epoch) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    expire_notifications(state, now, current_epoch);
    // Drain at most the bounded bridge queue plus its overflow summary. A burst
    // reaches history immediately; only the newest few strips occupy the HUD.
    for (std::size_t accepted = 0; accepted < 129; ++accepted) {
        const std::optional<services::PendingNotice> notice = bridge.take_notice();

        if (!notice) {
            break;
        }

        std::string message = notice->message;
        if (notice->repeat > 1) {
            std::array<char, 32> digits{};
            const std::to_chars_result count = std::to_chars(digits.data(), digits.data() + digits.size(), notice->repeat);

            if (count.ec == std::errc{}) {
                message.append(" (x");
                message.append(digits.data(), count.ptr);
                message.push_back(')');
            }
        }
        enqueue_notification(state, message, speed);
    }

    if (state.active_count != 0) {
        constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        const ImGuiViewport& viewport = *ImGui::GetMainViewport();
        const float hud_scale = std::clamp(std::min(viewport.Size.x / 1920.0f,
            viewport.Size.y / 1080.0f), 0.1f, 3.0f);
        const float scale = std::clamp(hud_scale, 0.85f, 2.0f);
        const float width = std::max(1.0f, std::min(760.0f * scale, viewport.Size.x - 32.0f));
        const float padding = std::min(64.0f * scale, width * 0.2f);
        const float text_width = std::max(1.0f, width - padding * 2);
        const float font_size = 28.0f * scale;
        const ImFont& font = *ImGui::GetFont();
        float bottom = viewport.Pos.y + viewport.Size.y - 112.0f * hud_scale;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
        for (std::size_t index = 0; index < std::min<std::size_t>(state.active_count, 4); ++index) {
            const Notification& notice = state.active[(state.active_begin + state.active_count - 1 - index) % state.active.size()];
            const ToastText text = wrap_toast_text(font, font_size, text_width, notice.message);
            const float height = std::max(44.0f * scale, font_size * text.count + 16.0f * scale);
            if (bottom - height < viewport.Pos.y + viewport.Size.y * 0.35f) break;
            const float age = std::chrono::duration<float>(now - notice.created).count();
            const float opacity = std::clamp((notice.lifetime_seconds - age) / 0.6f, 0.0f, 1.0f) * ImGui::GetStyle().Alpha;
            const ImVec2 position(viewport.Pos.x + (viewport.Size.x - width) * 0.5f, bottom - height);
            ImGui::SetNextWindowPos(position, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
            std::array<char, 32> name{};
            std::snprintf(name.data(), name.size(), "##NotificationToast%zu", index);
            if (ImGui::Begin(name.data(), nullptr, FLAGS)) {
                ImDrawList& draw = *ImGui::GetWindowDrawList();
                draw_paper_strip(draw, atlases[2], position, ImVec2(position.x + width, bottom), padding, opacity);
                const ImU32 ink = IM_COL32(42, 35, 28, static_cast<int>(255 * opacity));
                const float first_y = position.y + (height - font_size * text.count) * 0.5f;
                for (std::size_t line = 0; line < text.count; ++line) {
                    draw.AddText(&font, font_size, ImVec2(position.x + padding, first_y + font_size * line),
                        ink, text.lines[line].c_str());
                }
            }
            ImGui::End();
            bottom = position.y - 5.0f * scale;
        }
        ImGui::PopStyleVar(3);
    }
    if (state.show_history) {
        const ImVec2 display = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(display.x - 20, display.y * 0.12f + 5), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
        if (ImGui::Begin("알림 기록##history", &state.show_history, ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
            if (ImGui::Button("기록 삭제")) {
                for (std::string& message : state.history) {
                    message.clear();
                }
                state.history_begin = 0;
                state.history_count = 0;
            }
            ImGui::Separator();
            if (state.history_count == 0) {
                ImGui::TextDisabled("기록된 내용이 없습니다.");
            }
            if (ImGui::BeginChild("##history_scroll", ImVec2(0, 300), false)) {
                for (std::size_t offset = 0; offset < state.history_count; ++offset) {
                    const std::size_t index = (state.history_begin + state.history_count - 1 - offset) % state.history.size();

                    ImGui::TextWrapped("- %s", state.history[index].c_str());
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

} // namespace s8::overlay
