#include "game_font.h"
#include "ui_bridge.h"
#include "../domain/native_ui_layout.h"

// Resource layouts and LZ4 decoding adapted from SAN8R Resource Viewer 0.2.1.
// See third_party/san8r_resource_viewer/LICENSE.txt.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <unordered_map>
#include <charconv>
#include <atomic>

namespace s8::services {
namespace {

using Bytes = std::vector<unsigned char>;
constexpr std::size_t MAX_ASSET = 256 * 1024 * 1024;
constexpr std::uint32_t FONT_ID = 0x6fa07aa6;
std::atomic<unsigned> requested_face{~0u};
std::optional<unsigned> processed_face;
std::unordered_map<unsigned, std::uint32_t> portrait_ids;
std::filesystem::path game_directory;
own::shared_owner<const GamePortrait> current_portrait;
std::mutex font_mutex;
own::shared_owner<const GameFont> current_font;

// All offsets passed here are inside an envelope checked by the file parser.
template<class T>
T scalar(std::span<const unsigned char> bytes, std::size_t offset) {
    T value;

    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::optional<Bytes> read_file(const std::filesystem::path& path,
    std::uint64_t offset = 0, std::size_t count = 0) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        return std::nullopt;
    }

    const std::streamoff length = file.tellg();

    if (length < 0 || offset > static_cast<std::uint64_t>(length)) {
        return std::nullopt;
    }
    const std::uint64_t available = static_cast<std::uint64_t>(length) - offset;
    const std::uint64_t requested = count ? count : available;

    if (!requested || requested > MAX_ASSET || requested > available) {
        return std::nullopt;
    }

    Bytes bytes(static_cast<std::size_t>(requested));

    file.seekg(static_cast<std::streamoff>(offset));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return std::nullopt;
    }
    return bytes;
}

bool decode_lz4(std::span<const unsigned char> source, std::span<unsigned char> target) {
    std::size_t input = 0;
    std::size_t output = 0;
    const auto length = [&](std::size_t initial) -> std::optional<std::size_t> {
        if (initial == 15) {
            unsigned char extra = 255;

            while (extra == 255) {
                if (input == source.size()) {
                    return std::nullopt;
                }
                extra = source[input++];
                initial += extra;
                if (initial > target.size()) {
                    return std::nullopt;
                }
            }
        }
        return initial;
    };

    while (input < source.size()) {
        const unsigned char token = source[input++];
        const std::optional<std::size_t> literals = length(token >> 4);

        if (!literals || *literals > source.size() - input || *literals > target.size() - output) {
            return false;
        }
        std::memcpy(target.data() + output, source.data() + input, *literals);
        input += *literals;
        output += *literals;
        if (input == source.size()) {
            break;
        }
        if (source.size() - input < 2) {
            return false;
        }

        const std::uint16_t distance = scalar<std::uint16_t>(source, input);

        input += 2;
        const std::optional<std::size_t> match = length(token & 15);

        if (!distance || distance > output || !match || *match + 4 > target.size() - output) {
            return false;
        }
        for (std::size_t i = 0; i < *match + 4; ++i) {
            target[output] = target[output - distance];
            ++output;
        }
    }
    return output == target.size();
}

std::expected<Bytes, std::string> read_game_asset(const std::filesystem::path& directory, std::uint32_t resource_id, std::uint32_t type_id) {
    constexpr std::array INDEX_DIRS{L"", L"0002", L"data", L"Data"};
    std::optional<Bytes> index;
    std::optional<Bytes> locations;
    std::filesystem::path index_directory;

    for (const wchar_t* subdirectory : INDEX_DIRS) {
        index_directory = directory / subdirectory;
        index = read_file(index_directory / L"root.rdb");
        if (index) {
            locations = read_file(index_directory / L"root.rdx");
            break;
        }
    }
    if (!index || !locations || index->size() < 32 || locations->size() % 8 != 0 ||
        std::memcmp(index->data(), "_DRK0000", 8) != 0) {
        return std::unexpected("Game resource: root.rdb/root.rdx unavailable or unsupported.");
    }

    const std::uint32_t count = scalar<std::uint32_t>(*index, 16);
    std::size_t cursor = scalar<std::uint32_t>(*index, 8);

    if (cursor < 32 || cursor > index->size() || count > (index->size() - cursor) / 48) {
        return std::unexpected("Game resource: invalid resource index header.");
    }
    for (std::uint32_t entry = 0; entry < count; ++entry) {
        if (index->size() - cursor < 48 || std::memcmp(index->data() + cursor, "IDRK0000", 8) != 0) {
            return std::unexpected("Game resource: invalid resource index record.");
        }

        const std::uint64_t entry_size = scalar<std::uint64_t>(*index, cursor + 8);
        const std::uint64_t location_size = scalar<std::uint64_t>(*index, cursor + 16);

        if (entry_size < 48 || entry_size > index->size() - cursor || location_size > entry_size - 48) {
            return std::unexpected("Game resource: resource index record exceeds file.");
        }
        if (scalar<std::uint32_t>(*index, cursor + 36) == resource_id) {
            const std::uint64_t decoded_size = scalar<std::uint64_t>(*index, cursor + 24);
            const std::size_t location = cursor + static_cast<std::size_t>(entry_size - location_size);

            if (location_size != 13 || (*index)[location] != 1 || (*index)[location + 1] != 4 ||
                !decoded_size || decoded_size > MAX_ASSET ||
                scalar<std::uint32_t>(*index, cursor + 40) != type_id) {
                return std::unexpected("Game resource: unsupported resource location or type.");
            }

            const std::uint32_t offset = scalar<std::uint32_t>(*index, location + 2);
            const std::uint32_t stored_size = scalar<std::uint32_t>(*index, location + 6);
            const std::uint32_t identifier = (*index)[location + 10] |
                (static_cast<std::uint32_t>((*index)[location + 11]) << 8) |
                (static_cast<std::uint32_t>((*index)[location + 12]) << 16);
            std::optional<std::uint32_t> file_hash;

            for (std::size_t position = 0; position < locations->size(); position += 8) {
                if (scalar<std::uint16_t>(*locations, position) == identifier) {
                    file_hash = scalar<std::uint32_t>(*locations, position + 4);
                    break;
                }
            }
            if (!file_hash || stored_size < 48 || stored_size > MAX_ASSET) {
                return std::unexpected("Game resource: missing FDATA location.");
            }

            std::array<wchar_t, 24> filename{};

            swprintf_s(filename.data(), filename.size(), L"0x%08x.fdata", *file_hash);
            const std::array candidates{index_directory, directory, directory / L"fdata",
                directory / L"fdata_package", index_directory / L"fdata", index_directory / L"fdata_package"};
            std::optional<Bytes> record;

            for (const std::filesystem::path& candidate : candidates) {
                record = read_file(candidate / filename.data(), offset, stored_size);
                if (record) {
                    break;
                }
            }
            if (!record || std::memcmp(record->data(), "IDRK0000", 8) != 0 ||
                scalar<std::uint64_t>(*record, 8) != stored_size ||
                scalar<std::uint64_t>(*record, 24) != decoded_size ||
                scalar<std::uint32_t>(*record, 36) != resource_id ||
                scalar<std::uint32_t>(*record, 40) != type_id) {
                return std::unexpected("Game resource: FDATA record does not match index.");
            }

            const std::uint64_t compressed_size = scalar<std::uint64_t>(*record, 16);

            if (!compressed_size || compressed_size > record->size() - 48) {
                return std::unexpected("Game resource: invalid FDATA payload size.");
            }
            const std::span<const unsigned char> payload = std::span(*record).last(static_cast<std::size_t>(compressed_size));

            if (compressed_size == decoded_size) {
                return Bytes(payload.begin(), payload.end());
            }

            Bytes decoded(static_cast<std::size_t>(decoded_size));
            std::size_t input = 0;
            std::size_t output = 0;

            while (output < decoded.size()) {
                if (payload.size() - input < 10) {
                    return std::unexpected("Game resource: truncated FDATA chunk header.");
                }

                const std::uint16_t block_size = scalar<std::uint16_t>(payload, input);
                const std::size_t target_size = std::min(std::size_t{16384}, decoded.size() - output);

                input += 10;
                if (!block_size || block_size > payload.size() - input ||
                    !decode_lz4(payload.subspan(input, block_size), std::span(decoded).subspan(output, target_size))) {
                    return std::unexpected("Game resource: unsupported or corrupt FDATA LZ4 chunk.");
                }
                input += block_size;
                output += target_size;
            }
            if ((payload.size() - input != 0 && payload.size() - input != 2) ||
                std::any_of(payload.begin() + input, payload.end(), [](unsigned char byte) { return byte != 0; })) {
                return std::unexpected("Game resource: unexpected FDATA trailer.");
            }
            return decoded;
        }

        cursor = (cursor + static_cast<std::size_t>(entry_size) + 3) & ~std::size_t{3};
        if (cursor > index->size()) {
            return std::unexpected("Game resource: invalid resource index alignment.");
        }
    }
    return std::unexpected("Game resource: requested resource ID was not found.");
}

std::expected<RangeImage, std::string> decode_texture(std::span<const unsigned char> bytes, unsigned image_index) {
    if (bytes.size() < 28 || std::memcmp(bytes.data(), "GT1G", 4) != 0 ||
        scalar<std::uint32_t>(bytes, 8) != bytes.size()) {
        return std::unexpected("Game UI: invalid G1T header.");
    }

    const std::uint32_t table = scalar<std::uint32_t>(bytes, 12);
    const std::uint32_t count = scalar<std::uint32_t>(bytes, 16);

    if (table < 28 || table > bytes.size() || count > (bytes.size() - table) / 4 || image_index >= count) {
        return std::unexpected("Game UI: invalid G1T image table.");
    }

    const std::uint64_t start = static_cast<std::uint64_t>(table) + scalar<std::uint32_t>(bytes, table + image_index * 4);
    const std::uint64_t end = image_index + 1 < count ?
        static_cast<std::uint64_t>(table) + scalar<std::uint32_t>(bytes, table + (image_index + 1) * 4) : bytes.size();

    if (start < table + count * 4 || start > end || end > bytes.size() || end - start < 8) {
        return std::unexpected("Game UI: G1T image exceeds container.");
    }

    const unsigned format = bytes[start + 1];
    RangeImage image;

    image.width = 1u << (bytes[start + 2] & 15);
    image.height = 1u << (bytes[start + 2] >> 4);
    std::size_t pixels = static_cast<std::size_t>(start) + 8;

    if (bytes[start] >> 4 != 1 || (format != 8 && format != 0x5b) || bytes[start + 3] != 0 ||
        static_cast<std::uint64_t>(image.width) * image.height * 4 > MAX_ASSET) {
        return std::unexpected("Game UI: expected a single-mip BC3 texture.");
    }
    if (scalar<std::uint32_t>(bytes, start + 4) >> 24 == 0x10) {
        if (end - pixels < 12 || scalar<std::uint32_t>(bytes, pixels) != 12) {
            return std::unexpected("Game UI: unsupported extended G1T image header.");
        }
        pixels += 12;
    }

    const unsigned blocks_x = (image.width + 3) / 4;
    const unsigned blocks_y = (image.height + 3) / 4;

    if (static_cast<std::uint64_t>(blocks_x) * blocks_y * 16 != end - pixels) {
        return std::unexpected("Game UI: BC3 byte count does not match dimensions.");
    }
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    for (unsigned block_y = 0; block_y < blocks_y; ++block_y) {
        for (unsigned block_x = 0; block_x < blocks_x; ++block_x) {
            const std::size_t block = pixels + (static_cast<std::size_t>(block_y) * blocks_x + block_x) * 16;
            std::array<unsigned, 8> alpha{};

            alpha[0] = bytes[block];
            alpha[1] = bytes[block + 1];
            if (alpha[0] > alpha[1]) {
                for (unsigned i = 2; i < 8; ++i) {
                    alpha[i] = ((8 - i) * alpha[0] + (i - 1) * alpha[1]) / 7;
                }
            } else {
                for (unsigned i = 2; i < 6; ++i) {
                    alpha[i] = ((6 - i) * alpha[0] + (i - 1) * alpha[1]) / 5;
                }
                alpha[6] = 0;
                alpha[7] = 255;
            }

            std::uint64_t alpha_indices = 0;
            std::array<std::array<unsigned, 3>, 4> colors{};

            for (unsigned i = 0; i < 6; ++i) {
                alpha_indices |= static_cast<std::uint64_t>(bytes[block + 2 + i]) << (i * 8);
            }
            for (unsigned i = 0; i < 2; ++i) {
                const unsigned color = scalar<std::uint16_t>(bytes, block + 8 + i * 2);
                const unsigned red = color >> 11;
                const unsigned green = (color >> 5) & 63;
                const unsigned blue = color & 31;

                colors[i] = {(red << 3) | (red >> 2), (green << 2) | (green >> 4), (blue << 3) | (blue >> 2)};
            }
            for (unsigned channel = 0; channel < 3; ++channel) {
                colors[2][channel] = (2 * colors[0][channel] + colors[1][channel]) / 3;
                colors[3][channel] = (colors[0][channel] + 2 * colors[1][channel]) / 3;
            }

            const std::uint32_t color_indices = scalar<std::uint32_t>(bytes, block + 12);

            for (unsigned i = 0; i < 16; ++i) {
                const unsigned x = block_x * 4 + i % 4;
                const unsigned y = block_y * 4 + i / 4;

                if (x >= image.width || y >= image.height) {
                    continue;
                }
                const std::size_t destination = (static_cast<std::size_t>(y) * image.width + x) * 4;
                const std::array<unsigned, 3>& color = colors[(color_indices >> (i * 2)) & 3];

                for (unsigned channel = 0; channel < 3; ++channel) {
                    image.pixels[destination + channel] = static_cast<std::byte>(color[channel]);
                }
                image.pixels[destination + 3] = static_cast<std::byte>(alpha[(alpha_indices >> (i * 3)) & 7]);
            }
        }
    }
    return image;
}

std::expected<GameFont, std::string> decode_font(std::span<const unsigned char> bytes) {
    if (bytes.size() < 32 || std::memcmp(bytes.data(), "_N1G0000", 8) != 0 ||
        scalar<std::uint32_t>(bytes, 8) != bytes.size()) {
        return std::unexpected("Game font: unsupported G1N header.");
    }

    const std::uint32_t header = scalar<std::uint32_t>(bytes, 12);
    const std::uint32_t pixels = scalar<std::uint32_t>(bytes, 20);
    const std::uint32_t palettes = scalar<std::uint32_t>(bytes, 24);
    const std::uint32_t tables = scalar<std::uint32_t>(bytes, 28);

    if (!tables || tables > 64 || palettes > 256 || header != 32 + tables * 4 + palettes * 64 ||
        header > pixels || pixels > bytes.size()) {
        return std::unexpected("Game font: unsupported G1N table layout.");
    }
    // The first table is the game's 64px font. The second holds the same
    // characters at 128px and is not needed for the editor's text sizes.
    const std::uint32_t table = scalar<std::uint32_t>(bytes, 32);
    const std::uint32_t end = tables > 1 ? scalar<std::uint32_t>(bytes, 36) : pixels;

    if (table != header || end > pixels || end < table || end - table < 131072 + 12 ||
        (end - table - 131072) % 12 != 0) {
        return std::unexpected("Game font: invalid G1N character table.");
    }

    const std::size_t glyph_count = (end - table - 131072) / 12;

    if (glyph_count > 65536) {
        return std::unexpected("Game font: invalid G1N glyph count.");
    }
    GameFont font;
    std::vector<std::size_t> pixel_offsets(glyph_count);

    // Validate and decode each bitmap at the parser boundary. Four samples
    // become one atlas pixel, retaining all characters at a 32px cell height.
    for (std::size_t index = 0; index < glyph_count; ++index) {
        const std::size_t metric = table + 131072 + index * 12;
        const unsigned width = bytes[metric];
        const unsigned height = bytes[metric + 1];
        const unsigned stride = palettes ? (width + 1) / 2 * 2 : width;
        const std::uint32_t offset = scalar<std::uint32_t>(bytes, metric + 8);
        const std::size_t stored = stride * height / (palettes ? 2 : 1);

        if (!width || height != 64 || offset > bytes.size() - pixels || stored > bytes.size() - pixels - offset) {
            return std::unexpected("Game font: unsupported G1N glyph metrics or pixels.");
        }
        pixel_offsets[index] = font.pixels.size();
        const auto coverage = [&](unsigned x, unsigned y) -> unsigned {
            const std::size_t position = static_cast<std::size_t>(y) * stride + x;
            const unsigned char sample = bytes[pixels + offset + position / (palettes ? 2 : 1)];

            return palettes ? ((position & 1) ? (sample & 15) : (sample >> 4)) * 17 : sample;
        };

        for (unsigned y = 0; y < height; y += 2) {
            for (unsigned x = 0; x < width; x += 2) {
                const unsigned next_x = std::min(x + 1, width - 1);
                const unsigned value = coverage(x, y) + coverage(next_x, y) +
                    coverage(x, y + 1) + coverage(next_x, y + 1);

                font.pixels.push_back(static_cast<unsigned char>((value + 2) / 4));
            }
        }
    }
    for (std::uint32_t codepoint = 0; codepoint < 65536; ++codepoint) {
        const std::uint16_t ordinal = scalar<std::uint16_t>(bytes, table + codepoint * 2);

        if (ordinal >= glyph_count) {
            return std::unexpected("Game font: G1N character mapping exceeds glyph table.");
        }
        // Zero is the absent-character fallback, not a Unicode glyph. Leave
        // control characters and UTF-16 surrogate values to the base font.
        if (!ordinal || codepoint < 0x20 || codepoint == 0x7f || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            continue;
        }

        const std::size_t metric = table + 131072 + ordinal * 12;
        const unsigned width = bytes[metric];
        const float bearing_x = static_cast<float>(scalar<std::int8_t>(bytes, metric + 2));
        const float bearing_y = static_cast<float>(scalar<std::int8_t>(bytes, metric + 3));

        font.glyphs.push_back({codepoint, static_cast<std::uint16_t>((width + 1) / 2), 32,
            static_cast<float>(width), 64.0f, static_cast<float>(bytes[metric + 4]),
            bearing_x, 64.0f - bearing_y, pixel_offsets[ordinal]});
    }
    if (font.glyphs.empty()) {
        return std::unexpected("Game font: G1N contains no mapped characters.");
    }
    return font;
}

} // namespace

void prepare_game_font(const std::filesystem::path& source_directory) {
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    GameFont font;

    if (source_directory.empty() && (!length || length == executable.size())) {
        font.error = "Game font: cannot locate game executable.";
    } else {
        const std::filesystem::path directory = source_directory.empty() ? std::filesystem::path(executable.data()).parent_path() : source_directory;
        game_directory = directory;
        portrait_ids.clear();
        processed_face.reset();
        requested_face.store(~0u);
        {
            std::lock_guard lock(font_mutex);
            current_portrait = {};
        }
        std::ifstream metadata(directory / L"0002" / L"CharaCenterTexture.fdata.info");
        std::string line;
        while (std::getline(metadata, line)) {
            const std::size_t hash_begin = line.find(',');
            const std::size_t hash_end = line.find(',', hash_begin + 1);
            const std::size_t name_end = line.find(',', hash_end + 1);
            const std::size_t marker = line.find("texture_chara_center_", hash_end);
            if (hash_begin == std::string::npos || hash_end == std::string::npos ||
                name_end == std::string::npos || marker >= name_end ||
                line.compare(hash_end + 1, 5, "R_G1T") != 0) continue;
            unsigned face = 0;
            std::uint32_t resource = 0;
            const char* number = line.data() + marker + sizeof("texture_chara_center_") - 1;
            const std::from_chars_result id = std::from_chars(number, line.data() + name_end, face);
            const std::from_chars_result hash = std::from_chars(line.data() + hash_begin + 1, line.data() + hash_end, resource, 16);
            if (id.ec == std::errc{} && hash.ec == std::errc{} && hash.ptr == line.data() + hash_end) {
                portrait_ids.emplace(face, resource);
            }
        }
        std::expected<Bytes, std::string> asset = read_game_asset(directory, FONT_ID, 0x786dcd84);

        if (!asset) {
            font.error = std::move(asset.error());
        } else {
            std::expected<GameFont, std::string> decoded = decode_font(*asset);

            if (decoded) {
                font = std::move(*decoded);
            } else {
                font.error = std::move(decoded.error());
            }
        }
    }
    if (!font.error.empty()) {
        OutputDebugStringA(font.error.c_str());
        OutputDebugStringA("\n");
    }
    own::shared_owner<const GameFont> view = own::make_shared<GameFont>(std::move(font));
    const std::lock_guard<std::mutex> lock(font_mutex);

    current_font = std::move(view);
}

own::shared_owner<const GameFont> game_font_view() {
    const std::lock_guard<std::mutex> lock(font_mutex);

    return current_font;
}

bool request_game_portrait(unsigned face) {
    return requested_face.exchange(face, std::memory_order_relaxed) != face;
}

own::shared_owner<const GamePortrait> game_portrait_view() {
    std::lock_guard lock(font_mutex);
    return current_portrait;
}

void poll_game_portrait() {
    const unsigned face = requested_face.load(std::memory_order_relaxed);
    if (face == ~0u || processed_face == face) return;
    processed_face = face;
    GamePortrait portrait{face, {}};
    const auto found = portrait_ids.find(face);
    if (found != portrait_ids.end()) {
        std::expected<Bytes, std::string> asset = read_game_asset(game_directory, found->second, 0xafbec60c);
        if (asset) {
            std::expected<RangeImage, std::string> image = decode_texture(*asset, 0);
            if (image) portrait.image = own::make_shared<RangeImage>(std::move(*image));
            else OutputDebugStringA(image.error().c_str());
        } else OutputDebugStringA(asset.error().c_str());
    }
    std::lock_guard lock(font_mutex);
    current_portrait = own::make_shared<GamePortrait>(std::move(portrait));
}

bool publish_game_ui_images(UiBridge& bridge) {
    using domain::native_ui::Rect;
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));

    if (!length || length == executable.size()) {
        return false;
    }

    const std::filesystem::path directory = game_directory;
    std::unordered_map<std::uint32_t, Bytes> containers;
    const auto load = [&](std::uint32_t id, unsigned image_index) -> std::optional<RangeImage> {
        auto found = containers.find(id);

        if (found == containers.end()) {
            std::expected<Bytes, std::string> asset = read_game_asset(directory, id, 0xafbec60c);

            if (!asset) {
                OutputDebugStringA(asset.error().c_str());
                return std::nullopt;
            }
            found = containers.emplace(id, std::move(*asset)).first;
        }

        std::expected<RangeImage, std::string> image = decode_texture(found->second, image_index);

        if (!image) {
            OutputDebugStringA(image.error().c_str());
            return std::nullopt;
        }
        return std::move(*image);
    };
    const auto copy = [](const RangeImage& source, Rect region, RangeImage& target, Rect destination) {
        if (region.right > source.width || region.bottom > source.height ||
            destination.right > target.width || destination.bottom > target.height) {
            return false;
        }
        for (unsigned row = 0; row < region.bottom - region.top; ++row) {
            std::memcpy(target.pixels.data() + ((destination.top + row) * target.width + destination.left) * 4,
                source.pixels.data() + ((region.top + row) * source.width + region.left) * 4,
                (region.right - region.left) * 4);
        }
        return true;
    };

    UiAtlasImages images;
    constexpr std::array<std::pair<std::uint32_t, unsigned>, 5> sources{{
        {0xe295681c, 27}, {0xe295681c, 32}, {0xe9ed623d, 11}, {0x5889d015, 0}, {0xe295681c, 13}}};

    for (std::size_t i = 0; i < sources.size(); ++i) {
        std::optional<RangeImage> image = load(sources[i].first, sources[i].second);

        if (!image) {
            return false;
        }
        images[i] = std::move(*image);
    }

    // Keep the existing 2048x1024 logical coordinates for compact buttons.
    RangeImage& compact = images[3];

    if (compact.width != 4096 || compact.height != 2048) {
        return false;
    }
    RangeImage reduced{2048, 1024, std::vector<std::byte>(2048 * 1024 * 4)};

    for (unsigned y = 0; y < reduced.height; ++y) {
        for (unsigned x = 0; x < reduced.width; ++x) {
            for (unsigned channel = 0; channel < 4; ++channel) {
                unsigned sum = 0;

                for (unsigned dy = 0; dy < 2; ++dy) {
                    for (unsigned dx = 0; dx < 2; ++dx) {
                        sum += std::to_integer<unsigned>(compact.pixels[((y * 2 + dy) * compact.width + x * 2 + dx) * 4 + channel]);
                    }
                }
                reduced.pixels[(y * reduced.width + x) * 4 + channel] = static_cast<std::byte>((sum + 2) / 4);
            }
        }
    }
    compact = std::move(reduced);

    RangeImage controls{1024, 2048, std::vector<std::byte>(1024 * 2048 * 4)};

    if (!copy(images[0], {0, 0, 1024, 512}, controls, {0, 0, 1024, 512})) {
        return false;
    }
    images[0] = std::move(controls);

    {
        std::optional<RangeImage> header = load(0x47e44338, 1);

        if (!header || !copy(*header, {367, 0, 780, 72}, images[0], domain::native_ui::TITLE) ||
            !copy(*header, {0, 116, 853, 156}, images[0], domain::native_ui::NOTICE)) {
            return false;
        }
    }
    {
        std::optional<RangeImage> captions = load(0xf804b71f, 0);

        if (!captions || !copy(*captions, {536, 780, 802, 838}, images[0], domain::native_ui::EDIT) ||
            !copy(*captions, {1340, 720, 1606, 778}, images[0], domain::native_ui::SETTINGS) ||
            !copy(*captions, {536, 960, 802, 1018}, images[0], domain::native_ui::CITY) ||
            !copy(*captions, {1072, 960, 1338, 1018}, images[0], domain::native_ui::OFFICER)) {
            return false;
        }
    }
    {
        std::optional<RangeImage> cards = load(0xe295681c, 6);
        std::optional<RangeImage> lists = load(0x982e9247, 1);

        if (!cards || !lists || !copy(*cards, {908, 92, 988, 182}, images[0], domain::native_ui::CARD) ||
            !copy(*lists, {1712, 112, 1984, 212}, images[0], domain::native_ui::LARGE_BUTTON) ||
            !copy(*lists, {1674, 624, 1844, 678}, images[0], domain::native_ui::SELECTION)) {
            return false;
        }
    }
    {
        std::optional<RangeImage> paper = load(0x982e9247, 2);

        if (!paper || !copy(*paper, {1284, 0, 2564, 720}, images[2], {0, 0, 1280, 720})) {
            return false;
        }
    }

    {
        std::optional<RangeImage> map = load(0x9be02183, 0);
        std::optional<RangeImage> nameplate = load(0x532532ff, 1);
        if (!map || !nameplate || !copy(*map, {0, 0, 340, 225}, images[0], domain::native_ui::MAP) ||
            !copy(*nameplate, {360, 0, 576, 48}, images[0], domain::native_ui::NAMEPLATE)) return false;
    }
    // Add the eleven native tree illustrations without changing the existing five texture slots.
    RangeImage paper_atlas{4096, 2048, std::vector<std::byte>(4096 * 2048 * 4)};
    if (!copy(images[2], {0, 0, 2048, 2048}, paper_atlas, {0, 0, 2048, 2048})) return false;
    images[2] = std::move(paper_atlas);
    constexpr std::array<std::uint32_t, 11> trees{0x52e20e1c, 0x80baa87b, 0xae9342da, 0xdc6bdd39,
        0x0a447798, 0x381d11f7, 0x65f5ac56, 0x93ce46b5, 0xc1a6e114, 0x6894917e, 0x966d2bdd};
    for (unsigned category = 0; category < trees.size(); ++category) {
        std::optional<RangeImage> tree = load(trees[category], 0);
        if (!tree || tree->width != 1024 || tree->height != 1024) return false;
        const unsigned left = 2048 + category % 4 * 512;
        const unsigned top = category / 4 * 512;
        for (unsigned y = 0; y < 512; ++y) {
            for (unsigned x = 0; x < 512; ++x) {
                for (unsigned channel = 0; channel < 4; ++channel) {
                    unsigned sum = 0;
                    for (unsigned dy = 0; dy < 2; ++dy) {
                        for (unsigned dx = 0; dx < 2; ++dx) {
                            sum += std::to_integer<unsigned>(tree->pixels[((y * 2 + dy) * 1024 + x * 2 + dx) * 4 + channel]);
                        }
                    }
                    images[2].pixels[((top + y) * 4096 + left + x) * 4 + channel] = static_cast<std::byte>((sum + 2) / 4);
                }
            }
        }
    }
    std::optional<RangeImage> mural = load(0xae8b982b, 0);
    std::optional<RangeImage> jewels = load(0xac311b9c, 0);
    if (!mural || !jewels || !copy(*mural, {248, 72, 1024, 528}, images[2], domain::native_ui::JEWEL_BACKDROP)) return false;
    for (const domain::native_ui::JewelSprite& sprite : domain::native_ui::JEWEL_FRAMES) {
        if (!copy(*jewels, sprite.source, images[0], sprite.atlas)) return false;
    }
    for (const domain::native_ui::JewelSprite& sprite : domain::native_ui::JEWEL_GEMS) {
        if (!copy(*jewels, sprite.source, images[0], sprite.atlas)) return false;
    }
    for (const domain::native_ui::JewelSprite& sprite : domain::native_ui::JEWEL_CATEGORY_BUTTONS) {
        if (!copy(*jewels, sprite.source, images[0], sprite.atlas)) return false;
    }
    if (!copy(*jewels, domain::native_ui::JEWEL_START.source, images[0], domain::native_ui::JEWEL_START.atlas) ||
        !copy(*jewels, domain::native_ui::JEWEL_BUTTON_HOVER.source, images[0], domain::native_ui::JEWEL_BUTTON_HOVER.atlas)) return false;
    bridge.publish_ui_atlas_images(std::move(images));
    return true;
}

} // namespace s8::services
