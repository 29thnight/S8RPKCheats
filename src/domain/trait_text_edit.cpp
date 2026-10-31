#include "trait_text_edit.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace s8::domain {
namespace {

struct DefaultText {
    std::string_view name;
    std::string_view description;
};

#include "trait_text_defaults.inc"

int utf16_units(std::string_view text) {
    int units = 0;

    for (std::size_t offset = 0; offset < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[offset]);
        std::size_t length = 0;
        std::uint32_t value = 0;

        if (first > 0 && first < 0x80) {
            length = 1;
            value = first;
        } else if (first >= 0xc2 && first <= 0xdf) {
            length = 2;
            value = first & 0x1f;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3;
            value = first & 0x0f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4;
            value = first & 7;
        } else {
            return -1;
        }
        if (length > text.size() - offset) {
            return -1;
        }
        for (std::size_t part = 1; part < length; ++part) {
            const unsigned char next = static_cast<unsigned char>(text[offset + part]);

            if ((next & 0xc0) != 0x80) {
                return -1;
            }
            value = (value << 6) | (next & 0x3f);
        }
        if ((length == 3 && value < 0x800) || (length == 4 && value < 0x10000) ||
            (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) {
            return -1;
        }
        units += value > 0xffff ? 2 : 1;
        if (units > 512) {
            return units;
        }
        offset += length;
    }
    return units;
}

std::string_view next_format_token(std::string_view text, std::size_t& position, bool& valid) {
    constexpr std::string_view FLAGS = "-+ #0";
    constexpr std::string_view CONVERSIONS = "diuoxXfFeEgGaAcsp";

    while (position < text.size()) {
        const std::size_t start = text.find('%', position);

        if (start == std::string_view::npos) {
            position = text.size();
            return {};
        }
        position = start + 1;
        if (position < text.size() && text[position] == '%') {
            ++position;
            continue;
        }

        std::size_t end = position;

        while (end < text.size() && FLAGS.find(text[end]) != std::string_view::npos) {
            ++end;
        }
        while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
            ++end;
        }
        if (end < text.size() && text[end] == '.') {
            ++end;
            while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
                ++end;
            }
        }
        if (end < text.size() && CONVERSIONS.find(text[end]) != std::string_view::npos) {
            position = end + 1;
            return text.substr(start, end - start + 1);
        }
        if (end < text.size()) {
            const char character = text[end];

            // Reject unsupported argument consumers/writers, positional syntax,
            // dynamic widths, and length modifiers instead of treating them as
            // a description with no tokens. Natural language percent signs are
            // escaped before the description reaches the native formatter.
            if ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                character == '*' || character == '$') {
                valid = false;
                return {};
            }
        }
    }
    return {};
}

bool matching_format_tokens(std::string_view original, std::string_view replacement) {
    std::size_t before = 0;
    std::size_t after = 0;
    bool valid = true;
    std::string_view new_token = next_format_token(replacement, after, valid);

    if (!valid) {
        return false;
    }

    // The original editor explicitly permits omission of all value tokens.
    if (new_token.empty()) {
        return true;
    }
    while (!new_token.empty()) {
        if (new_token != next_format_token(original, before, valid) || !valid) {
            return false;
        }
        new_token = next_format_token(replacement, after, valid);
        if (!valid) {
            return false;
        }
    }
    return next_format_token(original, before, valid).empty() && valid;
}

} // namespace

std::string prepare_legacy_trait_description(std::string_view description) {
    std::string result;
    std::size_t copied = 0;
    std::size_t position = 0;
    bool valid = true;

    result.reserve(description.size() * 2);
    for (;;) {
        const std::string_view token = next_format_token(description, position, valid);
        const std::size_t end = token.empty() ? description.size()
            : static_cast<std::size_t>(token.data() - description.data());

        while (copied < end) {
            const char character = description[copied++];

            result.push_back(character);
            if (character == '%') {
                result.push_back('%');
                if (copied < end && description[copied] == '%') {
                    ++copied;
                }
            }
        }
        if (token.empty()) {
            break;
        }
        result.append(token);
        copied = position;
    }
    return result;
}

TraitTextEdits make_legacy_trait_text_rows() {
    TraitTextEdits rows{};

    rows.reserve(DEFAULT_TRAIT_TEXTS.size());
    for (const DefaultText& source : DEFAULT_TRAIT_TEXTS) {
        rows.push_back({0, false, std::string(source.name), std::string(source.description), {}, {}});
    }
    return rows;
}

TraitTextEdits make_embedded_trait_text_rows(const TraitRuntimeCatalog& catalog) {
    TraitTextEdits rows{};

    rows.reserve(TRAIT_RUNTIME_COUNT);
    for (std::size_t index = 0; index < catalog.metadata.size(); ++index) {
        const TraitRuntimeMetadata& metadata = catalog.metadata[index];

        if (metadata.present && !metadata.name.empty()) {
            rows.push_back({static_cast<std::uint16_t>(index + 1), true, metadata.name, metadata.description, {}, {}});
        }
    }
    return rows;
}

TraitTextValidation validate_trait_text_edit(const TraitTextEdit& row) {
    const int name_length = utf16_units(row.name);
    const int description_length = utf16_units(row.description);
    const int original_name_length = utf16_units(row.original_name);
    const int original_description_length = utf16_units(row.original_description);

    if (row.embedded && (row.trait_id == 0 || row.trait_id > TRAIT_RUNTIME_COUNT)) {
        return TraitTextValidation::InvalidId;
    }
    if (name_length < 0 || description_length < 0 || original_name_length < 0 || original_description_length < 0) {
        return TraitTextValidation::InvalidUtf8;
    }
    if (name_length > 5) {
        return TraitTextValidation::NameTooLong;
    }
    if (description_length > 512 || original_description_length > 512 || original_name_length > 512) {
        return TraitTextValidation::DescriptionTooLong;
    }
    if (!row.description.empty() && !matching_format_tokens(row.original_description, row.description)) {
        return TraitTextValidation::FormatMismatch;
    }
    return TraitTextValidation::Valid;
}

Result<TraitRuntimeCatalog> apply_embedded_trait_text_edits(const TraitRuntimeCatalog& catalog,
    std::span<const TraitTextEdit> rows) {
    TraitRuntimeCatalog updated = catalog;

    for (const TraitTextEdit& row : rows) {
        if (!row.embedded || validate_trait_text_edit(row) != TraitTextValidation::Valid ||
            !updated.metadata[row.trait_id - 1].present) {
            return std::unexpected(Error{ErrorCode::InvalidData});
        }

        TraitRuntimeMetadata& metadata = updated.metadata[row.trait_id - 1];

        if (!row.name.empty()) {
            metadata.name = row.name;
        }
        if (!row.description.empty()) {
            metadata.description = row.description;
        }
    }
    return updated;
}

} // namespace s8::domain
