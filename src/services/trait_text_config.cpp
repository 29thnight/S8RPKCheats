#include "trait_text_config.h"
#include "trait_runtime_config.h"
#include "../../third_party/nlohmann_json/single_include/nlohmann/json.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace s8::services {
namespace {

int hex_digit(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

bool decode_hex(std::string_view text, std::string& output) {
    if (text.size() % 2 != 0) {
        return false;
    }
    output.clear();
    output.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        const int high = hex_digit(text[index]);
        const int low = hex_digit(text[index + 1]);

        if (high < 0 || low < 0) {
            return false;
        }
        output.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

void append_hex(std::string& output, std::string_view input) {
    constexpr std::string_view DIGITS = "0123456789ABCDEF";

    for (const unsigned char value : input) {
        output.push_back(DIGITS[value >> 4]);
        output.push_back(DIGITS[value & 0xf]);
    }
}

void skip_space(std::string_view source, std::size_t& position) {
    while (position < source.size() &&
        (source[position] == ' ' || source[position] == '\r' || source[position] == '\n' || source[position] == '\t')) {
        ++position;
    }
}

std::size_t string_end(std::string_view source, std::size_t begin) {
    for (std::size_t position = begin + 1; position < source.size(); ++position) {
        if (source[position] == '\\') {
            ++position;
        } else if (source[position] == '"') {
            return position + 1;
        }
    }
    return source.size();
}

std::size_t value_end(std::string_view source, std::size_t begin) {
    int depth = 0;
    std::size_t position = begin;

    while (position < source.size()) {
        const char value = source[position];

        if (value == '"') {
            position = string_end(source, position);
            continue;
        }
        if (value == '[' || value == '{') {
            ++depth;
        } else if (value == ']' || value == '}') {
            if (depth == 0) {
                break;
            }
            --depth;
            if (depth == 0) {
                return position + 1;
            }
        } else if (value == ',' && depth == 0) {
            break;
        }
        ++position;
        if (depth == 0 && source[begin] == '"') {
            break;
        }
    }
    while (position > begin && (source[position - 1] == ' ' || source[position - 1] == '\r' ||
            source[position - 1] == '\n' || source[position - 1] == '\t')) {
        --position;
    }
    return position;
}

struct JsonMember {
    std::string key;
    std::size_t begin;
    std::size_t end;
};

std::vector<JsonMember> members(std::string_view source, std::size_t begin) {
    std::vector<JsonMember> output{};
    std::size_t position = begin + 1;

    while (position < source.size()) {
        skip_space(source, position);
        if (position >= source.size() || source[position] == '}') {
            break;
        }

        const std::size_t end = string_end(source, position);
        const nlohmann::json key = nlohmann::json::parse(source.begin() + position, source.begin() + end, nullptr, false);

        position = end;
        skip_space(source, position);
        ++position;
        skip_space(source, position);

        const std::size_t value_begin = position;
        const std::size_t value_stop = value_end(source, position);

        output.push_back({key.get<std::string>(), value_begin, value_stop});
        position = value_stop;
        skip_space(source, position);
        if (position >= source.size() || source[position] != ',') {
            break;
        }
        ++position;
    }
    return output;
}

struct Replacement {
    std::size_t begin;
    std::size_t end;
    std::string value;
};

} // namespace

domain::Result<domain::TraitTextEdits> read_legacy_trait_text_config(std::string_view source) {
    domain::TraitTextEdits rows = domain::make_legacy_trait_text_rows();
    std::size_t position = 0;

    while (position < source.size()) {
        const std::size_t newline = source.find('\n', position);
        std::string_view line = source.substr(position,
            newline == std::string_view::npos ? source.size() - position : newline - position);

        position = newline == std::string_view::npos ? source.size() : newline + 1;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (!line.starts_with("ROW:")) {
            continue;
        }

        const std::size_t first = line.find(':', 4);
        const std::size_t second = first == std::string_view::npos ? std::string_view::npos : line.find(':', first + 1);
        std::size_t row = 0;

        if (first == std::string_view::npos || second == std::string_view::npos) {
            continue;
        }

        const std::from_chars_result parsed = std::from_chars(line.data() + 4, line.data() + first, row);
        std::string name;
        std::string description;

        if (parsed.ec != std::errc{} || parsed.ptr != line.data() + first || row == 0 || row > rows.size() ||
            !decode_hex(line.substr(first + 1, second - first - 1), name) || !decode_hex(line.substr(second + 1), description)) {
            continue;
        }
        rows[row - 1].name = std::move(name);
        rows[row - 1].description = std::move(description);
    }
    for (const domain::TraitTextEdit& row : rows) {
        if (domain::validate_trait_text_edit(row) != domain::TraitTextValidation::Valid) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
    }
    return rows;
}

domain::Result<std::string> write_legacy_trait_text_config(std::span<const domain::TraitTextEdit> rows) {
    std::string output = "[SAN8RPK_TRAIT_TEXTS]\r\n";

    if (rows.size() != 72) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const domain::TraitTextEdit& row = rows[index];

        if (row.embedded || domain::validate_trait_text_edit(row) != domain::TraitTextValidation::Valid) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        output.append("ROW:").append(std::to_string(index + 1)).push_back(':');
        append_hex(output, row.name);
        output.push_back(':');
        append_hex(output, row.description);
        output.append("\r\n");
    }
    return output;
}

domain::Result<std::string> update_embedded_trait_text_config(std::string_view source,
    std::span<const domain::TraitTextEdit> rows) {
    const nlohmann::json root = nlohmann::json::parse(source.begin(), source.end(), nullptr, false);
    std::vector<Replacement> changes{};
    std::size_t metadata_begin = std::string_view::npos;

    if (!read_trait_runtime_config(source) || !root.contains("customNames") || !root["customNames"].is_object()) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }
    nlohmann::json traits = root["traits"];

    for (const domain::TraitTextEdit& row : rows) {
        if (!row.embedded || domain::validate_trait_text_edit(row) != domain::TraitTextValidation::Valid) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        if (row.effects) {
            for (nlohmann::json& trait : traits) {
                if (trait["index"].get<int>() != row.trait_id - 1) {
                    continue;
                }
                for (std::size_t index = 0; index < row.effects->size(); ++index) {
                    const domain::TraitEffect& effect = (*row.effects)[index];

                    trait["effects"][index]["type"] = effect.effect_id;
                    trait["effects"][index]["value"] = effect.value1;
                    trait["effects"][index]["param"] = effect.value2;
                }
            }
        }
    }
    for (const JsonMember& member : members(source, source.find('{'))) {
        if (member.key == "traits") {
            changes.push_back({member.begin, member.end, traits.dump(2)});
        }
        if (member.key == "customNames") {
            metadata_begin = member.begin;
        }
    }
    if (metadata_begin == std::string_view::npos) {
        return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
    }

    nlohmann::json metadata = root["customNames"];

    for (const domain::TraitTextEdit& row : rows) {
        if (row.name.empty() && row.description.empty()) {
            continue;
        }

        nlohmann::json& entry = metadata[std::to_string(row.trait_id - 1)];

        if (!row.name.empty()) {
            entry["name"] = row.name;
        }
        if (!row.description.empty()) {
            entry["desc"] = row.description;
        }
    }
    changes.push_back({metadata_begin, value_end(source, metadata_begin), metadata.dump(2)});
    std::sort(changes.begin(), changes.end(), [](const Replacement& left, const Replacement& right) {
        return left.begin > right.begin;
    });

    std::string output(source);
    std::size_t previous = source.size();

    for (const Replacement& change : changes) {
        if (change.end > previous) {
            return std::unexpected(domain::Error{domain::ErrorCode::InvalidData});
        }
        output.replace(change.begin, change.end - change.begin, change.value);
        previous = change.begin;
    }
    return output;
}

} // namespace s8::services
