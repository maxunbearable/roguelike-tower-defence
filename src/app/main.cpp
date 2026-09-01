#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "raylib.h"

#include "app/Game.h"
#include "content/Registry.h"
#include "content/Validate.h"
#include "render/PixelCanvas.h"
#include "sim/World.h"

namespace {

// Largest integer scale whose window still fits comfortably on the monitor.
// Non-integer scaling makes pixel art shimmer, so we size the window to the art
// rather than stretching the art to the window.
int bestWindowScale() {
    const int mon = GetCurrentMonitor();
    const int mw = GetMonitorWidth(mon);
    const int mh = GetMonitorHeight(mon);
    if (mw <= 0 || mh <= 0) return 1;
    int s = 1;
    while ((td::render::kVirtualW * (s + 1)) <= static_cast<int>(mw * 0.9) &&
           (td::render::kVirtualH * (s + 1)) <= static_cast<int>(mh * 0.9)) {
        ++s;
    }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace td;

    // Dev capture mode: --shot <file> [--after <seconds>] writes ONLY the game
    // framebuffer. --autostart skips the menu.
    const char* shotPath = nullptr;
    float shotAfter = 0.0f;
    bool autostart = false;
    bool hub = false;
    bool artCompare = false;
    bool spriteSheet = false;
    bool menuAt = false;
    bool results = false;
    int hubTab = 0;
    int menuX = 5, menuY = 3;
    bool mapsScreen = false;
    const char* forceMap = nullptr;
    int jumpWave = 0;
    int cluster = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shotPath = argv[++i];
        else if (std::strcmp(argv[i], "--after") == 0 && i + 1 < argc)
            shotAfter = static_cast<float>(std::atof(argv[++i]));
        else if (std::strcmp(argv[i], "--autostart") == 0)
            autostart = true;
        else if (std::strcmp(argv[i], "--hub") == 0)
            hub = true;
        else if (std::strcmp(argv[i], "--artcompare") == 0)
            artCompare = true;
        else if (std::strcmp(argv[i], "--tab") == 0 && i + 1 < argc)
            hubTab = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--menu") == 0) {
            menuAt = true;
            // Optional tile: "--menu 8 3". Defaults to bare grass, which is the
            // build menu -- the most common interaction.
            if (i + 2 < argc && argv[i + 1][0] != '-' && argv[i + 2][0] != '-') {
                menuX = std::atoi(argv[++i]);
                menuY = std::atoi(argv[++i]);
            }
        }
        else if (std::strcmp(argv[i], "--cluster") == 0 && i + 1 < argc)
            cluster = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--wave") == 0 && i + 1 < argc)
            jumpWave = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            forceMap = argv[++i];
        else if (std::strcmp(argv[i], "--maps") == 0)
            mapsScreen = true;
        else if (std::strcmp(argv[i], "--results") == 0)
            results = true;
        else if (std::strcmp(argv[i], "--sprites") == 0)
            spriteSheet = true;

    }

    content::Registry registry;
    registry.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    for (const auto& err : content::validate(registry)) {
        TraceLog(LOG_ERROR, "content: %s", err.c_str());
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(render::kVirtualW, render::kVirtualH, "Tower Defense");
    InitAudioDevice();
    SetMasterVolume(0.65f);
    const int s = bestWindowScale();
    SetWindowSize(render::kVirtualW * s, render::kVirtualH * s);
    SetWindowPosition((GetMonitorWidth(GetCurrentMonitor()) - render::kVirtualW * s) / 2,
                      (GetMonitorHeight(GetCurrentMonitor()) - render::kVirtualH * s) / 2);
    if (!shotPath) SetTargetFPS(60);

    app::Game game(registry);
    game.loadArt();  // after InitWindow: textures need a GL context
    if (autostart) {
        game.requestStart(/*demoTowers=*/true, forceMap ? forceMap : "");
        if (cluster > 0) game.devCluster(cluster);
        if (jumpWave > 0) game.devJumpToWave(jumpWave);
    }
    if (hub) { game.openHub(0); game.devSetHubTab(hubTab); }
    if (artCompare) game.showArtCompare();
    if (spriteSheet) game.showSpriteSheet();
    if (menuAt) {
        game.requestStart(/*demoTowers=*/true);
        game.devOpenMenu(menuX, menuY);
    }
    if (mapsScreen) game.devShowMaps();
    if (results) game.devShowResults(/*award=*/126);

    // The framebuffer is not reliably presentable on the first few frames, so a
    // capture at frame 0 came back solid black. Always warm up before grabbing.
    constexpr int kShotWarmupFrames = 8;
    float elapsed = 0.0f;
    int frames = 0;
    while (!WindowShouldClose()) {
        const float dt = shotPath ? sim::kFixedDt : GetFrameTime();
        elapsed += dt;
        ++frames;
        game.frame(dt);
        if (shotPath && elapsed >= shotAfter && frames >= kShotWarmupFrames) {
            TakeScreenshot(shotPath);
            break;
        }
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
