cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /FS")

project(silicate VERSION 1.0.0)

set(ARC_FEATURE_FULL ON CACHE BOOL "" FORCE)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp

    src/bot/bot.cpp
    src/bot/updater.cpp
    src/bot/scheduler.cpp

    src/frame_window/analyzer.cpp

    src/assist/autoclicker.cpp
    src/assist/hitboxes.cpp
    src/assist/cps.cpp

    src/hooks/CCActionManager.cpp
    src/hooks/CCDirector.cpp
    src/hooks/CCEGLView.cpp
    src/hooks/CCScheduler.cpp
    src/hooks/EnhancedGameObject.cpp
    src/hooks/FMODAudioEngine.cpp
    src/hooks/GJBaseGameLayer.cpp
    src/hooks/GameObject.cpp
    src/hooks/LevelEditorLayer.cpp
    src/hooks/PauseLayer.cpp
    src/hooks/PlayLayer.cpp
    src/hooks/PlayerObject.cpp
    src/hooks/RandTriggerGameObject.cpp
    src/hooks/ShaderLayer.cpp
    src/hooks/VideoOptionsLayer.cpp

    src/ui/manager.cpp
    src/ui/hook.cpp
    src/ui/theme.cpp

    src/replay/replay.cpp
    src/replay/system.cpp

    src/physics/collisions.cpp
    src/physics/gjbasegamelayer.cpp
    src/physics/gravity.cpp
    src/physics/object.cpp
    src/physics/player.cpp

    src/label/label.cpp

    src/checkpoint/fix.cpp
    src/checkpoint/checkpoint.cpp

    src/trajectory/trajectory.cpp

    src/render/renderer.cpp
    src/render/texture.cpp
    src/render/dsp.cpp
    src/render/pass.cpp

    src/util/midhook.cpp
    src/hooks/GJEffectManager.cpp
)

set(FW_GIT_SHA "unknown" CACHE STRING "Git commit embedded in Frame Window diagnostics")
target_compile_definitions(${PROJECT_NAME} PRIVATE SILICATE_FW_GIT_SHA="${FW_GIT_SHA}")

# Silicate upstream used /W4 unconditionally. Current Geode Win64 CI uses
# clang++ with the GNU-style command-line frontend, where /W4 is parsed as a
# file path and fails before any project source is compiled. Keep the upstream
# warning level only when CMake is actually driving MSVC. Geode supplies its
# own Clang warning flags for the Win64 toolchain.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE src include)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

CPMAddPackage("gh:silicate-bot/tabby#legacy-v1")
set(TABBY_USE_WIN32 ON)
set(TABBY_USE_OPENGL3 ON)
add_compile_definitions(TABBY_USE_WIN32)
add_compile_definitions(TABBY_USE_OPENGL3)
target_link_libraries(${PROJECT_NAME} tabby)

CPMAddPackage("gh:stephenberry/glaze#main")
target_link_libraries(${PROJECT_NAME} glaze::glaze)

CPMAddPackage("gh:zyantific/zydis#bffbb61")
target_link_libraries(${PROJECT_NAME} Zydis)

CPMAddPackage("gh:cursey/safetyhook#2f28386")
target_link_libraries(${PROJECT_NAME} safetyhook)

CPMAddPackage("https://git.puppy.lgbt/silicate/slc.git#main")

target_link_libraries(${PROJECT_NAME} libslc)
target_compile_options(GeodeBindings PRIVATE -Wno-overloaded-virtual -Wno-unused-variable)

setup_geode_mod(${PROJECT_NAME})

# Geode intentionally exposes convenience macros such as $on_mod and $execute.
# They are valid and documented Geode API, but clang++ in GNU driver mode emits
# -Wdollar-in-identifier-extension for the '$' token. The Win64 Geode toolchain
# promotes warnings to errors, so suppress only this extension diagnostic for
# Clang instead of changing Silicate/Geode source or weakening -Werror globally.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Geode enables -Wpedantic later through interface compile options. That
    # can re-enable -Wdollar-in-identifier-extension even if a preceding
    # -Wno-dollar-in-identifier-extension is present. Keep the diagnostic
    # visible, but make this one documented Geode extension exempt from the
    # project's global -Werror. The warning-specific no-error setting is more
    # specific than -Werror and therefore survives the final option ordering.
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wno-error=dollar-in-identifier-extension
    )
endif()
