#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/SpriteLoader.h"

static std::filesystem::path artFile() {
    return std::filesystem::path(TD_CONTENT_DIR) / "art" / "sprites.toml";
}

TEST_CASE("the shipped art file decodes", "[art]") {
    const auto sprites = td::content::loadSprites(artFile());
    REQUIRE(sprites.size() >= 14);
    for (const char* id : {"slime_0", "slime_1", "wolf_0", "goblin_0", "wraith_0", "arrow",
                           "tower_base", "crown_sniper", "crown_elf", "crown_hunter", "gem"}) {
        UNSCOPED_INFO("sprite " << id);
        REQUIRE(sprites.count(id) == 1);
    }
}

TEST_CASE("every sprite's pixel buffer matches its declared size", "[art]") {
    const auto sprites = td::content::loadSprites(artFile());
    for (const auto& [id, sp] : sprites) {
        UNSCOPED_INFO("sprite " << id);
        REQUIRE(sp.w > 0);
        REQUIRE(sp.h > 0);
        REQUIRE(sp.pixels.size() == static_cast<size_t>(sp.w) * static_cast<size_t>(sp.h));
    }
}

TEST_CASE("sprites are not blank and do carry transparency", "[art]") {
    const auto sprites = td::content::loadSprites(artFile());
    for (const auto& [id, sp] : sprites) {
        UNSCOPED_INFO("sprite " << id);
        int opaque = 0, clear = 0;
        for (const auto px : sp.pixels) {
            if ((px & 0xFFu) == 0) ++clear;
            else ++opaque;
        }
        REQUIRE(opaque > 0);  // an all-transparent sprite is an authoring mistake
        REQUIRE(clear > 0);   // and so is a solid rectangle with no silhouette
    }
}

TEST_CASE("every creature has a full animation cycle", "[art]") {
    // Four frames is the minimum that reads as motion; two is a slideshow.
    const auto sprites = td::content::loadSprites(artFile());
    for (const char* base : {"slime", "wolf", "goblin", "wraith"}) {
        int frames = 0;
        while (sprites.count(std::string(base) + "_" + std::to_string(frames))) ++frames;
        UNSCOPED_INFO(base << " has " << frames << " frames");
        REQUIRE(frames >= 4);
    }
}

TEST_CASE("animation frames agree on their dimensions", "[art]") {
    const auto sprites = td::content::loadSprites(artFile());
    for (const char* base : {"slime", "wolf", "goblin", "wraith"}) {
        const auto& first = sprites.at(std::string(base) + "_0");
        for (int f = 1;; ++f) {
            const auto it = sprites.find(std::string(base) + "_" + std::to_string(f));
            if (it == sprites.end()) break;
            UNSCOPED_INFO("frame " << f << " of " << base);
            REQUIRE(it->second.w == first.w);
            REQUIRE(it->second.h == first.h);
        }
    }
}

TEST_CASE("consecutive frames actually differ", "[art]") {
    // A cycle whose frames are identical animates nothing. This caught a real
    // authoring slip where a phase table repeated a value.
    const auto sprites = td::content::loadSprites(artFile());
    for (const char* base : {"slime", "wolf", "goblin", "wraith"}) {
        int identical = 0, pairs = 0;
        for (int f = 0;; ++f) {
            const auto a = sprites.find(std::string(base) + "_" + std::to_string(f));
            const auto b = sprites.find(std::string(base) + "_" + std::to_string(f + 1));
            if (a == sprites.end() || b == sprites.end()) break;
            ++pairs;
            if (a->second.pixels == b->second.pixels) ++identical;
        }
        UNSCOPED_INFO(base << ": " << identical << " identical of " << pairs << " pairs");
        REQUIRE(pairs > 0);
        REQUIRE(identical < pairs);  // at least some motion between frames
    }
}
