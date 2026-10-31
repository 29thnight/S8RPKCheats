#include "runtime_services.h"

#include "../platform/windows/config_file.h"
#include "../platform/windows/config_commit.h"
#include "../platform/windows/speed_clock.h"
#include "../services/config_service_loop.h"
#include "../../third_party/nlohmann_json/single_include/nlohmann/json.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <wincodec.h>
#include <winrt/base.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace s8::runtime {
namespace {

struct ComApartment {
    HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ~ComApartment() {
        if (SUCCEEDED(status)) {
            CoUninitialize();
        }
    }
};

} // namespace

std::expected<void, int> start_runtime_services(RuntimeServices& state, HWND game_window,
                                                 std::uintptr_t plugin_module) {
    const auto path = platform::windows::config_path(plugin_module);
    if (!path) {
        return std::unexpected(static_cast<int>(ERROR_PATH_NOT_FOUND));
    }
    state.config_thread = std::jthread([&state, path = *path, plugin_module](std::stop_token stop) {
        try {
            const platform::windows::RealClockScope real_clock;
            const ComApartment apartment;

            if (SUCCEEDED(apartment.status)) {
                {
                    services::RangeImages images;
                    winrt::com_ptr<IWICImagingFactory> factory;
                    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(factory.put())))) {
                        const auto module = reinterpret_cast<HMODULE>(plugin_module);
                        for (std::size_t i = 0; i < images.size(); ++i) {
                            const auto resource = FindResourceW(module, MAKEINTRESOURCEW(101 + i), RT_RCDATA);
                            if (!resource) {
                                continue;
                            }
                            const auto size = SizeofResource(module, resource);
                            const auto data = LockResource(LoadResource(module, resource));
                            winrt::com_ptr<IWICStream> stream;
                            winrt::com_ptr<IWICBitmapDecoder> decoder;
                            winrt::com_ptr<IWICBitmapFrameDecode> frame;
                            winrt::com_ptr<IWICFormatConverter> converter;
                            auto& image = images[i];
                            if (!data || !size || FAILED(factory->CreateStream(stream.put())) ||
                                FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(data), size)) ||
                                FAILED(factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.put())) ||
                                FAILED(decoder->GetFrame(0, frame.put())) ||
                                FAILED(frame->GetSize(&image.width, &image.height)) ||
                                FAILED(factory->CreateFormatConverter(converter.put())) ||
                                FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppRGBA,
                                       WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
                                image = {};
                                continue;
                            }
                            const auto stride = image.width * 4;
                            image.pixels.resize(static_cast<std::size_t>(stride) * image.height);
                            if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(image.pixels.size()),
                                                             reinterpret_cast<BYTE*>(image.pixels.data())))) {
                                image = {};
                            }
                        }
                    }
                    state.overlay.bridge.publish_range_images(std::move(images));
                }
            }
            domain::NamedSpecialties named{};
            const auto directory = path.parent_path();
            domain::SkillCounts skill_counts;
            auto skill_baseline = platform::windows::read_config_for_save(directory / L"S8RPK_skill_counts.json");
            bool skill_file_valid = skill_baseline.has_value();
            if (skill_file_valid && *skill_baseline) {
                const auto json = nlohmann::json::parse(**skill_baseline, nullptr, false);
                skill_file_valid = json.is_object();
                if (json.is_object()) {
                    for (auto it = json.begin(); it != json.end(); ++it) {
                        int id = 0;
                        const auto& key = it.key();
                        const auto parsed = std::from_chars(key.data(), key.data() + key.size(), id);
                        if (parsed.ec != std::errc{} || parsed.ptr != key.data() + key.size() ||
                            !it.value().is_object()) {
                            continue;
                        }
                        for (auto skill = it.value().begin(); skill != it.value().end(); ++skill) {
                            std::uintptr_t offset = 0;
                            const auto& skill_key = skill.key();
                            const auto parsed_offset = std::from_chars(skill_key.data(), skill_key.data() + skill_key.size(), offset);
                            if (parsed_offset.ec != std::errc{} || parsed_offset.ptr != skill_key.data() + skill_key.size() ||
                                !skill.value().is_number_integer()) {
                                continue;
                            }
                            if (skill.value().is_number_unsigned()) {
                                const auto value = skill.value().get<std::uint64_t>();
                                if (value <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
                                    skill_counts[id][offset] = static_cast<int>(value);
                                }
                            } else {
                                const auto value = skill.value().get<std::int64_t>();
                                if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
                                    skill_counts[id][offset] = static_cast<int>(value);
                                }
                            }
                        }
                    }
                }
            }
            if (skill_file_valid) {
                state.overlay.bridge.publish_skill_counts(std::move(skill_counts));
            }
            domain::OfficerNames officer_names;
            std::ifstream names_file(directory / L"S8RPK_cheat_char.json", std::ios::binary);
            std::string names_text;
            if (names_file) {
                names_text.assign(std::istreambuf_iterator<char>(names_file), std::istreambuf_iterator<char>());
            }
            if (!names_text.empty()) {
                if (auto names = services::read_officer_names(names_text)) {
                    officer_names = std::move(*names);
                }
            }
            state.overlay.bridge.publish_officer_names(std::move(officer_names));
            std::string effect_text;
            std::ifstream effect_file(directory / L"effect_definitions.json", std::ios::binary);
            if (effect_file) {
                effect_text.assign(std::istreambuf_iterator<char>(effect_file), std::istreambuf_iterator<char>());
            } else {
                const auto module = reinterpret_cast<HMODULE>(plugin_module);
                if (const auto resource = FindResourceW(module, MAKEINTRESOURCEW(202), RT_RCDATA)) {
                    const auto size = SizeofResource(module, resource);
                    const auto bytes = static_cast<const char*>(LockResource(LoadResource(module, resource)));
                    if (bytes && size) {
                        effect_text.assign(bytes, size);
                    }
                }
            }
            domain::TraitEffectDefinitions effect_definitions;
            const auto effect_json = nlohmann::json::parse(effect_text, nullptr, false);
            if (effect_json.is_array()) {
                for (const auto& item : effect_json) {
                    if (!item.is_object() || !item.contains("type") || !item["type"].is_number_integer() ||
                        !item.contains("template") || !item["template"].is_string()) {
                        continue;
                    }
                    const auto type = item["type"].get<std::int64_t>();
                    if (type < 0 || type > std::numeric_limits<std::uint16_t>::max()) {
                        continue;
                    }
                    domain::TraitEffectDefinition definition;
                    definition.text = item["template"].get<std::string>();
                    for (const auto* key : {"valueMap", "paramMap"}) {
                        if (!item.contains(key) || !item[key].is_object()) {
                            continue;
                        }
                        auto& output = key[0] == 'v' ? definition.values : definition.params;
                        for (auto it = item[key].begin(); it != item[key].end(); ++it) {
                            int number = 0;
                            const auto& text = it.key();
                            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
                            if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && it.value().is_string()) {
                                output[number] = it.value().get<std::string>();
                            }
                        }
                    }
                    effect_definitions[static_cast<std::uint16_t>(type)] = std::move(definition);
                }
            }
            state.overlay.bridge.publish_trait_effects(std::move(effect_definitions));
            const std::array candidates{directory / L"speciality_definitions.json",
                directory / L"specialty_definitions.json", directory / L"release" / L"speciality_definitions.json",
                directory / L"release" / L"specialty_definitions.json"};
            std::string specialty_text;
            for (const auto& candidate : candidates) {
                std::ifstream file(candidate, std::ios::binary);
                if (!file) {
                    continue;
                }
                specialty_text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                break;
            }
            if (specialty_text.empty()) {
                const auto module = reinterpret_cast<HMODULE>(plugin_module);
                const auto resource = FindResourceW(module, MAKEINTRESOURCEW(204), RT_RCDATA);
                if (resource) {
                    const auto size = SizeofResource(module, resource);
                    const auto bytes = static_cast<const char*>(LockResource(LoadResource(module, resource)));
                    if (bytes && size) {
                        specialty_text.assign(bytes, size);
                    }
                }
            }
            if (!specialty_text.empty()) {
                const auto json = nlohmann::json::parse(specialty_text, nullptr, false);
                if (json.is_object() && json.contains("items") && json["items"].is_array()) {
                    for (const auto& item : json["items"]) {
                        if (!item.is_object() || !item.contains("id") || !item["id"].is_number_integer() ||
                            !item.contains("name") || !item["name"].is_string()) {
                            continue;
                        }
                        const auto id = item["id"].get<std::int64_t>();
                        if (id >= 1 && id <= 300 && !item["name"].get_ref<const std::string&>().empty()) {
                            named[static_cast<std::size_t>(id)] = true;
                        }
                    }
                }
            }
            state.overlay.bridge.publish_named_specialties(named, std::random_device{}());
            state.config_saved.store(services::run_config_service(state.overlay.bridge, path,
                skill_baseline ? std::move(*skill_baseline) : std::nullopt, stop));
        } catch (...) {
            state.config_saved.store(false, std::memory_order_release);
            state.config_failed.store(true, std::memory_order_release);
            OutputDebugStringW(L"S8RPKCheats: configuration service failed; game execution remains active.\n");
            try {
                state.overlay.bridge.publish_config_notice({0, services::ConfigSaveStatus::Failed,
                    static_cast<std::uint32_t>(ERROR_UNHANDLED_EXCEPTION)});
                state.overlay.bridge.publish_notice("설정 서비스가 중단되어 변경 내용을 저장하지 못했습니다.");
            } catch (...) {
                // Error reporting must never escape the thread entry point.
            }
        }
    });
    const auto started = start_overlay(state.overlay, game_window);
    if (!started) {
        state.config_thread.request_stop();
        state.config_thread.join();
        return std::unexpected(started.error());
    }
    return {};
}

int stop_runtime_entries(RuntimeServices& state) {
    return stop_overlay_entries(state.overlay);
}

std::expected<bool, int> finish_runtime_services(RuntimeServices& state) {
    if (state.config_thread.joinable()) {
        state.config_thread.request_stop();
        state.config_thread.join();
    }
    const int status = finish_overlay_stop(state.overlay);

    if (status != 0) {
        return std::unexpected(status);
    }
    return state.config_saved.load();
}

} // namespace s8::runtime
