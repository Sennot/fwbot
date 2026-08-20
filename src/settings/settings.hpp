#pragma once

#include <Geode/Geode.hpp>
#include <fstream>
#include <glaze/glaze.hpp>
#include <string>
#include <unordered_map>

#include "../shared/value/value.hpp"

// Global settings class for Silicate.
class SLSettings {
   public:
    bool botEnabled = true;

    bool uiVisible = false;

    bool glassUi = true;
    float animationSpeed = 1.0f;
    bool playAnimations = true;
    float uiScale = 1.0f;
    float uiOpacity = 0.66f;
    int theme = 0;

    bool autosaveAtLevelEnd = true;
    bool autosaveAtInterval = true;
    double autosaveInterval = 180.0;

    bool autoExitLevel = false;

    bool rainbowMode = true;

    double tps = 240.0;
    double speed = 1.0;

    bool realTime = true;
    uint32_t maxUpr = 10;
    bool useVisualUpdates = true;
    bool dynamicUpr = true;
    double fpsTarget = 60.0;
    bool lockDelta = true;
    int lockDeltaMode = 0;

    bool fullGamePrediction = false;
    float acceptablePrediction = 0.45;

    bool speedhackAudio = true;
    bool blockInputs = true;
    bool useRegularBg = false;

    bool automaticVideoName = true;
    bool previewAudio = true;
    std::string videoNameTemplate = "%name%_%rand%";
    std::string lastLoadedPreset = "";
    bool scrollSpeedBugFix = false;

    bool autoclickerEnabled = false;
    bool showLabelsWhileRendering = false;

    bool useAlternateHook = false;

    bool backwardsStepping = false;
    uint32_t stepsToSave = 120;

    std::array<float, 4> layoutBgColor = {0.2828, 0.4901, 1.0, 1.0};
    std::array<float, 4> layoutGroundColor = {0.2828, 0.4901, 1.0, 1.0};

    struct TrajectorySettings {
        struct State {
            bool enabled = false;
            std::array<float, 4> colors = {0.0, 1.0, 0.0, 1.0};
        };

        enum Mode {
            Hold = 0x1,
            Swift = 0x2,
            Release = 0x4,

            Left = 0x8,
            Right = 0x10,

            Player1 = 0x20,
            Player2 = 0x40,

            FollowPlayer = 0x80,
            FollowOpposite = 0x100,

            Platformer = 0x200,
        };

        bool enabled = false;
        double width = 0.5;
        double length = 1.0;

        std::unordered_map<int, State> categories = {
            {Mode::Hold, {true}},
            {Mode::Swift, {}},
            {Mode::Release, {true}},

            {Mode::Hold | Mode::Platformer, {}},
            {Mode::Swift | Mode::Platformer, {}},
            {Mode::Release | Mode::Platformer, {}},

            {Mode::Hold | Mode::Left | Mode::Platformer, {}},
            {Mode::Swift | Mode::Left | Mode::Platformer, {}},
            {Mode::Release | Mode::Left | Mode::Platformer, {}},

            {Mode::Hold | Mode::Right | Mode::Platformer, {}},
            {Mode::Swift | Mode::Right | Mode::Platformer, {}},
            {Mode::Release | Mode::Right | Mode::Platformer, {}},

            {Mode::Hold | Mode::FollowPlayer | Mode::Platformer, {true}},
            {Mode::Swift | Mode::FollowPlayer | Mode::Platformer, {}},
            {Mode::Release | Mode::FollowPlayer | Mode::Platformer, {true}},

            {Mode::Hold | Mode::FollowOpposite | Mode::Platformer, {}},
            {Mode::Swift | Mode::FollowOpposite | Mode::Platformer, {}},
            {Mode::Release | Mode::FollowOpposite | Mode::Platformer, {}},
        };
    } trajectory;

    struct HitboxSettings {
        double width = 0.5;
        bool trailEnabled = false;
        bool playerAbove = true;
        bool rotatedAbove = true;

        enum Type {
            Player,
            PlayerRotated,
            PlayerInner,
            PlayerCircle,

            Solid,
            Hazard,
            Passable,
            Interactable,
            InteractableActive
        };

        struct HBState {
            bool enabled = true;
            double fillOpacity = 0.00;
            std::array<float, 4> colors;
        };

        std::unordered_map<int, HBState> categories = {
            {Type::Player, {true, 0.0, {1.0, 0.0, 0.0, 1.0}}},
            {Type::PlayerInner, {true, 0.0, {0.0, 0.0, 1.0, 1.0}}},
            {Type::PlayerRotated, {true, 0.0, {0.5, 0.0, 0.0, 1.0}}},
            {Type::PlayerCircle, {true, 0.0, {1.0, 0.0, 0.0, 1.0}}},

            {Type::Solid, {true, 0.0, {0.0, 0.0, 1.0, 1.0}}},
            {Type::Hazard, {true, 0.0, {1.0, 0.0, 0.0, 1.0}}},
            {Type::Passable, {true, 0.0, {0.0, 1.0, 1.0, 1.0}}},
            {Type::Interactable, {true, 0.0, {1.0, 1.0, 0.0, 1.0}}},
            {Type::InteractableActive, {true, 0.0, {0.2, 1.0, 0.0, 1.0}}},
        };
    } hitboxes;

    static SLSettings* get() {
        static SLSettings instance;
        return &instance;
    }
};

// clang-format off
template <>
struct glz::meta<SLSettings> {
    using T = SLSettings;
    static constexpr auto value = object(
        "global_enabled", &T::botEnabled,
        "ui_visible", hide{&T::uiVisible},
        "glass_ui", &T::glassUi,
        "ui_scale", &T::uiScale,
        "ui_opacity", &T::uiOpacity,
        "animation_speed", &T::animationSpeed,
        "play_animations", &T::playAnimations,
        "pride_mode", &T::rainbowMode,
        "theme", &T::theme,
        "tps", hide{&T::tps},
        "speed", hide{&T::speed},
        "real_time", &T::realTime,
        "max_upr", &T::maxUpr,
        "target_fps", &T::fpsTarget,
        "dynamic_upr", &T::dynamicUpr,
        "trajectory", &T::trajectory,
        "hitboxes", &T::hitboxes,
        "speedhack_audio", &T::speedhackAudio,
        "block_inputs", &T::blockInputs,
        "use_visual_updates", &T::useVisualUpdates,
        "lock_delta", &T::lockDelta,
        "auto_video_name", &T::automaticVideoName,
        "video_name_template", &T::videoNameTemplate,
        "preset", &T::lastLoadedPreset,
        "preview_audio", &T::previewAudio,
        "auto_exit_level", &T::autoExitLevel,
        "autoclicker_enabled", hide{&T::autoclickerEnabled},

        "ssb_fix", &T::scrollSpeedBugFix,

        "backwards_stepping", &T::backwardsStepping,
        "steps_saved", &T::stepsToSave,

        "autosave_at_level_end", &T::autosaveAtLevelEnd,
        "autosave_at_interval", &T::autosaveAtInterval,
        "autosave_interval", &T::autosaveInterval,

        "acceptable_prediction", &T::acceptablePrediction,

        "layout_bg", &T::layoutBgColor,
        "layout_ground", &T::layoutGroundColor,
        "layout_use_bg", &T::useRegularBg,

        "show_labels_while_rendering", &T::showLabelsWhileRendering,

        "alternate_hook", &T::useAlternateHook
    );
};
// clang-format on

$on_mod(DataSaved) {
    std::filesystem::path settingsPath =
        geode::Mod::get()->getConfigDir(true) / "settings.json";
    std::ofstream settingsFd(settingsPath);

    auto settings = SLSettings::get();
    std::string serialized =
        glz::write<glz::opts{.prettify = true}>(*settings).value_or(
            std::string{});
    settingsFd << serialized;

    std::filesystem::path keybindsPath =
        geode::Mod::get()->getConfigDir(true) / "keybinds.json";
    SLBindingManager::get()->writeToFile(keybindsPath);
}
