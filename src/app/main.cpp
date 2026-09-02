#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "raylib.h"

#include "app/Game.h"
#include "content/Registry.h"
#include "content/Startup.h"
#include "content/Validate.h"
#include "render/Capture.h"
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
    bool freshRun = false;
    bool hub = false;
    int openSlot = -1;
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
    bool abilities = false;
    bool settings = false;
    bool pauseIt = false;
    const char* menuPage = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shotPath = argv[++i];
        else if (std::strcmp(argv[i], "--after") == 0 && i + 1 < argc)
            shotAfter = static_cast<float>(std::atof(argv[++i]));
        else if (std::strcmp(argv[i], "--autostart") == 0)
            autostart = true;
        // The REAL opening: a starting purse and an empty board, which is what a
        // new player meets. --autostart is the dev run with money and towers.
        else if (std::strcmp(argv[i], "--freshrun") == 0)
            freshRun = true;
        else if (std::strcmp(argv[i], "--hub") == 0)
            hub = true;
        else if (std::strcmp(argv[i], "--openslot") == 0 && i + 1 < argc)
            openSlot = std::atoi(argv[++i]);
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
        else if (std::strcmp(argv[i], "--menupage") == 0 && i + 1 < argc)
            menuPage = argv[++i];
        else if (std::strcmp(argv[i], "--abilities") == 0)
            abilities = true;
        else if (std::strcmp(argv[i], "--settings") == 0)
            settings = true;
        else if (std::strcmp(argv[i], "--pause") == 0)
            pauseIt = true;
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

    // Content problems are reported and fatal, not thrown and not ignored.
    // loadAll used to be called with no try/catch, so a malformed file aborted
    // the process with "libc++abi: terminating due to uncaught exception" -- the
    // loader's precise message was produced and then discarded. Validation
    // errors were logged and the game started anyway, which meant content the
    // validator had already declared broken ran in an undefined state.
    content::Registry registry;
    if (const auto outcome = content::loadAndValidate(registry, TD_CONTENT_DIR); !outcome.ok) {
        std::fprintf(stderr, "\nCannot start: %s\n\n", outcome.problem.c_str());
        return 2;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(render::kVirtualW, render::kVirtualH, "Wardstone");
    if (!IsWindowReady()) {
        // Without this the game runs on regardless and buries the player in
        // "GLFW: Failed to find selected monitor" and framebuffer warnings. The
        // common cause on macOS is a locked screen, which denies a process the
        // window server -- worth naming, because the message otherwise looks
        // like a graphics driver fault.
        std::fprintf(stderr,
                     "\nCannot open a window. The display is unavailable -- on macOS this "
                     "usually means the screen is locked.\n\n");
        return 3;
    }
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
        if (abilities) game.devCastAbilities();
        if (settings) game.devSettings();
        if (jumpWave > 0) game.devJumpToWave(jumpWave);
        if (pauseIt) game.devPause();
    }
    // Order matters: opening a profile has to come first, or a run started
    // before it has no profile and reads a default one -- which is how a capture
    // of "the tutorial" came back showing step 0 whatever the save said.
    if (openSlot >= 0) game.openSlot(openSlot);
    if (freshRun) game.requestStart(/*demoTowers=*/false, forceMap ? forceMap : "");
    if (hub) { game.openHub(0); game.devSetHubTab(hubTab); }
    if (artCompare) game.showArtCompare();
    if (spriteSheet) game.showSpriteSheet();
    if (menuAt) {
        game.requestStart(/*demoTowers=*/true);
        game.devOpenMenu(menuX, menuY);
        if (menuPage) game.devMenuPage(menuPage);
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
            // Not TakeScreenshot: it prepends the working directory, so an
            // absolute path silently produced "<cwd>//tmp/x.png" and saved
            // nothing. ExportImage writes where it is told.
            const Image shot = render::captureScreen();
            if (!ExportImage(shot, shotPath))
                std::fprintf(stderr, "Could not write %s\n", shotPath);
            UnloadImage(shot);
            break;
        }
    }

    // Closing the window used to discard everything since the last wave
    // boundary -- which is the whole of the current build phase, the moment a
    // player spends their gold. A run mid-wave still cannot be written (enemies
    // are not serialised), but one sitting in a build phase now is.
    game.maybeAutosave();

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
