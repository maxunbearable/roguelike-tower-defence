cmake_minimum_required(VERSION 3.24)
project(td LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# A plain `cmake -B build` is what the README tells a player to run, so it has to
# produce a binary worth playing: optimised, with NDEBUG set. Debug remains one
# flag away (-DCMAKE_BUILD_TYPE=Debug) and RelWithDebInfo keeps symbols, so
# nothing is lost for development.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Several dependencies still declare cmake_minimum_required below 3.5, which
# CMake 4 rejects outright. This floors them instead of forking each one.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(MSVC)
  add_compile_options(/utf-8)
endif()

include(cmake/dependencies.cmake)

# --- Architecture boundary ------------------------------------------------
# td_core holds all game logic and must stay headlessly testable, which means
# it must never gain a raylib dependency. Enforced at configure time.
file(GLOB_RECURSE TD_CORE_TREE CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/src/core/*
     ${CMAKE_SOURCE_DIR}/src/sim/*
     ${CMAKE_SOURCE_DIR}/src/content/*)
foreach(f ${TD_CORE_TREE})
  file(READ ${f} _contents)
  if(_contents MATCHES "#[ \t]*include[ \t]*[<\"]raylib\\.h[>\"]")
    message(FATAL_ERROR "td_core boundary violated: ${f} includes raylib.h")
  endif()
endforeach()

# --- td_core : game logic, MUST NOT link raylib ---------------------------
file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS
     src/core/*.cpp src/sim/*.cpp src/content/*.cpp)
add_library(td_core STATIC ${CORE_SRC})
target_include_directories(td_core PUBLIC src)
# Last-resort resource root, so a build run in place from the source tree works
# without an install step. Every packaged layout is preferred over it.
target_compile_definitions(td_core PUBLIC TD_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(td_core PUBLIC
    EnTT::EnTT
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json)

# --- td_app : executable --------------------------------------------------
file(GLOB_RECURSE APP_SRC CONFIGURE_DEPENDS
     src/render/*.cpp src/ui/*.cpp src/app/*.cpp src/audio/*.cpp)
# The .rc gives the Windows executable its icon; elsewhere it is ignored.
if(WIN32)
  add_executable(td_app ${APP_SRC} packaging/app.rc)
else()
  add_executable(td_app ${APP_SRC})
endif()
target_link_libraries(td_app PRIVATE td_core raylib)
target_compile_definitions(td_app PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    TD_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# --- td_shot : the same game, rendered without a display -------------------
# Eleven rounds shipped UI changes that could not be looked at, because macOS
# denies a locked screen's processes an active display: CoreGraphics reports the
# monitor online but not active, GLFW then enumerates zero monitors, and raylib
# aborts InitWindow -- purely because it cannot decide where to centre a window.
# raylib ships a software rasteriser (rlsw) and a display-free backend for
# exactly this, so the capture tool is the identical game code linked against
# raylib built on PLATFORM_MEMORY. Verification no longer depends on the screen.
add_library(raylib_sw STATIC
    ${raylib_SOURCE_DIR}/src/rcore.c
    ${raylib_SOURCE_DIR}/src/rshapes.c
    ${raylib_SOURCE_DIR}/src/rtext.c
    ${raylib_SOURCE_DIR}/src/rtextures.c
    ${raylib_SOURCE_DIR}/src/rmodels.c
    ${raylib_SOURCE_DIR}/src/raudio.c)
target_include_directories(raylib_sw PUBLIC ${raylib_SOURCE_DIR}/src)
# rlsw's texture pool defaults to 128 entries and the game loads well over
# three hundred, so every texture past the 128th silently failed and its art
# rendered as nothing -- which is why the terrain and every UI panel came back
# black on the first capture. The pool holds descriptors, not pixels, so a
# generous ceiling costs almost nothing.
target_compile_definitions(raylib_sw PUBLIC PLATFORM_MEMORY GRAPHICS_API_OPENGL_SOFTWARE
                           SW_MAX_TEXTURES=1024)
if(APPLE)
  target_link_libraries(raylib_sw PUBLIC "-framework CoreAudio" "-framework AudioToolbox")
elseif(WIN32)
  target_link_libraries(raylib_sw PUBLIC winmm)
endif()

add_executable(td_shot ${APP_SRC})
target_link_libraries(td_shot PRIVATE td_core raylib_sw)
target_compile_definitions(td_shot PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    TD_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    TD_HEADLESS=1)

# --- header self-containment ----------------------------------------------
# One TU per header, so a header that leans on its includers fails here rather
# than on whichever toolchain happens not to supply the include transitively.
# WaveGen.h used uint64_t without <cstdint>: fine under libc++, red on Linux.
file(GLOB_RECURSE TD_HEADERS CONFIGURE_DEPENDS src/*.h)
set(TD_HDR_DIR "${CMAKE_BINARY_DIR}/header_check")
file(MAKE_DIRECTORY "${TD_HDR_DIR}")
set(TD_HDR_SRC "")
foreach(h ${TD_HEADERS})
  file(RELATIVE_PATH rel "${CMAKE_SOURCE_DIR}/src" "${h}")
  string(REPLACE "/" "_" flat "${rel}")
  string(REPLACE ".h" "" flat "${flat}")
  set(gen "${TD_HDR_DIR}/hdr_${flat}.cpp")
  file(WRITE "${gen}" "#include \"${rel}\"\n")
  list(APPEND TD_HDR_SRC "${gen}")
endforeach()
add_library(td_headers OBJECT EXCLUDE_FROM_ALL ${TD_HDR_SRC})
target_link_libraries(td_headers PRIVATE td_core raylib)
target_include_directories(td_headers PRIVATE src)
target_compile_definitions(td_headers PRIVATE
    TD_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content"
    TD_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# --- packaging -------------------------------------------------------------
# Resources sit beside the executable (or in Contents/Resources on macOS), which
# is what core::resourceRoot looks for.
set(TD_VERSION 1.0.0)

if(APPLE)
  set_target_properties(td_app PROPERTIES
      MACOSX_BUNDLE TRUE
      OUTPUT_NAME "Wardstone"
      MACOSX_BUNDLE_BUNDLE_NAME "Wardstone"
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.wardstone.game"
      MACOSX_BUNDLE_BUNDLE_VERSION "${TD_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${TD_VERSION}"
      MACOSX_BUNDLE_COPYRIGHT "zlib/MIT/CC0 third-party assets; see ASSET-POLICY.md"
      MACOSX_BUNDLE_ICON_FILE "Wardstone.icns")
  # The icns has to be a source of the target with a package location, or the
  # bundle gets a CFBundleIconFile pointing at a file that is not there.
  target_sources(td_app PRIVATE "${CMAKE_SOURCE_DIR}/packaging/Wardstone.icns")
  set_source_files_properties("${CMAKE_SOURCE_DIR}/packaging/Wardstone.icns"
      PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  install(TARGETS td_app BUNDLE DESTINATION .)
  install(DIRECTORY content assets DESTINATION "Wardstone.app/Contents/Resources")
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "Wardstone")
  set(CPACK_DMG_VOLUME_ICON "${CMAKE_SOURCE_DIR}/packaging/Wardstone.icns")
else()
  install(TARGETS td_app RUNTIME DESTINATION .)
  install(DIRECTORY content assets DESTINATION .)
  if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_PACKAGE_NAME "Wardstone")
    set(CPACK_NSIS_DISPLAY_NAME "Wardstone")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_PACKAGE_EXECUTABLES "td_app;Wardstone")
    set(CPACK_CREATE_DESKTOP_LINKS "td_app")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/packaging/Wardstone.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/packaging/Wardstone.ico")
  else()
    set(CPACK_GENERATOR "TGZ")
  endif()
endif()

set(CPACK_PACKAGE_NAME "Wardstone")
set(CPACK_PACKAGE_VENDOR "Wardstone")
set(CPACK_PACKAGE_VERSION "${TD_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A pixel-art roguelike tower defence")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Wardstone")
set(CPACK_PACKAGE_FILE_NAME "Wardstone-${TD_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md")
include(CPack)

enable_testing()
add_subdirectory(tests)
add_subdirectory(uitests)
