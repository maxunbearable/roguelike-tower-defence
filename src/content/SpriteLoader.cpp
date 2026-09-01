#include "content/SpriteLoader.h"

#include <stdexcept>

#include <toml++/toml.hpp>

namespace td::content {
namespace {

uint32_t parseHex(const std::string& s, const std::string& where) {
    if (s.size() != 8) {
        throw std::runtime_error(where + ": colour '" + s + "' must be 8 hex digits (RRGGBBAA)");
    }
    return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
}

}  // namespace

std::map<std::string, SpriteDef> loadSprites(const std::filesystem::path& file) {
    const toml::table tbl = toml::parse_file(file.string());

    std::map<char, uint32_t> palette;
    const auto* pal = tbl["palette"].as_table();
    if (!pal) throw std::runtime_error(file.string() + ": missing [palette]");
    for (const auto& [k, v] : *pal) {
        const std::string key(k.str());
        if (key.size() != 1) {
            throw std::runtime_error(file.string() + ": palette key '" + key +
                                     "' must be a single character");
        }
        const auto hex = v.value<std::string>();
        if (!hex) throw std::runtime_error(file.string() + ": palette '" + key + "' is not a string");
        palette[key[0]] = parseHex(*hex, file.string() + " palette '" + key + "'");
    }

    std::map<std::string, SpriteDef> out;
    const auto* arr = tbl["sprite"].as_array();
    if (!arr) throw std::runtime_error(file.string() + ": no [[sprite]] entries");

    for (const auto& elem : *arr) {
        const auto* st = elem.as_table();
        if (!st) throw std::runtime_error(file.string() + ": [[sprite]] is not a table");

        SpriteDef sp;
        const auto id = (*st)["id"].value<std::string>();
        if (!id) throw std::runtime_error(file.string() + ": sprite is missing an id");
        sp.id = *id;

        const auto* size = (*st)["size"].as_array();
        if (!size || size->size() != 2) {
            throw std::runtime_error("sprite '" + sp.id + "': size must be [w, h]");
        }
        sp.w = static_cast<int>((*size)[0].value_or<int64_t>(0));
        sp.h = static_cast<int>((*size)[1].value_or<int64_t>(0));
        if (sp.w <= 0 || sp.h <= 0) {
            throw std::runtime_error("sprite '" + sp.id + "': size must be positive");
        }

        const auto* rows = (*st)["rows"].as_array();
        if (!rows) throw std::runtime_error("sprite '" + sp.id + "': missing rows");
        if (static_cast<int>(rows->size()) != sp.h) {
            throw std::runtime_error("sprite '" + sp.id + "': " + std::to_string(rows->size()) +
                                     " rows but height is " + std::to_string(sp.h));
        }

        sp.pixels.reserve(static_cast<size_t>(sp.w) * static_cast<size_t>(sp.h));
        for (size_t y = 0; y < rows->size(); ++y) {
            const auto row = (*rows)[y].value<std::string>();
            if (!row) throw std::runtime_error("sprite '" + sp.id + "': row is not a string");
            if (static_cast<int>(row->size()) != sp.w) {
                throw std::runtime_error("sprite '" + sp.id + "' row " + std::to_string(y) +
                                         ": width " + std::to_string(row->size()) +
                                         " but declared " + std::to_string(sp.w));
            }
            for (const char c : *row) {
                const auto it = palette.find(c);
                if (it == palette.end()) {
                    throw std::runtime_error("sprite '" + sp.id + "': character '" +
                                             std::string(1, c) + "' is not in the palette");
                }
                sp.pixels.push_back(it->second);
            }
        }
        out[sp.id] = std::move(sp);
    }
    return out;
}

}  // namespace td::content
