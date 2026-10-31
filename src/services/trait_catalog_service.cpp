#include "trait_catalog_service.h"
#include "trait_runtime_config.h"
#include "../platform/windows/config_commit.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <string>
#include <utility>

namespace s8::services {
namespace {

bool regular_file(const std::filesystem::path& path) {
    std::error_code error;

    return !path.empty() && std::filesystem::is_regular_file(path, error) && !error;
}

const std::filesystem::path* first_file(const std::array<std::filesystem::path, 2>& candidates) {
    for (const std::filesystem::path& candidate : candidates) {
        if (regular_file(candidate)) {
            return &candidate;
        }
    }
    return nullptr;
}

} // namespace

TraitCatalogService::TraitCatalogService(const std::filesystem::path& directory) {
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));

    custom_paths = {directory / L"san8r_traits_config.json", current / L"san8r_traits_config.json"};
    default_paths = {directory / L"S8RPK_traits_default.json", current / L"S8RPK_traits_default.json"};
    if (length && length < executable.size()) {
        external_runtime_path = std::filesystem::path(executable.data()).parent_path() / L"version.dll";
    }
}

void TraitCatalogService::poll(UiBridge& bridge, TraitTextService& texts, bool force) {
    const bool external = regular_file(external_runtime_path);
    const std::filesystem::path* source = first_file(custom_paths);

    bridge.external_trait_runtime.store(external, std::memory_order_release);
    if (!source && !external) {
        source = first_file(default_paths);
    }

    std::optional<std::filesystem::file_time_type> write_time;
    std::uintmax_t size = 0;

    if (source) {
        std::error_code error;
        const std::filesystem::file_time_type time = std::filesystem::last_write_time(*source, error);

        if (!error) {
            write_time = time;
        }
        error.clear();
        size = std::filesystem::file_size(*source, error);
        if (error) {
            size = 0;
        }
    }
    const bool same_source = source ? *source == observed_source : observed_source.empty();

    if (!force && observed && external == observed_external && same_source &&
        write_time == observed_write_time && size == observed_size) {
        return;
    }
    observed = true;
    observed_external = external;
    observed_source = source ? *source : std::filesystem::path{};
    observed_write_time = write_time;
    observed_size = size;

    if (!source) {
        texts.clear_embedded_source();
        bridge.publish_trait_catalog(own::make_shared<domain::TraitRuntimeCatalog>());
        bridge.publish_custom_traits({});
        return;
    }

    domain::Result<std::optional<std::string>> contents = platform::windows::read_config_for_save(*source);

    if (!contents || !*contents) {
        bridge.publish_notice("기재 설정 파일을 읽지 못해 이전 설정을 유지합니다.");
        return;
    }
    domain::Result<domain::TraitRuntimeCatalog> catalog = read_trait_runtime_config(**contents);

    if (!catalog) {
        bridge.publish_notice("기재 설정 형식이 올바르지 않아 이전 설정을 유지합니다.");
        return;
    }
    domain::CustomTraits names;

    for (std::size_t index = 0; index < catalog->metadata.size(); ++index) {
        const domain::TraitRuntimeMetadata& metadata = catalog->metadata[index];

        if (metadata.present && !metadata.name.empty()) {
            names[static_cast<std::uint16_t>(index + 1)] = {metadata.name, metadata.grade >= 0 ? metadata.grade : 0};
        }
    }
    texts.set_embedded_source(*source, std::move(**contents));
    bridge.publish_trait_catalog(own::make_shared<domain::TraitRuntimeCatalog>(std::move(*catalog)));
    bridge.publish_custom_traits(std::move(names));
}

} // namespace s8::services
