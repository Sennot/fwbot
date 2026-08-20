#ifndef UI_STATE_HPP
#define UI_STATE_HPP

#include <string>
#include <string_view>
#include <tabby.hpp>
#include <unordered_map>
#include <vector>

#include "../settings/settings.hpp"
#include "../shared/value/value.hpp"

struct UIState {
    enum class UITab {
        Record,
        Assist,
        Prediction,
        Edit,
        FrameWindow,
        Render,
        Scripts,
        Keybinds,
        Settings
    };

    SLValuePtr<bool> m_visible =
        SLValue<bool>::create("ui.visible", &SLSettings::get()->uiVisible);
    SLValuePtr<bool> m_useShader =
        SLValue<bool>::create("ui.glass_ui", &SLSettings::get()->glassUi);
    SLValuePtr<float> m_uiScale = SLValue<float>::create(
        "ui._______ui_scale",
        &SLSettings::get()->uiScale);  // 7 underscores because SHOULDN'T HAVE
                                       // KEYBINDS BOUND TO THIS
    SLValuePtr<bool> m_rainbow =
        SLValue<bool>::create("ui.rainbow", &SLSettings::get()->rainbowMode);
    SLValuePtr<bool> m_playAnimations = SLValue<bool>::create(
        "ui.play_animations", &SLSettings::get()->playAnimations);
    SLValuePtr<float> m_animationSpeed = SLValue<float>::create(
        "ui.animation_speed", &SLSettings::get()->animationSpeed);
    SLValuePtr<float> m_opacity =
        SLValue<float>::create("ui.opacity", &SLSettings::get()->uiOpacity);

    UITab m_currentTab = UITab::Record;
    std::vector<std::string> m_replayNames;
    std::vector<std::string> m_presetNames;
    std::vector<std::string> m_scriptNames;

    bool m_restartGameInfo = false;
    bool m_showExperimentalFeatures = false;

    double m_fps = 240.0;
    double m_speed = 1.0;
    double m_tps = 240.0;

    int m_codec = 0;
    double m_bitrate = 30.0;

    int m_editIndex = 0;

    std::string m_replayName = "";
    std::string m_lastReplayName = "";

    std::string m_presetName = "";
    tabby::AutocompleteState m_presetAutocomplete;

    std::string m_scriptName = "";
    tabby::AutocompleteState m_scriptAutocomplete;

    using Mode = SLSettings::TrajectorySettings::Mode;
    int m_trajectoryMode = Mode::Hold;
    tabby::DropdownState m_trajectoryState = {
        .options =
            {
                "Hold",
                "Swift",
                "Release",

                "Hold Stationary",
                "Swift Stationary",
                "Release Stationary",

                "Hold Left",
                "Swift Left",
                "Release Left",

                "Hold Right",
                "Swift Right",
                "Release Right",

                "Hold Follow",
                "Swift Follow",
                "Release Follow",

                "Hold Opposite",
                "Swift Opposite",
                "Release Opposite",
            },
    };
    int m_categories[18] = {
        Mode::Hold,
        Mode::Swift,
        Mode::Release,

        Mode::Hold | Mode::Platformer,
        Mode::Swift | Mode::Platformer,
        Mode::Release | Mode::Platformer,

        Mode::Hold | Mode::Left | Mode::Platformer,
        Mode::Swift | Mode::Left | Mode::Platformer,
        Mode::Release | Mode::Left | Mode::Platformer,

        Mode::Hold | Mode::Right | Mode::Platformer,
        Mode::Swift | Mode::Right | Mode::Platformer,
        Mode::Release | Mode::Right | Mode::Platformer,

        Mode::Hold | Mode::FollowPlayer | Mode::Platformer,
        Mode::Swift | Mode::FollowPlayer | Mode::Platformer,
        Mode::Release | Mode::FollowPlayer | Mode::Platformer,

        Mode::Hold | Mode::FollowOpposite | Mode::Platformer,
        Mode::Swift | Mode::FollowOpposite | Mode::Platformer,
        Mode::Release | Mode::FollowOpposite | Mode::Platformer,
    };

    tabby::ColorState m_trajectoryColorState;

    tabby::DropdownState m_lockDeltaState = {
        .options = {"Performance", "Accuracy"},
    };

    tabby::DropdownState m_labelState = {};
    tabby::DropdownState m_labelPositionsState = {
        .options =
            {
                "Top Left",
                "Top Right",
                "Bottom Left",
                "Bottom Right",
            },
    };

    tabby::DropdownState m_labelFontsState = {
        .options =
            {
                "Big",
                "Regular",
            },
    };

    tabby::DropdownState m_autoclickerState = {
        .options =
            {
                "Player 1",
                "Player 2",
                "Both",
            },
    };

    tabby::DropdownState m_themeState;

    tabby::DropdownState m_noclipState = {
        .options =
            {
                "Player 1",
                "Player 2",
                "Both",
            },
    };

    using HitboxType = SLSettings::HitboxSettings::Type;
    tabby::DropdownState m_hitboxState = {
        .options = {"Player", "Inner Player", "Rotated Player",
                    "Player (Circle)",

                    "Solid", "Hazard", "Passable", "Interactable",
                    "Interactable (Active)"},
    };
    int m_hitboxCategories[9] = {HitboxType::Player,
                                 HitboxType::PlayerInner,
                                 HitboxType::PlayerRotated,
                                 HitboxType::PlayerCircle,

                                 HitboxType::Solid,
                                 HitboxType::Hazard,
                                 HitboxType::Passable,
                                 HitboxType::Interactable,
                                 HitboxType::InteractableActive};

    tabby::ColorState m_hitboxColorState;
    tabby::ColorState m_groundColorState;
    tabby::ColorState m_bgColorState;

    struct KeybindEdit {
        std::string tag;
        std::string value;
        uint64_t seen = 0;
    };

    std::unordered_map<KeybindControl*, KeybindEdit> m_keybindEdits;
    KeybindControl* m_editedKeybindTag = nullptr;
    KeybindControl* m_editedKeybindValue = nullptr;
    uint64_t m_keybindEditGeneration = 0;

    // Retagging destroys the control being drawn, so it can't happen while the
    // keybind list is still being iterated. Deferred to the end of the tab.
    KeybindControl* m_pendingRetag = nullptr;
    std::string m_pendingRetagTag;

    void toggle() { m_visible->inner() = !m_visible->inner(); }
};

#endif  // UI_STATE_HPP
