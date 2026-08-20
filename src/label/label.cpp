#include "label.hpp"

#include <Geode/binding/GJBaseGameLayer.hpp>
#include <glaze/glaze.hpp>

#include "assist/cps.hpp"
#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "ccTypes.h"
#include "checkpoint/fix.hpp"
#include "replay/system.hpp"

using namespace geode::prelude;

#define GET_PLAYER_VAR(display, var, precision)                             \
    {                                                                       \
        auto pl = PlayLayer::get();                                         \
        auto player = pl->m_player1;                                        \
        auto player2 = pl->m_player2;                                       \
        if (pl->m_gameState.m_isDualMode && player2) {                      \
            return fmt::format("{}: {:." #precision "f} / {:." #precision   \
                               "f}",                                        \
                               display, player->m_##var, player2->m_##var); \
        } else {                                                            \
            return fmt::format("{}: {:." #precision "f}", display,          \
                               player->m_##var);                            \
        }                                                                   \
    }

static std::string getStringifiedOrbName(RingObject* ring) {
    switch (ring->m_objectID) {
        case 36:
            return "Yellow";
        case 84:
            return "Blue";
        case 141:
            return "Pink";
        case 1022:
            return "Green";
        case 1330:
            return "Black";
        case 1333:
            return "Red";
        case 1594:
            return "Toggle";
        case 1704:
            return "Green Dash";
        case 1751:
            return "Pink Dash";
        case 3004:
            return "Spider";
        case 3027:
            return "Teleport";
        default:
            return "Unknown";
    }
}

LabelManager::LabelManager() {
    addLabel("frame", "Tick",
             [](Label&) {
                 auto& updater = Bot::get()->updater();
                 return fmt::format("Tick: {}", updater.getFrame());
             },
             {});

    addLabel("internal_frame", "Internal Game Tick",
             [](Label&) {
                 return fmt::format(
                     "Game tick: {}",
                     PlayLayer::get()->m_gameState.m_currentProgress);
             },
             {});

    addLabel("tps", "TPS",
             [](Label&) {
                 return fmt::format("TPS: {}", Bot::get()->updater().getTps());
             },
             {});

    addLabel("player_x", "Player X",
             [](Label&) { GET_PLAYER_VAR("X", position.x, 6) }, {});
    addLabel("player_y", "Player Y",
             [](Label&) { GET_PLAYER_VAR("Y", position.y, 6) }, {});
    addLabel(
        "player_xvel", "Player X Velocity",
        [](Label&) { GET_PLAYER_VAR("X Velocity", platformerXVelocity, 6) },
        {});

    addLabel(
        "player_yvel", "Player Y Velocity",
        [](Label&) {
            auto pl = PlayLayer::get();
            auto player = pl->m_player1;
            auto player2 = pl->m_player2;

            static bool isVelocityUnroundingLoaded =
                Loader::get()->isModLoaded("chizz.velocity-unrounding");

            if (isVelocityUnroundingLoaded) {
                if (pl->m_gameState.m_isDualMode && player2) {
                    return fmt::format(
                        "Y Velocity: {:.8f} / {:.8f} (Unrounded)",
                        player->m_yVelocity, player2->m_yVelocity);
                } else {
                    return fmt::format("Y Velocity: {:.8f} (Unrounded)",
                                       player->m_yVelocity);
                }
            }

            if (pl->m_gameState.m_isDualMode && player2) {
                return fmt::format("Y Velocity: {:.3f} / {:.3f}",
                                   player->m_yVelocity, player2->m_yVelocity);
            } else {
                return fmt::format("Y Velocity: {:.3f}", player->m_yVelocity);
            }
        },
        {});

    addLabel("player_rot", "Player Rotation",
             [](Label&) { GET_PLAYER_VAR("Rotation", fRotationX, 3) }, {});
    addLabel("player_speed", "Player Speed",
             [](Label&) { GET_PLAYER_VAR("Speed", playerSpeed, 2) }, {});

    addLabel("player_grav", "Player Gravity",
             [](Label&) {
                 auto pl = PlayLayer::get();
                 auto player = pl->m_player1;
                 auto player2 = pl->m_player2;
                 if (pl->m_gameState.m_isDualMode && player2) {
                     return fmt::format(
                         "{}: {} / {}", "Gravity",
                         player->m_isUpsideDown ? "Flipped" : "Normal",
                         player2->m_isUpsideDown ? "Flipped" : "Normal");
                 } else {
                     return fmt::format(
                         "{}: {}", "Gravity",
                         player->m_isUpsideDown ? "Flipped" : "Normal");
                 }
             },
             {});

    addLabel("player_alive", "Player Dead/Alive",
             [](Label&) {
                 auto pl = PlayLayer::get();
                 auto player = pl->m_player1;
                 auto player2 = pl->m_player2;
                 if (pl->m_gameState.m_isDualMode && player2) {
                     return fmt::format("{} / {}",
                                        player->m_isDead ? "Dead" : "Alive",
                                        player2->m_isDead ? "Dead" : "Alive");
                 } else {
                     return fmt::format("{}",
                                        player->m_isDead ? "Dead" : "Alive");
                 }
             },
             {});

    addLabel("checkpoints", "Checkpoint #",
             [](Label&) {
                 auto& pf = Bot::get()->practiceFix();

                 return fmt::format("Checkpoint #: C - {} / B - {} / P - {}",
                                    pf.m_savedCheckpoints.size(),
                                    pf.m_storedFrames.size(),
                                    pf.m_platformerCheckpoints.size());
             },
             {});

    addLabel("level_time", "Level Time",
             [](Label&) {
                 return fmt::format("Level Time: {:.6f}s",
                                    PlayLayer::get()->m_gameState.m_levelTime);
             },
             {});

    addLabel("time_warp", "Time Warp",
             [](Label&) {
                 return fmt::format("Time Warp: {:.2f}x",
                                    PlayLayer::get()->m_gameState.m_timeWarp);
             },
             {});

    addLabel("bot_state", "Bot State",
             [](Label&) {
                 return fmt::format("Bot State: {}", Bot::get()->isPlaying()
                                                         ? "Playing"
                                                         : "Recording");
             },
             {});
    addLabel("random_state", "Random Seed State",
             [](Label&) {
                 return fmt::format(
                     "Random State: {}",
                     Bot::get()->replaySystem().getCurrentRandomState());
             },
             {});
    addLabel("shake_state", "Random Shake State",
             [](Label&) {
                 return fmt::format(
                     "Shake Random State: {}",
                     Bot::get()->replaySystem().getCurrentShakeState());
             },
             {});
    addLabel("teleport_state", "Random Teleport State",
             [](Label&) {
                 return fmt::format(
                     "Teleport Random State: {}",
                     Bot::get()->replaySystem().m_teleportRandomState);
             },
             {});
    addLabel("action_index", "Action Index",
             [](Label&) {
                 return fmt::format(
                     "Action Index: {}/{}",
                     Bot::get()->replaySystem().getInputIndex(),
                     Bot::get()->replaySystem().m_actionAtom.length());
             },
             {});
    addLabel("intentional_death", "Intentional Death",
             [](Label&) {
                 if (Bot::get()->isRecording()) {
                     return fmt::format("Intentional Death: {}",
                                        Bot::get()->updater().m_canDie->inner()
                                            ? "Enabled"
                                            : "Disabled");
                 }

                 return fmt::format("Intentional Death: {}",
                                    Bot::get()->updater().m_expectsDeath
                                        ? "Expects death"
                                        : "Nothing");
             },
             {});
    addLabel("ssb_factor", "Scroll Speed Bug Factor",
             [](Label&) {
                 return fmt::format("SSB Factor: {}",
                                    Bot::get()->updater().getSSB());
             },
             {});
    addLabel("touching_orbs", "Touching Orbs",
             [](Label&) {
                 // TODO: fix this. this makes way too many allocations
                 PlayerObject* player = PlayLayer::get()->m_player1;
                 CCArray* touchingRings = player->m_touchingRings;

                 std::string ringText = "Touching Orbs: ";
                 ringText.reserve(64);

                 uint32_t count = touchingRings->count();

                 std::vector<std::string> strings{};
                 strings.reserve(16);

                 for (uint32_t i = 0; i < count; i++) {
                     RingObject* ring = static_cast<RingObject*>(
                         touchingRings->objectAtIndex(i));

                     if (!ring->hasBeenActivatedByPlayer(player)) {
                         strings.push_back(getStringifiedOrbName(ring));

                         if (ring->m_objectID == 1594) {
                             strings.back() +=
                                 fmt::format("({})", ring->m_targetGroupID);
                         }

                         if (ring->m_isMultiActivate) {
                             strings.back() += "*";
                         }
                     }
                 }

                 if (strings.empty()) {
                     ringText += "None";
                 } else {
                     ringText += geode::utils::string::join(strings, ", ");
                 }

                 return ringText;
             },
             {});

    addLabel("max_upr", "Dynamic UPR",
             [](Label&) {
                 if (Bot::get()->updater().m_realTime->inner()) {
                     return std::string("Dynamic UPR: Uncapped");
                 }

                 if (Bot::get()->updater().m_dynamicUpr->inner()) {
                     return fmt::format("Dynamic UPR: {}",
                                        Bot::get()->updater().m_stepLimit);
                 } else {
                     return fmt::format("Static UPR: {}",
                                        Bot::get()->updater().m_stepLimit);
                 }
             },
             {});

    addLabel("cps", "CPS",
             [](Label& l) {
                 int cps1 = Bot::get()->cps().queryCPS(1);
                 int cps2 = Bot::get()->cps().queryCPS(2);

                 auto pl = GJBaseGameLayer::get();
                 if (cps1 > 16 || cps2 > 16) {
                     l.get()->setColor(ccc3(255, 128, 128));
                 } else {
                     l.get()->setColor(ccc3(255, 255, 255));
                 }

                 if (pl->m_gameState.m_isDualMode &&
                     pl->m_levelSettings->m_twoPlayerMode) {
                     return fmt::format("CPS: {} / {}", cps1, cps2);
                 } else {
                     return fmt::format("CPS: {}", cps1);
                 }
             },
             {});

    addLabel("max_cps", "Max CPS",
             [](Label& l) {
                 int cps1 = Bot::get()->cps().queryMaxCPS(1);
                 int cps2 = Bot::get()->cps().queryMaxCPS(2);

                 auto pl = GJBaseGameLayer::get();
                 if (cps1 > 16 || cps2 > 16) {
                     l.get()->setColor(ccc3(255, 128, 128));
                 } else {
                     l.get()->setColor(ccc3(255, 255, 255));
                 }

                 if (pl->m_levelSettings->m_twoPlayerMode) {
                     return fmt::format("Max CPS: {} / {}", cps1, cps2);
                 } else {
                     return fmt::format("Max CPS: {}", cps1);
                 }
             },
             {});

    addLabel("last_input", "Ticks Since Last Input",
             [](Label&) {
                 uint64_t tick = Bot::get()->updater().getFrame();
                 auto& rs = Bot::get()->replaySystem();

                 if (Bot::get()->isRecording()) {
                     if (!rs.m_actionAtom.m_actions.empty()) {
                         auto& action = rs.m_actionAtom.m_actions.back();
                         return fmt::format("Ticks Since Last Input: {}",
                                            tick - action.m_frame);
                     } else {
                         return fmt::format("No Inputs In Replay");
                     }
                 } else {
                     size_t length = rs.m_actionAtom.length();
                     if (length != 0) {
                         if (rs.m_inputIndex == 0) {
                             return fmt::format("Waiting For First Input");
                         }

                         auto& action =
                             rs.m_actionAtom.m_actions[rs.m_inputIndex - 1];

                         return fmt::format("Ticks Since Last Input: {}",
                                            tick - action.m_frame);
                     } else {
                         return fmt::format("No Inputs In Replay");
                     }
                 }
             },
             {});
}

#undef GET_PLAYER_VAR

void LabelManager::update(bool forceDisable) {
    float heights[4] = {0, 0, 0, 0};

    for (auto& label : m_labels) {
        int anchorIndex = static_cast<int>(label.m_config.m_anchor);
        label.update(forceDisable || !this->m_globalEnabled, m_requiresRefresh,
                     heights[anchorIndex]);
    }

    m_requiresRefresh = false;
}

// clang-format off
template <>
struct glz::meta<Label::LabelConfig> {
    using T = Label::LabelConfig;
    static constexpr auto value = object(
        "enabled", &T::m_enabled,
        "anchor", &T::m_anchor,
        "font", &T::m_font,
        "opacity", &T::m_opacity,
        "scale", &T::m_scale
    );
};
// clang-format on

void LabelManager::readFromConfig() {
    std::unordered_map<std::string, Label::LabelConfig> labels;

    auto labelConfigPath = Mod::get()->getConfigDir() / "labels.json";
    if (std::filesystem::exists(labelConfigPath)) {
        auto ec = glz::read_file_json(labels, labelConfigPath.string(),
                                      std::string{});
        if (ec) {
            log::error("Failed to read label config: {}",
                       ec.custom_error_message);
            return;
        }
    }

    for (auto& label : m_labels) {
        label.m_config = labels[label.getId()];
    }

    m_requiresRefresh = true;
}

void LabelManager::writeToConfig() {
    std::unordered_map<std::string, Label::LabelConfig> labels;

    for (const auto& label : m_labels) {
        labels[label.getId()] = label.m_config;
    }

    auto labelConfigPath = Mod::get()->getConfigDir() / "labels.json";

    auto ec = glz::write_file_json<glz::opts{.prettify = true}>(
        labels, labelConfigPath.string(), std::string{});
    if (ec) {
        log::error("Failed to write label config: {}", ec.custom_error_message);
    }
}

CCLabelBMFont* Label::get() {
    auto pl = PlayLayer::get();
    CCLabelBMFont* label = nullptr;

    std::string cocosLabelID =
        fmt::format("{}/label.{}", Mod::get()->getID(), m_id);
    if (pl) {
        if (auto l = pl->getChildByID(cocosLabelID); l) {
            return dynamic_cast<CCLabelBMFont*>(l);
        }

        switch (m_config.m_font) {
            case LabelFont::BigFont:
                m_font = "bigFont.fnt";
                break;
            case LabelFont::ChatFont:
                m_font = "chatFont.fnt";
                break;
        }

        label = CCLabelBMFont::create("Loading...", m_font.c_str());

        label->setID(cocosLabelID);
        label->setScale(m_config.m_scale);
        // this->calculatePosition(0, label);

        PlayLayer::get()->addChild(label, 100000);
    }

    return label;
}

void Label::calculatePosition(float& currentHeight, CCLabelBMFont* label) {
    if (!label) return;

    auto pl = PlayLayer::get();
    const float spacing = 8.0f;
    float innerSpacing = 4.0f * m_config.m_scale;

    switch (m_config.m_anchor) {
        case LabelAnchor::TopLeft:
            m_position = cocos2d::CCPoint(
                spacing, pl->getContentSize().height - spacing - currentHeight);
            m_cocosAnchor = cocos2d::CCPoint(0.0f, 1.0f);
            break;
        case LabelAnchor::TopRight:
            m_position = cocos2d::CCPoint(
                pl->getContentSize().width - spacing,
                pl->getContentSize().height - spacing - currentHeight);
            m_cocosAnchor = cocos2d::CCPoint(1.0f, 1.0f);
            break;
        case LabelAnchor::BottomLeft:
            m_position = cocos2d::CCPoint(10.0f, currentHeight + spacing);
            m_cocosAnchor = cocos2d::CCPoint(0.0f, 0.0f);
            break;
        case LabelAnchor::BottomRight:
            m_position = cocos2d::CCPoint(pl->getContentSize().width - spacing,
                                          currentHeight + spacing);
            m_cocosAnchor = cocos2d::CCPoint(1.0f, 0.0f);
            break;
    }

    currentHeight += label->getScaledContentSize().height + innerSpacing;

    label->setPosition(m_position);
    label->setAnchorPoint(m_cocosAnchor);
}

void Label::update(bool forceDisable, bool refresh, float& currentHeight) {
    auto pl = PlayLayer::get();
    if (pl) {
        CCLabelBMFont* label = get();
        if (!label) return;

        if (!m_config.m_enabled) {
            refresh = false;
            // don't tick the label if it's not enabled
        }

        if (refresh) {
            switch (m_config.m_font) {
                case LabelFont::BigFont:
                    m_font = "bigFont.fnt";
                    break;
                case LabelFont::ChatFont:
                    m_font = "chatFont.fnt";
                    break;
            }

            label->setFntFile(m_font.c_str());
            label->setOpacity(
                static_cast<GLubyte>(m_config.m_opacity * 255.0f));
            label->setScale(m_config.m_scale);
            this->calculatePosition(currentHeight, label);
        }

        label->setVisible(m_config.m_enabled && !forceDisable);
        label->setString(m_display(*this).c_str());
    }
}

$on_mod(Loaded) { Bot::get()->labels().readFromConfig(); }

$on_mod(DataSaved) { Bot::get()->labels().writeToConfig(); }
