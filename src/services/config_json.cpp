#include "config_json.h"
#include "../platform/windows/unicode.h"

#include "../../third_party/nlohmann_json/single_include/nlohmann/json.hpp"

#include <string>
#include <charconv>
#include <unordered_set>
#include <map>
#include <limits>
#include <utility>

namespace s8::services {
namespace {

void skip_space(std::string_view source, std::size_t& position) {
    while (position < source.size() && (source[position] == ' ' || source[position] == '\t' ||
           source[position] == '\r' || source[position] == '\n')) {
        ++position;
    }
}

std::size_t string_end(std::string_view source, std::size_t position) {
    ++position;
    while (position < source.size()) {
        if (source[position] == '\\') {
            position += 2;
        } else if (source[position++] == '"') {
            break;
        }
    }
    return position;
}

std::size_t value_end(std::string_view source, std::size_t position) {
    std::size_t depth = 0;
    while (position < source.size()) {
        const char c = source[position];
        if (c == '"') {
            position = string_end(source, position);
        } else if (c == '{' || c == '[') {
            ++depth;
            ++position;
        } else if (c == '}' || c == ']') {
            if (depth == 0) {
                break;
            }
            --depth;
            ++position;
        } else if (c == ',' && depth == 0) {
            break;
        } else {
            ++position;
        }
    }
    while (position > 0 && (source[position - 1] == ' ' || source[position - 1] == '\t' ||
           source[position - 1] == '\r' || source[position - 1] == '\n')) {
        --position;
    }
    return position;
}

std::string encode_value(const ConfigChange& change) {
    if (const auto value = std::get_if<bool>(&change.value)) {
        return *value ? "true" : "false";
    }
    if (const auto value = std::get_if<int>(&change.value)) {
        return std::to_string(*value);
    }
    if (const auto value = std::get_if<float>(&change.value)) {
        return nlohmann::json(*value).dump();
    }
    return nlohmann::json(std::get<std::string>(change.value)).dump();
}

} // namespace

domain::Result<std::string> update_config_json(std::string_view source, std::span<const ConfigChange> changes) {
    if (source.empty()) {
        source = "{}";
    }
    if (!nlohmann::json::accept(source.begin(), source.end())) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    std::size_t position = 0;
    skip_space(source, position);
    if (source[position] != '{') {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    ++position;
    std::size_t copied = 0;
    std::string output;
    std::unordered_set<std::string> keys;
    std::vector<bool> found(changes.size());
    bool has_members = false;
    while (true) {
        skip_space(source, position);
        if (source[position] == '}') {
            break;
        }
        has_members = true;
        const auto key_start = position;
        position = string_end(source, position);
        auto key_json = nlohmann::json::parse(source.begin() + key_start, source.begin() + position, nullptr, false);
        const auto key = key_json.get<std::string>();
        if (!keys.insert(key).second) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        skip_space(source, position);
        ++position; // colon
        skip_space(source, position);
        const auto start = position;
        const auto end = value_end(source, start);
        for (std::size_t i = 0; i < changes.size(); ++i) {
            if (changes[i].key == key) {
                output.append(source.substr(copied, start - copied));
                output.append(encode_value(changes[i]));
                copied = end;
                found[i] = true;
                break;
            }
        }
        position = end;
        skip_space(source, position);
        if (source[position] == ',') {
            ++position;
        }
    }
    output.append(source.substr(copied, position - copied));
    for (std::size_t i = 0; i < changes.size(); ++i) {
        if (!found[i]) {
            if (has_members) {
                output.push_back(',');
            }
            output.append("\n  ");
            output.append(nlohmann::json(changes[i].key).dump());
            output.append(": ");
            output.append(encode_value(changes[i]));
            has_members = true;
        }
    }
    output.append(source.substr(position));
    return output;
}

domain::Result<std::string> config_lines_for_load(std::string_view source, SecondaryJewelConfig* secondary,
                                                  TerrainBonusChanges* terrain_bonus, DefBuildingChanges* def_building,
                                                  ScalarConfigChanges* scalars) {
    const auto validated = update_config_json(source, {});
    if (!validated) {
        return std::unexpected(validated.error());
    }
    const auto object = nlohmann::json::parse(*validated, nullptr, false);
    if (scalars) {
        const auto read_float = [&](const char* key) -> std::optional<float> {
            const auto item = object.find(key);
            if (item == object.end() || !item->is_number()) {
                return std::nullopt;
            }
            const auto text = item->dump();
            float value = 0;
            if (std::from_chars(text.data(), text.data() + text.size(), value).ec != std::errc{}) {
                return std::nullopt;
            }
            return value;
        };
        const auto read_int = [&](const char* key) -> std::optional<int> {
            const auto item = object.find(key);
            if (item == object.end() || !item->is_number()) {
                return std::nullopt;
            }
            const auto text = item->dump();
            int value = 0;
            if (std::from_chars(text.data(), text.data() + text.size(), value).ec != std::errc{}) {
                return std::nullopt;
            }
            return value;
        };
        const auto read_bool = [&](const char* key) -> std::optional<bool> {
            const auto item = object.find(key);
            if (item == object.end() || (!item->is_boolean() && !item->is_number())) {
                return std::nullopt;
            }
            return item->is_boolean() && item->get<bool>();
        };
        scalars->notification_speed = read_float("g_notificationSpeed");
        scalars->domestics_player = read_float("fDomesticsPlayer");
        scalars->domestics_force = read_float("fDomesticsForce");
        scalars->siege_heal_rate = read_int("iSiegeHealRate");
        scalars->skip_days = read_int("nSkipDays");
        scalars->short_battle_cooldown_days = read_int("iShortBattleCooldownDays");
        scalars->short_battle_cooldown_enabled = read_bool("bShortBattleCooldownEnabled");
        scalars->tech_city_visibility = read_bool("bTechCityEditorVisible");
        scalars->reinforcement_arrival = read_bool("bReinforcementArrivalAction");
        scalars->troop_count_combat = read_bool("bTroopCountCombatScaling");
        scalars->stratagem5_enabled = read_bool("bStratagemFiveEnabled").value_or(false);
        constexpr std::array stratagem5_keys{
            "iStratagem5Target", "iStratagem5Effect1", "iStratagem5Power1", "iStratagem5Duration1",
            "iStratagem5Effect2", "iStratagem5Power2", "iStratagem5Duration2", "iStratagem5Range",
            "iStratagem5HealAmount"};
        for (std::size_t i = 0; i < stratagem5_keys.size(); ++i) {
            scalars->stratagem5[i] = read_int(stratagem5_keys[i]);
        }
        scalars->catapult_min_range = read_int("v_Catapult_MinRange");
        scalars->catapult_max_range = read_int("v_Catapult_MaxRange");
        scalars->catapult_amount = read_int("v_Catapult_Amount");
        scalars->catapult_range = read_int("v_Catapult_Range");
        for (std::size_t i = 0; i < 3; ++i) {
            const auto level = std::to_string(i + 1);
            const auto read_level = [&](std::string_view prefix, std::string_view suffix) {
                const auto key = std::string(prefix) + level + std::string(suffix);
                return read_int(key.c_str());
            };
            scalars->heal_amount[i] = read_level("v_HealLv", "_Amount");
            if (i < 2) {
                const auto key = "v_HealLv" + std::to_string(i + 2) + "_Range";
                scalars->heal_range[i] = read_int(key.c_str());
            }
            scalars->dongto_probability[i] = read_level("v_DongtoLv", "_Prob");
            scalars->dongto_status_probability[i] = read_level("v_DongtoLv", "_StateProb");
            scalars->celestial_probability[i] = read_level("v_CelestiaLv", "_Prob");
            scalars->celestial_amount[i] = read_level("v_CelestiaLv", "_Amount");
            scalars->celestial_range[i] = read_level("v_CelestiaLv", "_Range");
            scalars->water_amount[i] = read_level("v_WaterLv", "_Amount");
            scalars->water_range[i] = read_level("v_WaterLv", "_Range");
            scalars->stone_amount[i] = read_level("v_StoneLv", "_Amount");
            scalars->stone_range[i] = read_level("v_StoneLv", "_Range");
        }
        scalars->all_jewels_open = read_bool("bAllJewelsOpen");
    }
    if (secondary) {
        const auto mask = object.find("secondaryJewelForceMask");
        if (mask != object.end()) {
            secondary->mask_present = true;
            if (mask->is_string()) {
                const auto parsed = domain::parse_secondary_jewel_mask(mask->get_ref<const std::string&>());
                if (parsed) {
                    secondary->mask = *parsed;
                } else {
                    secondary->valid = false;
                }
            } else {
                secondary->valid = false;
            }
        } else {
            const auto legacy = object.find("bAllSecondaryJewels");
            if (legacy != object.end() && legacy->is_boolean() && legacy->get<bool>()) {
                secondary->mask = domain::all_secondary_jewels();
            }
        }
    }
    std::string lines;
    if (def_building) {
        for (std::size_t i = 0; i < domain::DEF_BUILDING_KEYS.size(); ++i) {
            const auto item = object.find(std::string(domain::DEF_BUILDING_KEYS[i]));
            if (item != object.end() && item->is_number()) {
                const auto text = item->dump();
                int value = 0;
                if (std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc{}) {
                    (*def_building)[i] = value;
                }
            }
        }
    }
    if (terrain_bonus) {
        for (std::size_t troop = 1; troop <= 12; ++troop) {
            for (std::size_t terrain = 1; terrain <= 17; ++terrain) {
                const auto key = "TB_" + std::to_string(troop) + "_" + std::to_string(terrain);
                const auto item = object.find(key);
                if (item != object.end() && item->is_number()) {
                    const auto text = item->dump();
                    int value = 0;
                    if (std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc{}) {
                        (*terrain_bonus)[troop][terrain] = static_cast<std::int16_t>(value);
                    }
                }
            }
        }
    }
    for (auto item = object.begin(); item != object.end(); ++item) {
        if (!item.value().is_boolean() && !item.value().is_number()) {
            continue;
        }
        lines.append(nlohmann::json(item.key()).dump());
        lines.append(": ");
        lines.append(item.value().dump());
        lines.push_back('\n');
    }
    return lines;
}

domain::Result<SecondaryJewelConfig> read_secondary_jewel_config(std::string_view source) {
    SecondaryJewelConfig result;
    const auto lines = config_lines_for_load(source, &result);
    if (!lines) {
        return std::unexpected(lines.error());
    }
    return result;
}

domain::Result<domain::OfficerNames> read_officer_names(std::string_view source) {
    const auto text = platform::windows::utf8_from_system_code_page(source);
    const auto json = nlohmann::json::parse(text, nullptr, false);
    if (!json.is_array()) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    domain::OfficerNames names;
    for (const auto& entry : json) {
        if (!entry.is_object() || !entry.contains("id") || !entry["id"].is_number_integer() ||
            !entry.contains("name") || !entry["name"].is_string()) {
            continue;
        }
        const auto id = entry["id"].get<std::int64_t>();
        const auto& name = entry["name"].get_ref<const std::string&>();
        if (id < 1 || id > 5102 || name.empty()) {
            continue;
        }
        auto display = platform::windows::normalize_utf8(name);
        if (entry.contains("ja") && entry["ja"].is_string()) {
            const auto& courtesy = entry["ja"].get_ref<const std::string&>();
            if (!courtesy.empty()) {
                display += "(" + platform::windows::normalize_utf8(courtesy) + ")";
            }
        }
        names[static_cast<int>(id)] = std::move(display);
    }
    return names;
}

domain::Result<std::string> update_officer_name(std::string_view source, int id, std::string_view name) {
    const auto text = platform::windows::utf8_from_system_code_page(source);
    auto json = text.empty() ? nlohmann::json::array() : nlohmann::json::parse(text, nullptr, false);
    if (!json.is_array() || id < 1 || id > 5102) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    std::map<int, nlohmann::json> entries;
    for (const auto& entry : json) {
        if (!entry.is_object() || !entry.contains("id") || !entry["id"].is_number_integer()) {
            continue;
        }
        const auto entry_id = entry["id"].get<std::int64_t>();
        if (entry_id < std::numeric_limits<int>::min() || entry_id > std::numeric_limits<int>::max()) {
            continue;
        }
        const auto entry_name = entry.contains("name") && entry["name"].is_string()
            ? entry["name"].get<std::string>() : std::string{};
        const auto courtesy = entry.contains("ja") && entry["ja"].is_string()
            ? entry["ja"].get<std::string>() : std::string{};
        entries[static_cast<int>(entry_id)] = {{"id", entry_id},
            {"name", platform::windows::normalize_utf8(entry_name)},
            {"ja", platform::windows::normalize_utf8(courtesy)}};
    }
    auto found = entries.find(id);
    if (found == entries.end()) {
        entries[id] = {{"id", id}, {"name", std::string(name)}, {"ja", ""}};
    } else {
        found->second["name"] = std::string(name);
    }
    json = nlohmann::json::array();
    for (auto& [entry_id, entry] : entries) {
        json.push_back(std::move(entry));
    }
    return json.dump(2) + "\n";
}

} // namespace s8::services
