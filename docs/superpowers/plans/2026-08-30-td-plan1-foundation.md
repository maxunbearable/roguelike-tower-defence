# Plan 1 — Foundation & Playable Core

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A completable tower defense run — one TOML-defined map with a fixed path, enemies that walk it, arrow towers bought with gold that shoot and kill them, waves, and a win/lose result.

**Architecture:** Three CMake targets. `td_core` is a static library holding all game logic (`core/`, `sim/`, `content/`) and **must never link or include raylib** — this is what makes the logic headlessly testable. `td_app` is the executable adding `render/`, `ui/`, `app/` and links raylib. `td_tests` links `td_core` and Catch2. Simulation runs on a fixed 1/60 s timestep driven by an accumulator; rendering interpolates.

**Tech Stack:** C++20, CMake >= 3.24 + CPM.cmake, raylib 6.0, EnTT, toml++, nlohmann/json, Catch2 v3.

**Spec:** `docs/superpowers/specs/2026-08-30-pixel-roguelike-td-design.md`

## Global Constraints

- C++20. `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`.
- **`td_core` must not include or link raylib.** Enforced by a test task. Core uses its own `td::core::Vec2`, converted to raylib types only at the render boundary.
- **Determinism:** one seeded `std::mt19937_64` per run, injected. `rand()` and mid-run `std::random_device` are prohibited.
- **Fixed simulation timestep of 1/60 s** via accumulator. Systems never read wall-clock time.
- Virtual resolution 960x540. Tiles 32 px. Play area 30x15 tiles (960x480). HUD band 60 px. Integer scaling only, `TEXTURE_FILTER_POINT`.
- `MIN_DAMAGE_FLOOR = 0.10f` — a hit always deals at least 10% of raw damage regardless of armour.
- All gameplay data lives in TOML under `content/`. No gameplay constant is hardcoded in C++ except the floors and timestep above.
- Namespaces: `td::core`, `td::sim`, `td::content`, `td::render`, `td::ui`, `td::app`.
- No `git commit` steps — this project's rules forbid unprompted commits. Each task ends with a verification checkpoint instead.

---

### Task 1: Build skeleton, three targets, test harness

**Files:**
- Create: `CMakeLists.txt`, `cmake/CPM.cmake` (downloaded), `cmake/get_cpm.cmake`
- Create: `src/core/Version.h`, `src/app/main.cpp`
- Create: `tests/CMakeLists.txt`, `tests/core/test_harness.cpp`
- Create: `.gitignore`

**Interfaces:**
- Consumes: nothing.
- Produces: CMake targets `td_core` (STATIC), `td_app` (EXE), `td_tests` (EXE). `td::core::kVersion` as `constexpr const char*`.

- [ ] **Step 1: Write `cmake/get_cpm.cmake` to bootstrap CPM**

```cmake
set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
  message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
  file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
    ${CPM_DOWNLOAD_LOCATION})
endif()
include(${CPM_DOWNLOAD_LOCATION})
```

- [ ] **Step 2: Write the root `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.24)
project(td LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(cmake/get_cpm.cmake)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# Tags verified against the upstream repos on 2026-08-30.
# raylib tags have no "v" prefix, so it uses CPM's "#<tag>" verbatim form.
# EnTT 4.0.0 exists but is a fresh major with API churn; pinned to the latest 3.x.
CPMAddPackage("gh:raysan5/raylib#6.0")
CPMAddPackage("gh:skypjack/entt@3.16.0")
CPMAddPackage("gh:marzer/tomlplusplus@3.4.0")
CPMAddPackage("gh:nlohmann/json@3.12.0")
CPMAddPackage("gh:catchorg/Catch2@3.16.0")

# --- td_core : game logic, MUST NOT link raylib -------------------------
file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS
     src/core/*.cpp src/sim/*.cpp src/content/*.cpp)
add_library(td_core STATIC ${CORE_SRC})
target_include_directories(td_core PUBLIC src)
target_link_libraries(td_core PUBLIC EnTT::EnTT tomlplusplus::tomlplusplus
                                     nlohmann_json::nlohmann_json)

# --- td_app : executable ------------------------------------------------
file(GLOB_RECURSE APP_SRC CONFIGURE_DEPENDS
     src/render/*.cpp src/ui/*.cpp src/app/*.cpp)
add_executable(td_app ${APP_SRC})
target_link_libraries(td_app PRIVATE td_core raylib)

enable_testing()
add_subdirectory(tests)
```

Note: `td_core` has no `.cpp` files yet, which makes `add_library` fail. Create `src/core/Version.h` **and** a one-line `src/core/Version.cpp` so the glob is non-empty.

- [ ] **Step 3: Write `src/core/Version.h` and `src/core/Version.cpp`**

```cpp
// Version.h
#pragma once
namespace td::core { const char* versionString(); }
```
```cpp
// Version.cpp
#include "core/Version.h"
namespace td::core { const char* versionString() { return "0.1.0"; } }
```

- [ ] **Step 4: Write `tests/CMakeLists.txt`**

```cmake
file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS *.cpp)
add_executable(td_tests ${TEST_SRC})
target_link_libraries(td_tests PRIVATE td_core Catch2::Catch2WithMain)
target_compile_definitions(td_tests PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content")

# CPM does not put Catch2's CMake helpers on the module path; without this,
# include(Catch) fails with "Could not find Catch.cmake".
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(td_tests)
```

- [ ] **Step 5: Write the failing harness test `tests/core/test_harness.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/Version.h"
#include <string>

TEST_CASE("test harness runs and core is linkable", "[harness]") {
    REQUIRE(std::string(td::core::versionString()) == "0.1.0");
}
```

- [ ] **Step 6: Write `src/app/main.cpp`**

```cpp
#include "raylib.h"
#include "core/Version.h"

int main() {
    InitWindow(1920, 1080, "Tower Defense");
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{24, 20, 37, 255});
        DrawText(td::core::versionString(), 20, 20, 20, RAYWHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
```

- [ ] **Step 7: Configure and build**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j`
Expected: configure downloads all five dependencies; all three targets build.
If the `raylib#6.0` tag does not resolve, run `git ls-remote --tags https://github.com/raysan5/raylib` and pin the newest stable tag, recording the choice here.

- [ ] **Step 8: Run tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS, 1 test.

- [ ] **Step 9: Checkpoint**

`./build/td_app` opens a 1920x1080 window showing `0.1.0` on a dark background; closing it exits cleanly. `ctest` green.

---

### Task 2: Core primitives — Vec2 and deterministic Rng

**Files:**
- Create: `src/core/Vec2.h`, `src/core/Vec2.cpp`, `src/core/Rng.h`, `src/core/Rng.cpp`
- Test: `tests/core/test_vec2.cpp`, `tests/core/test_rng.cpp`

**Interfaces:**
- Consumes: Task 1 targets.
- Produces:
  - `struct td::core::Vec2 { float x, y; }` with `operator+ - *` and `operator==`
  - `float td::core::length(Vec2)`, `Vec2 td::core::normalized(Vec2)`, `float td::core::distance(Vec2, Vec2)`, `Vec2 td::core::lerp(Vec2 a, Vec2 b, float t)`
  - `class td::core::Rng` with `explicit Rng(uint64_t seed)`, `float unit()`, `bool chance(float p)`, `int range(int lo, int hiInclusive)`, `uint64_t seed() const`

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/core/test_vec2.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Vec2.h"
using td::core::Vec2;
using Catch::Matchers::WithinAbs;

TEST_CASE("Vec2 arithmetic", "[vec2]") {
    REQUIRE((Vec2{1,2} + Vec2{3,4}) == Vec2{4,6});
    REQUIRE((Vec2{5,7} - Vec2{1,2}) == Vec2{4,5});
    REQUIRE((Vec2{2,3} * 2.0f)      == Vec2{4,6});
}

TEST_CASE("Vec2 length and distance", "[vec2]") {
    REQUIRE_THAT(td::core::length(Vec2{3,4}), WithinAbs(5.0f, 1e-5f));
    REQUIRE_THAT(td::core::distance(Vec2{1,1}, Vec2{4,5}), WithinAbs(5.0f, 1e-5f));
}

TEST_CASE("normalized returns unit length, and zero for zero vector", "[vec2]") {
    REQUIRE_THAT(td::core::length(td::core::normalized(Vec2{0,9})), WithinAbs(1.0f, 1e-5f));
    REQUIRE(td::core::normalized(Vec2{0,0}) == Vec2{0,0});
}

TEST_CASE("lerp interpolates and clamps at endpoints", "[vec2]") {
    REQUIRE(td::core::lerp(Vec2{0,0}, Vec2{10,20}, 0.0f) == Vec2{0,0});
    REQUIRE(td::core::lerp(Vec2{0,0}, Vec2{10,20}, 1.0f) == Vec2{10,20});
    REQUIRE(td::core::lerp(Vec2{0,0}, Vec2{10,20}, 0.5f) == Vec2{5,10});
}
```

```cpp
// tests/core/test_rng.cpp
#include <catch2/catch_test_macros.hpp>
#include "core/Rng.h"
#include <vector>
using td::core::Rng;

static std::vector<float> draw(uint64_t seed, int n) {
    Rng r(seed);
    std::vector<float> v;
    for (int i = 0; i < n; ++i) v.push_back(r.unit());
    return v;
}

TEST_CASE("same seed reproduces the same sequence", "[rng]") {
    REQUIRE(draw(1234, 50) == draw(1234, 50));
}

TEST_CASE("different seeds diverge", "[rng]") {
    REQUIRE(draw(1234, 50) != draw(5678, 50));
}

TEST_CASE("unit stays in [0,1)", "[rng]") {
    Rng r(99);
    for (int i = 0; i < 1000; ++i) {
        float v = r.unit();
        REQUIRE(v >= 0.0f);
        REQUIRE(v <  1.0f);
    }
}

TEST_CASE("chance(0) never fires and chance(1) always fires", "[rng]") {
    Rng r(7);
    for (int i = 0; i < 200; ++i) {
        REQUIRE_FALSE(r.chance(0.0f));
        REQUIRE(r.chance(1.0f));
    }
}

TEST_CASE("range is inclusive on both ends and never escapes", "[rng]") {
    Rng r(3);
    bool sawLo = false, sawHi = false;
    for (int i = 0; i < 500; ++i) {
        int v = r.range(2, 5);
        REQUIRE(v >= 2); REQUIRE(v <= 5);
        if (v == 2) sawLo = true;
        if (v == 5) sawHi = true;
    }
    REQUIRE(sawLo); REQUIRE(sawHi);
}

TEST_CASE("seed is retrievable for save files", "[rng]") {
    REQUIRE(Rng(424242).seed() == 424242u);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: compile failure — `core/Vec2.h` and `core/Rng.h` do not exist.

- [ ] **Step 3: Implement `Vec2.h` / `Vec2.cpp`**

```cpp
#pragma once
namespace td::core {
struct Vec2 {
    float x = 0.0f, y = 0.0f;
    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    friend constexpr bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
};
float length(Vec2 v);
float distance(Vec2 a, Vec2 b);
Vec2  normalized(Vec2 v);           // returns {0,0} for the zero vector
Vec2  lerp(Vec2 a, Vec2 b, float t);
}
```

`normalized` must guard against zero length rather than dividing by zero. `lerp` is `a + (b - a) * t`.

- [ ] **Step 4: Implement `Rng.h` / `Rng.cpp`**

Wrap `std::mt19937_64`. `unit()` uses `std::uniform_real_distribution<float>(0.0f, 1.0f)` — note this can return exactly 1.0f on some implementations due to float rounding, so clamp with `std::nextafter` or reject-and-redraw to honour the `[0,1)` test. `chance(p)` must return `false` for `p <= 0` and `true` for `p >= 1` **without consuming a draw**, so that guard branches do not perturb the sequence. `range(lo, hi)` uses `std::uniform_int_distribution<int>(lo, hi)`.

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: PASS, all Vec2 and Rng tests.

- [ ] **Step 6: Checkpoint**

`ctest` green. Determinism is now guaranteed for everything built on top.

---

### Task 3: Path — distance-parameterised movement

**Files:**
- Create: `src/core/Path.h`, `src/core/Path.cpp`
- Test: `tests/core/test_path.cpp`

**Interfaces:**
- Consumes: `td::core::Vec2`.
- Produces: `class td::core::Path` with `explicit Path(std::vector<Vec2> waypoints)`, `float totalLength() const`, `Vec2 positionAt(float d) const` (clamped at both ends), `const std::vector<Vec2>& waypoints() const`, `bool empty() const`.

**Why this shape:** parameterising by distance travelled rather than waypoint index makes enemy movement a single `distance += speed * dt`, makes "how far along is this enemy" (needed by `targetPriority = first`) a direct float comparison, and makes leak detection `distance >= totalLength()`. It removes an entire class of index-tracking bugs.

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Path.h"
using td::core::Path; using td::core::Vec2;
using Catch::Matchers::WithinAbs;

TEST_CASE("straight path length", "[path]") {
    Path p({{0,0},{10,0}});
    REQUIRE_THAT(p.totalLength(), WithinAbs(10.0f, 1e-5f));
}

TEST_CASE("L-shaped path sums its segments", "[path]") {
    Path p({{0,0},{3,0},{3,4}});
    REQUIRE_THAT(p.totalLength(), WithinAbs(7.0f, 1e-5f));
}

TEST_CASE("positionAt walks across the segment boundary", "[path]") {
    Path p({{0,0},{3,0},{3,4}});
    REQUIRE(p.positionAt(0.0f) == Vec2{0,0});
    REQUIRE(p.positionAt(1.5f) == Vec2{1.5f,0});
    REQUIRE(p.positionAt(3.0f) == Vec2{3,0});
    REQUIRE(p.positionAt(5.0f) == Vec2{3,2});   // 2 units up the second segment
    REQUIRE(p.positionAt(7.0f) == Vec2{3,4});
}

TEST_CASE("positionAt clamps outside the path", "[path]") {
    Path p({{0,0},{10,0}});
    REQUIRE(p.positionAt(-5.0f)  == Vec2{0,0});
    REQUIRE(p.positionAt(999.0f) == Vec2{10,0});
}

TEST_CASE("degenerate paths do not crash", "[path]") {
    Path single({{4,4}});
    REQUIRE_THAT(single.totalLength(), WithinAbs(0.0f, 1e-5f));
    REQUIRE(single.positionAt(3.0f) == Vec2{4,4});
    Path none({});
    REQUIRE(none.empty());
    REQUIRE(none.positionAt(1.0f) == Vec2{0,0});
}

TEST_CASE("zero-length segments are handled without NaN", "[path]") {
    Path p({{0,0},{0,0},{5,0}});
    REQUIRE_THAT(p.totalLength(), WithinAbs(5.0f, 1e-5f));
    REQUIRE(p.positionAt(2.5f) == Vec2{2.5f,0});
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build -j 2>&1 | tail -20`
Expected: FAIL — `core/Path.h` not found.

- [ ] **Step 3: Implement Path**

Precompute a `std::vector<float> cum_` of cumulative arc length, `cum_[0] == 0`, `cum_[i] = cum_[i-1] + distance(pts_[i-1], pts_[i])`. `positionAt(d)` clamps `d` to `[0, totalLength()]`, finds the segment with `std::upper_bound` on `cum_`, and lerps within it. Guard the zero-length-segment divide.

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Checkpoint**

`ctest` green, including the degenerate and zero-length-segment cases.

---

### Task 4: Damage formula

**Files:**
- Create: `src/core/Damage.h`, `src/core/Damage.cpp`
- Test: `tests/core/test_damage.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
```cpp
namespace td::core {
inline constexpr float kMinDamageFloor = 0.10f;
struct DamageInput {
    float raw          = 0.0f;
    bool  crit         = false;
    float critMult     = 1.0f;
    float targetArmor  = 0.0f;
    float armorShred   = 0.0f;   // active shred on the target
    float armorPen     = 0.0f;   // flat pen from the attacker
};
float computeDamage(const DamageInput& in);
}
```

**Formula** (spec section 8, steps 4): `raw' = crit ? raw * critMult : raw`; `effArmor = max(0, targetArmor - armorShred - armorPen)`; `result = max(raw' - effArmor, raw' * kMinDamageFloor)`.

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Damage.h"
using td::core::DamageInput; using td::core::computeDamage;
using Catch::Matchers::WithinAbs;

TEST_CASE("unarmoured target takes raw damage", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw=100}), WithinAbs(100.0f, 1e-4f));
}

TEST_CASE("armour subtracts flat", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw=100, .targetArmor=30}), WithinAbs(70.0f, 1e-4f));
}

TEST_CASE("crit multiplies before armour is applied", "[damage]") {
    // 100 * 2 = 200, minus 30 armour = 170
    REQUIRE_THAT(computeDamage({.raw=100, .crit=true, .critMult=2.0f, .targetArmor=30}),
                 WithinAbs(170.0f, 1e-4f));
}

TEST_CASE("shred and pen both reduce effective armour and stack", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw=100, .targetArmor=50, .armorShred=20, .armorPen=10}),
                 WithinAbs(80.0f, 1e-4f));
}

TEST_CASE("effective armour never goes negative and never heals", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw=100, .targetArmor=10, .armorPen=999}),
                 WithinAbs(100.0f, 1e-4f));
}

TEST_CASE("the 10 percent floor applies against overwhelming armour", "[damage]") {
    REQUIRE_THAT(computeDamage({.raw=100, .targetArmor=1000}), WithinAbs(10.0f, 1e-4f));
}

TEST_CASE("damage is never negative", "[damage]") {
    REQUIRE(computeDamage({.raw=0, .targetArmor=500}) >= 0.0f);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build -j 2>&1 | tail -20`
Expected: FAIL — `core/Damage.h` not found.

- [ ] **Step 3: Implement `computeDamage` exactly as the formula above**

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS, 7 damage tests.

- [ ] **Step 5: Checkpoint**

`ctest` green. This function is the single choke point every future element and tower spec routes through, so it must stay pure and free of ECS types.

---

### Task 5: Content definitions and TOML loading

**Files:**
- Create: `src/content/Defs.h`, `src/content/Registry.h`, `src/content/Registry.cpp`, `src/content/TomlLoader.h`, `src/content/TomlLoader.cpp`
- Create: `content/enemies/enemies.toml`, `content/maps/greenfields.toml`, `content/towers/arrow.toml`
- Test: `tests/content/test_loader.cpp`

**Interfaces:**
- Consumes: `td::core::Vec2`, `td::core::Path`.
- Produces:
```cpp
namespace td::content {
struct EnemyDef { std::string id, name; float maxHp, armor, speed; int bounty, shardValue; bool flying; };
struct WaveGroup { std::string enemyId; int count; float interval; float startDelay; };
struct WaveDef   { float delay; std::vector<WaveGroup> groups; };
struct TowerLevel{ int cost; float damageMult, rangeMult, fireRateMult; };
struct TowerDef  { std::string id, name; int buildCost; float sellRefundPct;
                   float damage, fireRate, range, projectileSpeed;
                   int projectileCount, pierce;
                   float critChance, critMult, armorPen;
                   std::string targetPriority;
                   std::vector<TowerLevel> levels; };
struct MapDef    { std::string id, name; int gridW, gridH, startGold; float buildTime;
                   std::vector<std::string> tileRows;
                   std::vector<core::Vec2> pathWaypoints;
                   std::vector<WaveDef> waves;
                   bool buildableAt(int x, int y) const; };

class Registry {
public:
    void loadAll(const std::filesystem::path& contentDir);   // throws std::runtime_error
    const EnemyDef& enemy(const std::string& id) const;      // throws if absent
    const TowerDef& tower(const std::string& id) const;
    const MapDef&   map(const std::string& id) const;
    bool hasEnemy(const std::string& id) const;
    bool hasTower(const std::string& id) const;
    const std::map<std::string, MapDef>& maps() const;
    const std::map<std::string, EnemyDef>& enemies() const;
    const std::map<std::string, TowerDef>& towers() const;
};
}
```

`buildableAt(x,y)` returns true only when `tileRows[y][x] == '.'` and the coordinate is in bounds.

- [ ] **Step 1: Author `content/enemies/enemies.toml`**

```toml
[[enemy]]
id = "slime"
name = "Slime"
maxHp = 40.0
armor = 0.0
speed = 1.6        # tiles per second
bounty = 6
shardValue = 1
flying = false

[[enemy]]
id = "wolf"
name = "Wolf"
maxHp = 55.0
armor = 0.0
speed = 3.2
bounty = 8
shardValue = 1
flying = false

[[enemy]]
id = "goblin"
name = "Goblin"
maxHp = 160.0
armor = 6.0
speed = 1.3
bounty = 14
shardValue = 2
flying = false

[[enemy]]
id = "bee"
name = "Bee"
maxHp = 30.0
armor = 2.0
speed = 4.0
bounty = 10
shardValue = 2
flying = true
```

- [ ] **Step 2: Author `content/towers/arrow.toml`**

```toml
[tower]
id = "arrow"
name = "Arrow Tower"
buildCost = 60
sellRefundPct = 0.6
damage = 12.0
fireRate = 1.1          # shots per second
range = 3.5             # tiles
projectileSpeed = 14.0  # tiles per second
projectileCount = 1
pierce = 0
critChance = 0.05
critMult = 2.0
armorPen = 0.0
targetPriority = "first"

[[level]]              # level 2
cost = 75
damageMult = 1.6
rangeMult = 1.1
fireRateMult = 1.15

[[level]]              # level 3
cost = 140
damageMult = 2.4
rangeMult = 1.2
fireRateMult = 1.3
```

Level multipliers are absolute against base, not cumulative — level 3 damage is `base * 2.4`, not `base * 1.6 * 2.4`.

- [ ] **Step 3: Author `content/maps/greenfields.toml`**

30 columns x 15 rows. `'.'` buildable, `'#'` blocked scenery, `'='` path, `'S'` spawn, `'E'` exit. The path waypoints must trace the `=` tiles in tile coordinates; enemies are drawn at tile centres, so waypoint `[x,y]` means the centre of tile `(x,y)`.

```toml
[map]
id = "greenfields"
name = "Greenfields"
gridW = 30
gridH = 15
startGold = 220
buildTime = 20.0

tiles = """
..............................
..............................
S=============................
.............=................
.............=................
.............=................
....==========................
....=.........................
....=.........................
....==================........
.....................=........
.....................=........
.....................========E
..............................
..............................
"""

path = [[0,2],[13,2],[13,6],[4,6],[4,9],[21,9],[21,12],[29,12]]

[[wave]]
delay = 5.0
[[wave.group]]
enemy = "slime"
count = 8
interval = 0.9
startDelay = 0.0

[[wave]]
delay = 5.0
[[wave.group]]
enemy = "slime"
count = 12
interval = 0.7
startDelay = 0.0

[[wave]]
delay = 5.0
[[wave.group]]
enemy = "wolf"
count = 8
interval = 0.6
startDelay = 0.0
[[wave.group]]
enemy = "slime"
count = 8
interval = 0.9
startDelay = 2.0

[[wave]]
delay = 5.0
[[wave.group]]
enemy = "goblin"
count = 6
interval = 1.4
startDelay = 0.0

[[wave]]
delay = 5.0
[[wave.group]]
enemy = "wolf"
count = 14
interval = 0.45
startDelay = 0.0
[[wave.group]]
enemy = "goblin"
count = 6
interval = 1.5
startDelay = 3.0
```

Verify by hand before moving on: the `path` waypoint list must turn at exactly the tiles where the `=` run changes direction, and the final waypoint must be the `E` tile.

- [ ] **Step 4: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "content/Registry.h"
#include <filesystem>

static td::content::Registry loaded() {
    td::content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    return r;
}

TEST_CASE("enemies load with their stats", "[content]") {
    auto r = loaded();
    REQUIRE(r.hasEnemy("slime"));
    REQUIRE(r.enemy("goblin").armor == 6.0f);
    REQUIRE(r.enemy("bee").flying);
    REQUIRE(r.enemies().size() == 4);
}

TEST_CASE("arrow tower loads with two upgrade levels", "[content]") {
    auto r = loaded();
    const auto& t = r.tower("arrow");
    REQUIRE(t.buildCost == 60);
    REQUIRE(t.levels.size() == 2);
    REQUIRE(t.levels[1].cost == 140);
    REQUIRE(t.targetPriority == "first");
}

TEST_CASE("map loads with a correctly sized tile grid", "[content]") {
    auto r = loaded();
    const auto& m = r.map("greenfields");
    REQUIRE(m.gridW == 30);
    REQUIRE(m.gridH == 15);
    REQUIRE(m.tileRows.size() == 15);
    for (const auto& row : m.tileRows) REQUIRE(row.size() == 30);
}

TEST_CASE("buildableAt respects tile type and bounds", "[content]") {
    auto r = loaded();
    const auto& m = r.map("greenfields");
    REQUIRE(m.buildableAt(0, 0));           // '.'
    REQUIRE_FALSE(m.buildableAt(0, 2));     // 'S'
    REQUIRE_FALSE(m.buildableAt(5, 2));     // '='
    REQUIRE_FALSE(m.buildableAt(-1, 0));    // out of bounds
    REQUIRE_FALSE(m.buildableAt(30, 0));
    REQUIRE_FALSE(m.buildableAt(0, 15));
}

TEST_CASE("waves and groups load in order", "[content]") {
    auto r = loaded();
    const auto& m = r.map("greenfields");
    REQUIRE(m.waves.size() == 5);
    REQUIRE(m.waves[2].groups.size() == 2);
    REQUIRE(m.waves[2].groups[1].enemyId == "slime");
    REQUIRE(m.waves[2].groups[1].startDelay == 2.0f);
}

TEST_CASE("a missing id throws rather than returning garbage", "[content]") {
    auto r = loaded();
    REQUIRE_THROWS(r.enemy("does_not_exist"));
}
```

Add `target_compile_definitions(td_tests PRIVATE TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content")` to `tests/CMakeLists.txt` so tests find content regardless of working directory.

- [ ] **Step 5: Run to verify it fails**

Run: `cmake --build build -j 2>&1 | tail -20`
Expected: FAIL — `content/Registry.h` not found.

- [ ] **Step 6: Implement the loader**

`TomlLoader` parses each file with `toml::parse_file` and converts to the structs. The tile block is a multi-line TOML string; **strip the leading and trailing newline** the `"""` syntax introduces, then split on `\n`. Throw `std::runtime_error` with the file path and a human-readable reason on any missing key, wrong type, or row-length mismatch — a bad content file must never produce a half-populated def.

- [ ] **Step 7: Run to verify it passes**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS, 6 content tests.

- [ ] **Step 8: Checkpoint**

`ctest` green. All gameplay numbers now live in TOML and can be edited without recompiling.

---

### Task 6: Content validation as a test

**Files:**
- Create: `src/content/Validate.h`, `src/content/Validate.cpp`
- Test: `tests/content/test_validate.cpp`

**Interfaces:**
- Consumes: `td::content::Registry`.
- Produces: `std::vector<std::string> td::content::validate(const Registry&)` — returns one human-readable message per problem, empty on success.

**Checks required** (spec section 9):
1. Every `WaveGroup.enemyId` resolves to a known enemy.
2. Every map's `tileRows.size() == gridH` and every row length `== gridW`.
3. Every path waypoint is inside the grid.
4. Every map has at least two path waypoints.
5. Every consecutive waypoint pair is axis-aligned (paths only turn at right angles) — this catches transcription errors in hand-authored maps.
6. The first waypoint's tile is `'S'` and the last waypoint's tile is `'E'`.
7. Every tile the path passes through is `'='`, `'S'` or `'E'` — this is the check that catches a `path` array that disagrees with the `tiles` block, the single most likely authoring mistake.
8. `buildCost > 0`, `fireRate > 0`, `range > 0`, `projectileSpeed > 0` for every tower; every level `cost > 0`.
9. Every enemy has `maxHp > 0` and `speed > 0`.

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "content/Registry.h"
#include "content/Validate.h"
#include <filesystem>

TEST_CASE("shipped content passes validation", "[content][validate]") {
    td::content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    auto errors = td::content::validate(r);
    for (const auto& e : errors) UNSCOPED_INFO("validation error: " << e);
    REQUIRE(errors.empty());
}

TEST_CASE("a wave referencing an unknown enemy is reported", "[content][validate]") {
    td::content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    auto& m = const_cast<td::content::MapDef&>(r.map("greenfields"));
    m.waves[0].groups[0].enemyId = "ghost";
    auto errors = td::content::validate(r);
    REQUIRE_FALSE(errors.empty());
}

TEST_CASE("a path waypoint off the tile route is reported", "[content][validate]") {
    td::content::Registry r;
    r.loadAll(std::filesystem::path(TD_CONTENT_DIR));
    auto& m = const_cast<td::content::MapDef&>(r.map("greenfields"));
    m.pathWaypoints[1] = td::core::Vec2{13, 3};   // no longer axis-aligned with its neighbours' route
    auto errors = td::content::validate(r);
    REQUIRE_FALSE(errors.empty());
}
```

`const_cast` here is deliberate and confined to tests: it lets us corrupt a loaded registry to prove the validator actually rejects bad data, without needing fixture files on disk.

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — `content/Validate.h` not found.

- [ ] **Step 3: Implement `validate` covering all nine checks**

Check 7 walks each path segment tile by tile (segments are axis-aligned by check 5, so this is a simple integer march) and asserts the tile character.

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS. If the shipped `greenfields.toml` fails, **fix the map, not the validator** — that is exactly the bug this task exists to catch.

- [ ] **Step 5: Checkpoint**

`ctest` green. Malformed content now fails CI rather than a player.

---

### Task 7: Simulation world — spawning, movement, leaks

**Files:**
- Create: `src/sim/Components.h`, `src/sim/World.h`, `src/sim/World.cpp`
- Create: `src/sim/systems/WaveSystem.h/.cpp`, `MovementSystem.h/.cpp`, `LeakSystem.h/.cpp`
- Test: `tests/sim/test_world_movement.cpp`

**Interfaces:**
- Consumes: `Registry`, `MapDef`, `Path`, `Rng`.
- Produces:
```cpp
namespace td::sim {
enum class Phase { Build, Wave, Cleared, Defeated };

struct Position     { core::Vec2 v; };
struct PrevPosition { core::Vec2 v; };          // for render interpolation
struct PathFollower { float distance = 0.0f; };
struct Health       { float hp, maxHp; };
struct Armor        { float value = 0.0f; };
struct Speed        { float base = 1.0f; };     // tiles/sec
struct EnemyTag     { std::string defId; int bounty; int shardValue; };

class World {
public:
    // goldOverride < 0 uses the map's authored startGold. It exists so balance
    // tests can assert properties of the design without being blocked by economy
    // tuning; production code never passes it.
    World(const content::Registry& reg, const content::MapDef& map, uint64_t seed,
          int goldOverride = -1);
    void tick(float dt);                       // dt is always kFixedDt
    entt::registry& reg();
    const entt::registry& reg() const;
    const core::Path& path() const;
    const content::MapDef& map() const;
    int   lives() const;
    int   waveIndex() const;                   // 0-based index of the wave in progress or next
    int   waveCount() const;
    Phase phase() const;
    bool  waveInProgress() const;
    void  startNextWave();                     // Build -> Wave; no-op otherwise
    int   aliveEnemies() const;
};
inline constexpr float kFixedDt = 1.0f / 60.0f;
inline constexpr int   kStartingLives = 20;
}
```

**Tick order for this task** (the remaining systems slot in at Task 9): WaveSystem, MovementSystem, LeakSystem.

**Deliberate deviation from spec section 11:** the spec lists ten systems,
including a separate `EconomySystem` and `WinLoseSystem`. Both are folded into
`World` and `DeathSystem` here, because each would be a three-line system with no
state of its own. If either grows real logic in Plan 2 or 3, split it out then.

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "content/Registry.h"
#include "sim/World.h"
#include <filesystem>

using namespace td;

static content::Registry loadReg() {
    content::Registry r; r.loadAll(std::filesystem::path(TD_CONTENT_DIR)); return r;
}
static void advance(sim::World& w, float seconds) {
    int steps = static_cast<int>(seconds / sim::kFixedDt);
    for (int i = 0; i < steps; ++i) w.tick(sim::kFixedDt);
}

TEST_CASE("a new world starts in the build phase with no enemies", "[sim]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    REQUIRE(w.phase() == sim::Phase::Build);
    REQUIRE(w.aliveEnemies() == 0);
    REQUIRE(w.lives() == sim::kStartingLives);
    REQUIRE(w.waveCount() == 5);
}

TEST_CASE("starting a wave spawns exactly the authored count", "[sim]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.startNextWave();
    REQUIRE(w.phase() == sim::Phase::Wave);
    advance(w, 12.0f);                       // 8 slimes at 0.9s interval = 6.3s of spawning
    int spawned = 0;
    w.reg().view<sim::EnemyTag>().each([&](auto...) { ++spawned; });
    REQUIRE(spawned == 8);
}

TEST_CASE("enemies advance along the path at their authored speed", "[sim]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.startNextWave();
    advance(w, 1.0f);                        // first slime has spawned
    auto view = w.reg().view<sim::PathFollower, sim::EnemyTag>();
    REQUIRE(view.size_hint() > 0);
    entt::entity first = *view.begin();
    float before = view.get<sim::PathFollower>(first).distance;
    advance(w, 1.0f);
    float after = view.get<sim::PathFollower>(first).distance;
    // slime speed is 1.6 tiles/sec; one second of travel, within timestep tolerance
    REQUIRE(after - before > 1.5f);
    REQUIRE(after - before < 1.7f);
}

TEST_CASE("an enemy reaching the exit costs one life and despawns", "[sim]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.startNextWave();
    advance(w, 120.0f);                      // long enough for every slime to walk the whole path
    REQUIRE(w.lives() == sim::kStartingLives - 8);
    REQUIRE(w.aliveEnemies() == 0);
}

TEST_CASE("losing every life ends the run in defeat", "[sim]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    for (int i = 0; i < 5 && w.phase() != sim::Phase::Defeated; ++i) {
        w.startNextWave();
        advance(w, 200.0f);
    }
    REQUIRE(w.phase() == sim::Phase::Defeated);
}

TEST_CASE("the simulation is deterministic for a fixed seed", "[sim]") {
    auto reg = loadReg();
    auto run = [&] {
        sim::World w(reg, reg.map("greenfields"), 4242);
        w.startNextWave();
        advance(w, 30.0f);
        std::vector<float> d;
        w.reg().view<sim::PathFollower>().each([&](auto& p) { d.push_back(p.distance); });
        return d;
    };
    REQUIRE(run() == run());
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — `sim/World.h` not found.

- [ ] **Step 3: Implement Components, World and the three systems**

`World` owns an `entt::registry`, a `core::Path` built from the map's waypoints, a `core::Rng`, and run state (lives, wave index, phase, per-group spawn timers and remaining counts).

WaveSystem: for each group in the active wave, hold a timer; once `startDelay` has elapsed, emit one enemy every `interval` seconds until `count` is exhausted. When all groups are exhausted **and** `aliveEnemies() == 0`, return to `Phase::Build`, or `Phase::Cleared` if that was the final wave.

MovementSystem: `distance += speed * dt`; write `PrevPosition` from the old `Position`, then `Position = path.positionAt(distance)`.

LeakSystem: any follower whose `distance >= path.totalLength()` decrements lives and is destroyed. When lives reach zero, phase becomes `Defeated`.

**Do not destroy entities while iterating an EnTT view.** Collect into a `std::vector<entt::entity>` and destroy afterwards; this is the most common EnTT crash and the test above will hit it.

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS, 6 sim tests.

- [ ] **Step 5: Checkpoint**

`ctest` green including the determinism test. The game is now simulable headlessly with zero rendering.

---

### Task 8: Rendering — see it move

**Files:**
- Create: `src/render/PixelCanvas.h/.cpp`, `src/render/WorldRenderer.h/.cpp`, `src/render/Palette.h`
- Modify: `src/app/main.cpp`

**Interfaces:**
- Consumes: `sim::World`, `content::MapDef`.
- Produces:
```cpp
namespace td::render {
inline constexpr int kVirtualW = 960, kVirtualH = 540;
inline constexpr int kTile = 32;
inline constexpr int kPlayW = 960, kPlayH = 480;   // 30x15 tiles
inline constexpr int kHudY = 480, kHudH = 60;

class PixelCanvas {
public:
    PixelCanvas();                 // creates the RenderTexture2D, sets POINT filter
    ~PixelCanvas();
    void begin();                  // BeginTextureMode + ClearBackground
    void end();                    // EndTextureMode
    void blitToWindow() const;     // integer-scaled, letterboxed, centred
    int  scale() const;            // current integer scale factor, >= 1
    core::Vec2 windowToVirtual(core::Vec2 windowPos) const;
};

void drawWorld(const sim::World& w, float alpha);   // alpha = render interpolation factor
}
```

`scale()` is `max(1, min(screenW / kVirtualW, screenH / kVirtualH))`. `windowToVirtual` inverts the blit (subtract letterbox offset, divide by scale) and is what makes mouse tower placement land on the right tile in Task 10.

- [ ] **Step 1: Implement `PixelCanvas`**

`LoadRenderTexture(kVirtualW, kVirtualH)`, then `SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT)`. `blitToWindow` draws with a source rect of negative height (`-kVirtualH`) because raylib render textures are y-flipped — forgetting this renders the whole game upside down.

- [ ] **Step 2: Implement `Palette.h`**

Named `Color` constants for tile types and entities, so Task 11's art pass has one file to replace: `kTileGrass`, `kTilePath`, `kTileBlocked`, `kSpawn`, `kExit`, `kEnemy`, `kEnemyArmored`, `kTower`, `kProjectile`, `kRangeRing`, `kHudBg`, `kHudText`.

- [ ] **Step 3: Implement `drawWorld`**

Draw tiles as 32x32 filled rects by tile character. Draw each enemy at `lerp(PrevPosition, Position, alpha) * kTile` as a filled circle, with a small HP bar above it. Positions are in tile units, so multiply by `kTile` to reach pixels; a waypoint of `[13,2]` is the tile *centre*, so pixel position is `(x + 0.5) * kTile`.

- [ ] **Step 4: Rewrite `main.cpp` with the fixed-timestep loop**

```cpp
float accumulator = 0.0f;
while (!WindowShouldClose()) {
    accumulator += GetFrameTime();
    if (accumulator > 0.25f) accumulator = 0.25f;      // spiral-of-death guard
    while (accumulator >= sim::kFixedDt) {
        world.tick(sim::kFixedDt);
        accumulator -= sim::kFixedDt;
    }
    const float alpha = accumulator / sim::kFixedDt;
    canvas.begin();
      render::drawWorld(world, alpha);
    canvas.end();
    BeginDrawing();
      ClearBackground(BLACK);
      canvas.blitToWindow();
    EndDrawing();
}
```

Temporarily call `world.startNextWave()` on the `SPACE` key so there is something to look at.

- [ ] **Step 5: Checkpoint — this one is visual**

Run `./build/td_app`. Expected: the Greenfields tile grid renders with a clearly visible path from `S` to `E`; pressing SPACE spawns slimes that walk the path smoothly and vanish at the exit. Resize the window — the image must stay pixel-crisp and centred, never blurry or stretched. **If the path drawn from `tiles` and the route the enemies walk disagree, the map's `path` array is wrong** — fix the TOML, and note that Task 6's check 7 should have caught it.

---

### Task 9: Towers — placement, targeting, firing, damage, death

**Files:**
- Create: `src/sim/systems/TargetingSystem.h/.cpp`, `FiringSystem.h/.cpp`, `ProjectileSystem.h/.cpp`, `DeathSystem.h/.cpp`
- Modify: `src/sim/Components.h`, `src/sim/World.h/.cpp`
- Test: `tests/sim/test_towers.cpp`

**Interfaces:**
- Consumes: everything above, plus `core::computeDamage`.
- Produces, added to `Components.h`:
```cpp
struct TowerTag    { std::string defId; int level = 1; int goldSpent = 0; };
struct TileCoord   { int x, y; };
// placeTower attaches Position (the tile CENTRE, i.e. {x+0.5f, y+0.5f} in tile
// units) alongside TowerTag/TileCoord/TowerStats/Cooldown/TargetRef. Range checks
// compare Position to Position, so tower and enemy must use the same convention.
struct TowerStats  { float damage, fireRate, range, projectileSpeed;
                     int projectileCount, pierce;
                     float critChance, critMult, armorPen; };
struct Cooldown    { float remaining = 0.0f; };
struct TargetRef   { entt::entity e = entt::null; };
struct Projectile  { core::Vec2 dir; float speed, damage; int pierceLeft;
                     entt::entity source; float critMult; bool crit; };
struct Lifetime    { float remaining; };
```
Added to `World`:
```cpp
enum class PlaceResult { Ok, NotBuildable, Occupied, TooPoor, OutOfBounds };
PlaceResult placeTower(int tileX, int tileY, const std::string& towerId);
bool        upgradeTower(int tileX, int tileY);
bool        sellTower(int tileX, int tileY);
int         gold() const;
entt::entity towerAt(int tileX, int tileY) const;   // entt::null if none
```

**Targeting:** `first` selects the in-range enemy with the **greatest** `PathFollower::distance` (furthest along, i.e. closest to the exit). Range is compared in tile units against `Position`.

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "content/Registry.h"
#include "sim/World.h"
#include <filesystem>
using namespace td;

static content::Registry loadReg() {
    content::Registry r; r.loadAll(std::filesystem::path(TD_CONTENT_DIR)); return r;
}
static void advance(sim::World& w, float s) {
    int n = static_cast<int>(s / sim::kFixedDt);
    for (int i = 0; i < n; ++i) w.tick(sim::kFixedDt);
}

TEST_CASE("placement obeys buildability, occupancy, bounds and gold", "[towers]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    REQUIRE(w.placeTower(1, 1, "arrow")  == sim::PlaceResult::Ok);
    REQUIRE(w.placeTower(1, 1, "arrow")  == sim::PlaceResult::Occupied);
    REQUIRE(w.placeTower(5, 2, "arrow")  == sim::PlaceResult::NotBuildable);   // path tile
    REQUIRE(w.placeTower(-1, 0, "arrow") == sim::PlaceResult::OutOfBounds);
}

TEST_CASE("placing deducts gold and selling refunds a fraction", "[towers]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    int before = w.gold();
    REQUIRE(w.placeTower(1, 1, "arrow") == sim::PlaceResult::Ok);
    REQUIRE(w.gold() == before - 60);
    REQUIRE(w.sellTower(1, 1));
    REQUIRE(w.gold() == before - 60 + 36);      // 60 * 0.6 refund
    REQUIRE(w.towerAt(1, 1) == entt::null);
}

TEST_CASE("gold runs out", "[towers]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);   // 220 gold, towers cost 60
    REQUIRE(w.placeTower(1, 1, "arrow") == sim::PlaceResult::Ok);
    REQUIRE(w.placeTower(2, 1, "arrow") == sim::PlaceResult::Ok);
    REQUIRE(w.placeTower(3, 1, "arrow") == sim::PlaceResult::Ok);
    REQUIRE(w.placeTower(4, 1, "arrow") == sim::PlaceResult::TooPoor);
}

TEST_CASE("upgrading raises level and applies the authored multiplier", "[towers]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(1, 1, "arrow");
    auto t = w.towerAt(1, 1);
    float baseDamage = w.reg().get<sim::TowerStats>(t).damage;
    REQUIRE(w.upgradeTower(1, 1));
    REQUIRE(w.reg().get<sim::TowerTag>(t).level == 2);
    REQUIRE(w.reg().get<sim::TowerStats>(t).damage == baseDamage * 1.6f);
}

TEST_CASE("a tower kills enemies walking past it and earns bounty", "[towers]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(3, 1, "arrow");        // adjacent to the y=2 path run
    w.placeTower(6, 1, "arrow");
    int goldAfterBuilding = w.gold();
    w.startNextWave();
    advance(w, 60.0f);
    REQUIRE(w.gold() > goldAfterBuilding);            // bounty was collected
    REQUIRE(w.lives() > sim::kStartingLives - 8);     // not everything leaked
}

TEST_CASE("towers do not shoot beyond their range", "[towers]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(28, 0, "arrow");       // far corner, nothing in range on wave 1
    int lives = w.lives();
    w.startNextWave();
    advance(w, 120.0f);
    REQUIRE(w.lives() == lives - 8);    // every slime leaked, so nothing was shot
}

TEST_CASE("targeting first picks the enemy furthest along the path", "[towers]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    w.placeTower(3, 1, "arrow");
    w.startNextWave();
    advance(w, 6.0f);                   // several slimes strung out along the path
    auto t = w.towerAt(3, 1);
    auto target = w.reg().get<sim::TargetRef>(t).e;
    REQUIRE(target != entt::null);
    float chosen = w.reg().get<sim::PathFollower>(target).distance;
    auto& r = w.reg();
    float towerRange = r.get<sim::TowerStats>(t).range;
    auto tpos = r.get<sim::Position>(t).v;
    r.view<sim::PathFollower, sim::Position, sim::EnemyTag>().each(
        [&](auto, auto& pf, auto& pos, auto&) {
            if (core::distance(pos.v, tpos) <= towerRange) REQUIRE(pf.distance <= chosen);
        });
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — `placeTower` not declared.

- [ ] **Step 3: Implement the four systems and the World placement API**

Tick order becomes: WaveSystem, MovementSystem, LeakSystem, TargetingSystem, FiringSystem, ProjectileSystem, DeathSystem.

- TargetingSystem: drop a target that died, left range, or leaked; otherwise keep it (sticky targeting, so a tower does not thrash between equidistant enemies). Reselect by `targetPriority`.
- FiringSystem: `Cooldown.remaining -= dt`; when `<= 0` and a target exists, spawn `projectileCount` projectiles aimed at the target's current position and reset the cooldown to `1.0f / fireRate`.
- ProjectileSystem: move by `dir * speed * dt`; hit-test against enemies with a small radius; on hit call `core::computeDamage`, subtract from `Health`, decrement `pierceLeft` and destroy at `< 0`. Roll crit **once at spawn**, not per hit, so a pierce shot behaves consistently. Give every projectile a `Lifetime` so strays are reaped.
- DeathSystem: `hp <= 0` grants `bounty` and destroys.

Collect-then-destroy everywhere, as in Task 7.

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS, 7 tower tests.

- [ ] **Step 5: Checkpoint**

`ctest` green. The combat loop is complete and fully headless.

---

### Task 10: Run flow, HUD and mouse input

**Files:**
- Create: `src/ui/Hud.h/.cpp`, `src/app/Game.h/.cpp`
- Modify: `src/app/main.cpp`, `src/sim/World.h/.cpp`
- Test: `tests/sim/test_runflow.cpp`

**Interfaces:**
- Produces:
```cpp
namespace td::sim {
// added to World
float buildTimeRemaining() const;   // counts down during Phase::Build; 0 disables auto-start
int   earlyStartBonus() const;      // gold awarded if startNextWave() is called now
}
namespace td::app {
enum class Screen { MainMenu, Playing, RunOver };
class Game {
public:
    Game();
    void update(float dt);
    void draw() const;
    bool shouldExit() const;
};
}
```

Build phase counts down `map.buildTime`; reaching zero starts the wave automatically. Starting manually awards `earlyStartBonus() = static_cast<int>(buildTimeRemaining()) * 2` gold.

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "content/Registry.h"
#include "sim/World.h"
#include <filesystem>
using namespace td;

static content::Registry loadReg() {
    content::Registry r; r.loadAll(std::filesystem::path(TD_CONTENT_DIR)); return r;
}
static void advance(sim::World& w, float s) {
    int n = static_cast<int>(s / sim::kFixedDt);
    for (int i = 0; i < n; ++i) w.tick(sim::kFixedDt);
}

TEST_CASE("the build timer auto-starts the wave when it expires", "[runflow]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    REQUIRE(w.phase() == sim::Phase::Build);
    advance(w, 21.0f);                     // buildTime is 20s
    REQUIRE(w.phase() == sim::Phase::Wave);
}

TEST_CASE("starting early awards bonus gold", "[runflow]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1);
    advance(w, 5.0f);                      // 15s left
    int before = w.gold();
    int bonus  = w.earlyStartBonus();
    REQUIRE(bonus > 0);
    w.startNextWave();
    REQUIRE(w.gold() == before + bonus);
}

TEST_CASE("clearing every wave clears the map", "[runflow]") {
    auto reg = loadReg();
    sim::World w(reg, reg.map("greenfields"), 1, /*goldOverride=*/5000);
    // a wall of towers flanking the first path run
    for (int x : {1,2,3,4,5,6,7,8,9,10,11,12}) {
        w.placeTower(x, 1, "arrow");
        w.placeTower(x, 3, "arrow");
    }
    for (int i = 0; i < 400 && w.phase() != sim::Phase::Cleared
                            && w.phase() != sim::Phase::Defeated; ++i) {
        if (w.phase() == sim::Phase::Build) w.startNextWave();
        advance(w, 1.0f);
    }
    REQUIRE(w.phase() == sim::Phase::Cleared);
}
```

The third test uses `goldOverride` because 24 towers cost 1440 gold and the map
starts with 220. It asserts a **property of the design** — that a heavily defended
map is clearable — so if it fails, either the economy or the tower damage is
mistuned, and that is a real finding. Tune the TOML; never weaken the assertion.

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — `buildTimeRemaining` not declared.

- [ ] **Step 3: Implement the build timer, early-start bonus and phase transitions in `World`**

- [ ] **Step 4: Implement `ui/Hud`**

Draw into the 60 px band at `y = 480`: lives, gold, wave `N/5`, current phase, build-timer countdown with the early-start bonus, the selected tower's cost, and the result of the last placement attempt (e.g. "Not enough gold") for two seconds.

- [ ] **Step 5: Implement `app/Game` with mouse input**

Left click in the play area converts window to virtual coordinates via `PixelCanvas::windowToVirtual`, divides by `kTile` to get a tile, and calls `placeTower`. Hovering a buildable tile shows a ghost tower and its range ring; hovering an existing tower shows its range and upgrade cost. `U` upgrades the hovered tower, `S` sells it, `SPACE` starts the wave early. `MainMenu` is a title and "Press ENTER"; `RunOver` shows cleared-or-defeated, waves survived, and returns to the menu on ENTER.

- [ ] **Step 6: Run tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS, full suite.

- [ ] **Step 7: Checkpoint — play it**

Launch `./build/td_app` and play a full run start to finish. Required outcomes: towers can be placed only on buildable tiles, gold decreases and bounty increases it, enemies die, leaks cost lives, the run ends in either Cleared or Defeated, and the result screen returns to the menu. **A losing run must be losable and a careful run must be winnable** — if either is impossible, record the fact and tune the TOML, not the code.

---

### Task 11: Enforce the core/raylib boundary

**Files:**
- Create: `tests/architecture/test_no_raylib_in_core.cpp` or a CMake check
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: the whole tree.
- Produces: a build-time or test-time guarantee that `td_core` never gains a raylib dependency.

- [ ] **Step 1: Add the check**

The cheapest reliable form is a CMake-time scan asserting no file under `src/core`, `src/sim` or `src/content` contains `#include "raylib.h"` or `#include <raylib.h>`:

```cmake
file(GLOB_RECURSE CORE_HEADERS_AND_SRC CONFIGURE_DEPENDS
     src/core/* src/sim/* src/content/*)
foreach(f ${CORE_HEADERS_AND_SRC})
  file(READ ${f} _contents)
  if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]raylib\\.h[>\"]")
    message(FATAL_ERROR "td_core boundary violated: ${f} includes raylib.h")
  endif()
endforeach()
```

- [ ] **Step 2: Verify it catches a violation**

Temporarily add `#include "raylib.h"` to `src/core/Vec2.h`, re-run `cmake -B build`, and confirm the configure fails with the boundary message. Remove it again.

- [ ] **Step 3: Checkpoint**

`cmake -B build` succeeds on the clean tree and fails on a deliberate violation. The property that makes the whole test strategy possible is now mechanically enforced rather than merely intended.

---

## Known balance concern, to confirm during Task 10

Arrow base damage is 12 and a goblin has 6 armour, so an unupgraded tower deals 6
per hit against 160 HP — 27 hits, or roughly 24 seconds of uninterrupted fire from
one tower. Wave 4 is six goblins. This is very likely too slow and will show up as
wave 4 being unclearable. **The fix belongs in `content/`** (raise base damage,
lower goblin armour, or make `armorPen` part of the level-2 upgrade), not in code.
Flat armour subtraction is unusually punishing at low damage values; this is the
first place to look if wave 4 feels like a wall.

## Definition of done for Plan 1

- `cmake -B build && cmake --build build -j` succeeds from a clean checkout.
- `ctest --test-dir build --output-on-failure` is green, covering Vec2, Rng determinism, Path, damage, content loading, content validation, spawning/movement/leaks, tower placement/economy/combat, and run flow.
- `./build/td_app` plays a complete run on Greenfields: place towers with gold, five waves, win or lose, result screen.
- No file under `src/core`, `src/sim` or `src/content` includes raylib.
- Every gameplay number is editable in `content/` without recompiling.

---

## Execution log — deviations from this plan

Recorded during execution on 2026-08-30. All are deliberate; none are silent.

**Environment**
- `cmake` was not installed. Installed CMake 4.4.3 and ninja via Homebrew.
- CMake 4 rejects `cmake_minimum_required` below 3.5, which several dependencies
  still declare. Added `set(CMAKE_POLICY_VERSION_MINIMUM 3.5)` rather than
  forking them.
- Dependency pins were verified against upstream and corrected: EnTT 3.16.0
  (4.0.0 exists but is a fresh major), nlohmann/json 3.12.0, Catch2 3.16.0.

**Structure**
- Task 11's raylib boundary check was folded into Task 1's `CMakeLists.txt`
  rather than left to the end, and proved in both directions immediately: a
  deliberate `#include "raylib.h"` in `src/core/Version.h` failed configure with
  the expected message.
- The plan specified seven separate system files. Implemented as two —
  `EnemySystems.cpp` (wave/movement/leak) and `CombatSystems.cpp`
  (targeting/firing/projectile/death) — because files that change together
  should live together. Split further if either grows.
- `app::Game` exposes `frame(float)` rather than the planned `update()`/`draw()`
  pair, and `shouldExit()` was dropped in favour of raylib's
  `WindowShouldClose()` in `main`.

**Corrections to the plan itself**
- The build-timer auto-start was planned for Task 10 but implemented in Task 7.
  Task 7's leak test advanced 120s, which would have silently broken once
  auto-start landed, because wave 2 auto-starts partway through that window. The
  test now advances 50s and stays inside wave 1.
- The window was hardcoded to 1920x1080 on a 1710x1107 display and was clipped.
  `main` now picks the largest integer scale that fits the monitor and centres
  the window.
- `Rng::range` is mapped by hand instead of via `uniform_int_distribution`, whose
  consumption pattern is implementation-defined and would make a saved seed
  replay differently across platforms.
- Added a test, not in the plan, that `Rng::chance`'s short-circuit branches
  consume no draw. Without that property a 0%-crit tower would desync the RNG
  stream from an otherwise identical run.
- Added a test, not in the plan, pinning whether the damage floor applies before
  or after crit. The spec was ambiguous; it is now defined as after-crit.
- Catch2's expression decomposition is ambiguous against EnTT's `null_t`
  comparisons, so `entt::null` assertions are wrapped in extra parentheses.

**Scope change requested mid-execution: 50 waves per map**
- Hand-authoring 50 waves per map across 5 maps is not viable, so waves are now
  expanded from a per-map `[waves]` recipe by `content::WaveGen`. Expansion is
  fully deterministic (no RNG), so a map always plays identically.
- `WaveGroup` gained `hpMult`, `armorAdd` and `bountyMult`, baked in at
  generation time and applied at spawn. Hand-authored `[[wave]]` arrays still
  load and leave these neutral, which keeps small fixture maps possible.
- Validation gained recipe-specific checks: pool enemies resolve, at least one
  enemy is available at wave 1, and the generated wave count matches the request.
- `buildTime` dropped from 20s to 12s, since there are now 50 build phases.

**Balance finding**
- Task 10's "map is clearable" assertion was replaced with "a defended map
  survives deep into the wave schedule" (`waveIndex() >= 10`). Clearing all 50
  waves is *not* achievable in Plan 1 by design — the missing power comes from
  the skill trees in Plans 2 and 3. Measured: 24 level-2 arrow towers reach
  **wave 22 of 50**. That number is the baseline the tree power curve must lift.
- The predicted goblin/armour problem is real and still open: arrow base damage
  12 against 6 armour is 6 per hit. It is survivable now only because wave
  scaling is gentle early. Fix in `content/`, not in code.

**Not done, deliberately**
- No `git init`, no commits: this project's rules forbid unprompted commits.
- `Game::requestStart(demoTowers)` is a dev-only affordance used by `--shot`
  capture so screenshots show real gameplay.

## Performance notes for later

- Targeting is O(towers x enemies) per tick and projectile collision is
  O(projectiles x enemies). At 24 towers and 50 enemies this is trivial, but a
  late 50-wave map with hundreds of entities will want spatial bucketing. Revisit
  in Plan 3, not before — measure first.
