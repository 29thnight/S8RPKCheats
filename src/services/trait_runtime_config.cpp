#include "trait_runtime_config.h"
#include "../../third_party/nlohmann_json/single_include/nlohmann/json.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace s8::services {
namespace {

bool integer(const nlohmann::json& object, const char* key, int minimum, int maximum, int& output) {
    const nlohmann::json::const_iterator field = object.find(key);

    if (field == object.end() || !field->is_number_integer()) {
        return false;
    }
    if (field->is_number_unsigned()) {
        const std::uint64_t value = field->get<std::uint64_t>();

        if (value > static_cast<std::uint64_t>(maximum)) {
            return false;
        }
        output = static_cast<int>(value);
        return output >= minimum;
    }

    const std::int64_t value = field->get<std::int64_t>();

    if (value < minimum || value > maximum) {
        return false;
    }
    output = static_cast<int>(value);
    return true;
}

bool optional_integer(const nlohmann::json& object, const char* key, int& output) {
    if (!object.contains(key)) {
        return true;
    }
    return integer(object, key, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), output);
}

bool text_field(const nlohmann::json& object, const char* key, std::string& output) {
    const nlohmann::json::const_iterator field = object.find(key);

    if (field == object.end()) {
        return true;
    }
    if (!field->is_string()) {
        return false;
    }
    output = field->get<std::string>();
    return output.find('\0') == std::string::npos;
}

} // namespace

domain::Result<domain::TraitRuntimeCatalog> read_trait_runtime_config(std::string_view source) {
    const nlohmann::json root = nlohmann::json::parse(source.begin(), source.end(), nullptr, false);
    domain::TraitRuntimeCatalog catalog{};
    std::size_t count = 0;

    if (!root.is_object() || !root.contains("traits") || !root["traits"].is_array()) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    for (const nlohmann::json& row : root["traits"]) {
        int index = 0;
        int id = 0;

        if (!row.is_object() || !integer(row, "index", 0, 253, index) ||
            !integer(row, "id", 1, 254, id) || id != index + 1 ||
            !row.contains("effects") || !row["effects"].is_array() || row["effects"].size() != 6) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }

        domain::TraitRuntimeEntry entry{};
        std::size_t effect_index = 0;

        entry.present = true;
        entry.trait.id = static_cast<std::uint16_t>(id);
        for (const nlohmann::json& effect : row["effects"]) {
            int type = 0;
            int value = 0;
            int parameter = 0;

            if (!effect.is_object() || !integer(effect, "type", 0, 65535, type) ||
                !integer(effect, "value", 0, 65535, value) || !integer(effect, "param", 0, 65535, parameter)) {
                return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
            }
            entry.trait.effects[effect_index++] = {static_cast<std::uint16_t>(type),
                static_cast<std::uint16_t>(value), static_cast<std::uint16_t>(parameter)};
        }
        if (!catalog.traits[static_cast<std::size_t>(index)].present) {
            ++count;
        }
        // The original file accepts repeated indices with last-row precedence.
        catalog.traits[static_cast<std::size_t>(index)] = entry;
    }
    if (count == 0) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    if (!root.contains("customNames")) {
        return catalog;
    }
    if (!root["customNames"].is_object()) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }

    const nlohmann::json& metadata = root["customNames"];

    for (nlohmann::json::const_iterator row = metadata.begin(); row != metadata.end(); ++row) {
        const std::string& key = row.key();
        int index = 0;
        const std::from_chars_result parsed = std::from_chars(key.data(), key.data() + key.size(), index);
        domain::TraitRuntimeMetadata entry{};

        if (parsed.ec != std::errc{} || parsed.ptr != key.data() + key.size() ||
            index < 0 || index >= static_cast<int>(domain::TRAIT_RUNTIME_COUNT) || !row->is_object() ||
            !text_field(*row, "name", entry.name) || !text_field(*row, "desc", entry.description) ||
            !optional_integer(*row, "cloneSrc", entry.clone_source) ||
            !optional_integer(*row, "bgTrait", entry.background_trait) ||
            !optional_integer(*row, "grade", entry.grade)) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        entry.present = true;
        catalog.metadata[static_cast<std::size_t>(index)] = std::move(entry);
    }
    return catalog;
}

} // namespace s8::services
