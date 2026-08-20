#include "Geode/binding/GJBaseGameLayer.hpp"

#include <Zydis/Zydis.h>

#include <Geode/Geode.hpp>
#include <Geode/binding/AdvancedFollowInstance.hpp>
#include <Geode/binding/GJGroundLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <safetyhook.hpp>
#include <vector>

#include "assist/cps.hpp"
#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "checkpoint/fix.hpp"
#include "frame_window/analyzer.hpp"
#include "physics/collisions.hpp"
#include "physics/gjbasegamelayer.hpp"
#include "replay/system.hpp"
#include "trajectory/trajectory.hpp"
#include "util/midhook.hpp"

using namespace geode::prelude;

#include <Geode/modify/GJBaseGameLayer.hpp>

$execute {
    // Patch GJBaseGameLayer::resetLevelVariables to not release buttons
    geode::log::info("Patching {} (GJBaseGameLayer::resetLevelVariables)",
                     base::get() + 0x23B4EA);
    if (!Mod::get()->patch(reinterpret_cast<void*>(base::get() + 0x23B4EA),
                           {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90})) {
        geode::log::error(
            "Failed to patch GJBaseGameLayer::resetLevelVariables");
    }

    if (!Mod::get()->patch(
            reinterpret_cast<void*>(base::get() + 0x4cd95c),
            {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
             0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90})) {
        geode::log::error("Failed to patch UILayer::handleKeypress");
    }

    if (!Mod::get()->patch(reinterpret_cast<void*>(base::get() + 0x3B994C),
                           {0xeb, 0x5e})) {
        geode::log::error("Failed to patch PlayLayer::resetLevel");
    }

    if (!Mod::get()->patch(reinterpret_cast<void*>(base::get() + 0x3BA508),
                           {0x90, 0x90, 0x90, 0x90, 0x90})) {
        geode::log::error("Failed to patch PlayLayer::resumeAndRestart");
    }
}

struct SLGJBaseGameLayer : Modify<SLGJBaseGameLayer, GJBaseGameLayer> {
    struct PlayerState {
        float m_rotation;

        void saveFromPlayer(PlayerObject* player) {
            m_rotation = player->getRotation();
        }

        void loadIntoPlayer(PlayerObject* player) {
            player->setRotation(m_rotation);
        }
    };

    struct GroundState {
        CCPoint m_sprite1Pos;
        CCPoint m_sprite2Pos;

        void saveFromGround(GJGroundLayer* layer) {
            if (!layer) return;
            if (!layer->m_ground1Sprite) return;

            m_sprite1Pos = layer->m_ground1Sprite->getPosition();
            if (layer->m_ground2Sprite) {
                m_sprite2Pos = layer->m_ground2Sprite->getPosition();
            }
        }

        void loadIntoGround(GJGroundLayer* layer) {
            if (!layer) return;
            if (!layer->m_ground1Sprite) return;

            layer->m_ground1Sprite->setPosition(m_sprite1Pos);
            if (layer->m_ground2Sprite) {
                layer->m_ground2Sprite->setPosition(m_sprite2Pos);
            }
        }
    };

    struct Fields {
        GJGameState m_lastActualGameState;
        GroundState m_lastGround;
        GroundState m_lastGround2;

        PlayerState m_lastActualP1;
        PlayerState m_lastActualP2;
    };

    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("GJBaseGameLayer::handleButton",
                                     Priority::Last)) {
            geode::log::warn(
                "Failed to set priority for handleButton! Alt hook may not "
                "work as expected");
        }
    }

    bool shouldExtrapolate() {
        auto& updater = Bot::get()->updater();
        if (updater.estimatedStepCount != 0) {
            return false;
        }

        return true;
    }

    void extrapolateVisualUpdates(float dt) {
        auto& updater = Bot::get()->updater();

        float dt60 = dt * 60.0;

        using Mode = SLSettings::TrajectorySettings::Mode;
        auto* t = Bot::get()->trajectory().unsafeInner();

        // reset player rotation for predictions
        m_player1->setRotation(m_fields->m_lastActualP1.m_rotation);
        m_player2->setRotation(m_fields->m_lastActualP2.m_rotation);

        // predict player 1 and player 2 positions in next frame
        Mode m = m_player1->m_jumpBuffered ? Mode::Hold : Mode::Release;
        auto prediction = t->simulate(this, true, Mode::Player1 | m, true,
                                      {
                                          .m_bypassConfig = true,
                                          .m_maxLength = 0,
                                      });

        Mode m2 = m_player2->m_jumpBuffered ? Mode::Hold : Mode::Release;
        auto prediction2 = t->simulate(this, true, Mode::Player2 | m2, true,
                                       {
                                           .m_bypassConfig = true,
                                           .m_maxLength = 0,
                                       });

        // % of current frame "completion" - if halfway between this and next
        // frame, this is 0.5
        float framePosition = updater.m_tpsOverflow / updater.getPhysicsDt();

        // lerp positions based on frame position
        CCPoint lerped = prediction.position * framePosition +
                         m_player1->m_position * (1 - framePosition);
        float lerpedRot =
            prediction.rotation * framePosition +
            m_fields->m_lastActualP1.m_rotation * (1 - framePosition);

        CCPoint lerped2 = prediction2.position * framePosition +
                          m_player2->m_position * (1 - framePosition);
        float lerpedRot2 =
            prediction2.rotation * framePosition +
            m_fields->m_lastActualP2.m_rotation * (1 - framePosition);

        this->m_player1->setPosition(lerped);
        this->m_player2->setPosition(lerped2);
        this->m_player1->setRotation(lerpedRot);
        this->m_player2->setRotation(lerpedRot2);

        // update gjbgl (updateCamera only sets gameState variables so it's
        // easily restorable)
        this->updateCamera(dt60);
        this->updateVisibility(dt);

        HardStreak* p1Trail = this->m_player1->m_waveTrail;
        HardStreak* p2Trail = this->m_player2->m_waveTrail;

        if (this->m_player1->m_isDart) {
            p1Trail->setPosition(this->m_player1->m_position);
        }

        if (this->m_player2->m_isDart) {
            p2Trail->setPosition(this->m_player2->m_position);
        }

        this->CCLayer::update(dt);
    }

    void storeActualState() {
        m_fields->m_lastActualP1.saveFromPlayer(m_player1);
        m_fields->m_lastActualP2.saveFromPlayer(m_player2);
        m_fields->m_lastActualGameState = m_gameState;
        m_fields->m_lastGround.saveFromGround(m_groundLayer);
        m_fields->m_lastGround2.saveFromGround(m_groundLayer2);
    }

    void loadActualState() {
        m_gameState = m_fields->m_lastActualGameState;
        m_fields->m_lastActualP1.loadIntoPlayer(m_player1);
        m_fields->m_lastActualP2.loadIntoPlayer(m_player2);
        m_fields->m_lastGround.loadIntoGround(m_groundLayer);
        m_fields->m_lastGround2.loadIntoGround(m_groundLayer2);
    }

    void update(float dt) override {
        auto& updater = Bot::get()->updater();
        if (updater.m_onlyRefresh || !Bot::get()->isEnabled()) {
            GJBaseGameLayer::update(dt);
            return;
        }

        bool lockDelta = updater.isLockDelta() && PlayLayer::get();

        if (!lockDelta) {
            // Must be lock delta off OR is editor
            updater.calculateSteps(dt, updater.getPhysicsDt());
            updater.m_shouldRender = true;
        }

        if (updater.m_respawnTimer > 0) {
            updater.m_respawnTimer--;
            updater.totalStepCount = std::min(updater.totalStepCount, 1);
            updater.estimatedStepCount =
                std::min(updater.estimatedStepCount, 1);
        }

        if (updater.m_extrapolateFrames->inner() &&
            updater.getFrame() > updater.m_frameOnLastAttempt) {
            if (this->shouldExtrapolate()) {
                this->extrapolateVisualUpdates(dt);
            } else {
                this->loadActualState();
                GJBaseGameLayer::update(dt);
                this->storeActualState();
            }
        } else {
            if (lockDelta || updater.estimatedStepCount != 0) {
                GJBaseGameLayer::update(dt);
                this->storeActualState();
            }
        }
    }

    void gameEventTriggered(GJGameEvent event, int p1, int p2) {
        auto bot = Bot::get();

        if (bot->trajectory().drawing()) return;
        if (event == GJGameEvent::CheckpointRespawn &&
            !Bot::get()->practiceFix().m_shouldLoadPlatformer)
            return;

        GJBaseGameLayer::gameEventTriggered(event, p1, p2);
    }

    void destroyObject(GameObject* obj) {
        auto bot = Bot::get();
        if (bot->trajectory().drawing()) return;

        if (this->m_isPracticeMode) {
            bot->practiceFix().registerBrokenObject(obj);
        }

        GJBaseGameLayer::destroyObject(obj);
    }

    void updateCamera(float dt) {
        Bot::get()->updater().m_lastCameraPos =
            Bot::get()->updater().m_currentCameraPos;
        GJBaseGameLayer::updateCamera(dt);
        Bot::get()->updater().m_currentCameraPos = m_objectLayer->getPosition();
    }

    double getModifiedDelta(float dt) {
        if (!Bot::get()->isEnabled()) {
            return GJBaseGameLayer::getModifiedDelta(dt);
        }

        float modDelta = GJBaseGameLayer::getModifiedDelta(dt);
        if (auto lel = LevelEditorLayer::get(); lel && lel->m_playbackActive) {
            return modDelta;
        }

        auto& updater = Bot::get()->updater();
        if (updater.m_onlyRefresh) {
            return 0.0f;
        }

        float wantedDt =
            updater.getPhysicsDt() * fminf(m_gameState.m_timeWarp, 1.0f);

        return wantedDt;
    }

    void addInputToReplay(PlayerButtonCommand cmd) {
        auto bot = Bot::get();
        auto& replay = bot->replaySystem().m_actionAtom;
        if (replay.length() > 0 &&
            replay.m_actions.back() > bot->updater().getFrame()) {
            return;
        }

        auto _result = replay.addAction(
            bot->updater().getFrame(),
            static_cast<slc::Action::ActionType>(cmd.m_button), cmd.m_isPush,
            bot->replaySystem().playerFlipped(cmd.m_isPlayer2));

        if (cmd.m_button == PlayerButton::Jump && cmd.m_isPush) {
            bot->cps().pushAction(cmd.m_isPlayer2);
        }
    }

    void performMaintainGravity() {
        auto bot = Bot::get();

        m_queuedButtons.clear();  // we're maintaining gravity so we're
                                  // overriding queued buttons anyway

        auto p1 = this->m_player1;
        auto p2 = this->m_player2;
        if (PlayLayer::get()->m_levelEndAnimationStarted) {
            return;
        }

        bool p1Jumping = m_uiLayer->m_p1Jumping || m_uiLayer->m_p1TouchId != -1;
        bool p2Jumping = m_uiLayer->m_p2Jumping || m_uiLayer->m_p2TouchId != -1;
        if (bot->replaySystem().hasFlippedControls()) {
            std::swap(p1Jumping, p2Jumping);
        }

        if (bot->replaySystem().m_mirrorInputs) {
            p1Jumping |= p2Jumping ^ bot->replaySystem().m_mirrorInverted;
            p2Jumping |= p1Jumping ^ bot->replaySystem().m_mirrorInverted;
        }

        while (p1->m_isUpsideDown == (p1Jumping == p1->m_jumpBuffered) &&
               !p1->m_controlsDisabled) {
            addInputToReplay(PlayerButtonCommand{
                .m_button = static_cast<PlayerButton>(1),
                .m_isPush = (bool)(p1Jumping ^ p1->m_isUpsideDown),
                .m_isPlayer2 = bot->replaySystem().playerFlipped(false),
                .m_step = 0,
                .m_timestamp = 0.0});

            this->handleButton(p1Jumping ^ p1->m_isUpsideDown, 1,
                               bot->replaySystem().playerFlipped(true));
        }

        while (this->m_gameState.m_isDualMode &&
               this->m_levelSettings->m_twoPlayerMode &&
               p2->m_isUpsideDown == (p2Jumping == p2->m_jumpBuffered) &&
               !p2->m_controlsDisabled) {
            addInputToReplay(PlayerButtonCommand{
                .m_button = static_cast<PlayerButton>(1),
                .m_isPush = (bool)(p2Jumping ^ p2->m_isUpsideDown),
                .m_isPlayer2 = bot->replaySystem().playerFlipped(true),
                .m_step = 0,
                .m_timestamp = 0.0});

            this->handleButton(p2Jumping ^ p2->m_isUpsideDown, 1,
                               bot->replaySystem().playerFlipped(false));
        }
    }

    void requeueInverted() {
        auto bot = Bot::get();

        for (auto& cmd : m_queuedButtons) {
            bot->replaySystem().m_lastInputs.insert_or_assign(
                static_cast<int>(cmd.m_button) +
                    ((cmd.m_isPlayer2 && this->m_levelSettings->m_twoPlayerMode)
                         ? 4
                         : 0),
                slc::Action(
                    bot->replaySystem().m_forceNextInput
                        ? std::numeric_limits<uint32_t>::max()
                        : bot->updater().getFrame(),
                    0, static_cast<slc::Action::ActionType>(cmd.m_button),
                    cmd.m_isPush,
                    cmd.m_isPlayer2 && this->m_levelSettings->m_twoPlayerMode));

            if (!bot->replaySystem().m_mirrorInputs) continue;

            bool inverted = bot->replaySystem().m_mirrorInverted;

            this->queueButton(
                static_cast<int>(cmd.m_button), cmd.m_isPush ^ inverted,
                bot->replaySystem().playerFlipped(!cmd.m_isPlayer2), 0.0);
        }
    }

    void handleButton(bool pressed, int button, bool player1) {
        auto bot = Bot::get();
        if (!bot->replaySystem().m_useAlternateHook->inner() ||
            !bot->isRecording()) {
            return GJBaseGameLayer::handleButton(pressed, button, player1);
        }

        addInputToReplay({
            .m_button = static_cast<PlayerButton>(button),
            .m_isPush = pressed,
            .m_isPlayer2 = !player1,
            .m_step = 0,
            .m_timestamp = 0.0,
        });
        return GJBaseGameLayer::handleButton(pressed, button, player1);
    }

    void saveQueuedButtons() {
        for (auto& cmd : m_queuedButtons) {
            addInputToReplay(cmd);
        }
    }

    void processReplayAction(slc::Action& action) {
        auto bot = Bot::get();

        // Frame Window analysis deliberately reuses Silicate's existing
        // replay execution path. getNextInput() increments the input index
        // before returning the action, so the action being executed is index
        // (current - 1).
        if (bot->frameWindow().isActive()) {
            const size_t current = bot->replaySystem().getInputIndex();
            bot->frameWindow().onReplayAction(current == 0 ? 0 : current - 1,
                                              action, this);
        }

        int button = static_cast<int>(action.m_type);
        if (action.m_type == slc::Action::ActionType::Death) {
            bot->updater().m_expectsDeath = true;
            bot->replaySystem().m_startingSeedThisAttempt =
                bot->replaySystem().getCurrentRandomState();
            return;
        }

        if (action.m_type == slc::Action::ActionType::RestartFull) {
            bot->updater().m_expectsDeath = true;
            bot->replaySystem().m_startingSeedThisAttempt =
                bot->replaySystem().getCurrentRandomState();
            ((PlayLayer*)this)->fullReset();
            return;
        }

        if (action.m_type == slc::Action::ActionType::Restart) {
            bot->updater().m_expectsDeath = true;
            bot->replaySystem().m_startingSeedThisAttempt =
                bot->replaySystem().getCurrentRandomState();
            ((PlayLayer*)this)->resetLevel();
            return;
        }

        if (action.m_type == slc::Action::ActionType::TPS) {
            bot->updater().m_tps->inner() = action.m_tps;
            bot->updater().estimatedStepCount = 0;
            bot->updater().totalStepCount = 0;
            return;
        }

        if (button == 0 || button > 3) {
            return;
        }

        if (button == 1 && action.m_holding) {
            bot->cps().pushAction(action.m_player2);
        }

        this->queueButton(button, action.m_holding,
                          bot->replaySystem().playerFlipped(action.m_player2),
                          0);
    }

    void performAutoFlipOnDeath() {
        auto bot = Bot::get();

        if (PlayLayer::get()->m_levelEndAnimationStarted) {
            return;
        }

        bool p1Jumping = m_uiLayer->m_p1Jumping || m_uiLayer->m_p1TouchId != -1;
        bool p2Jumping = m_uiLayer->m_p2Jumping || m_uiLayer->m_p2TouchId != -1;
        if (bot->replaySystem().hasFlippedControls()) {
            std::swap(p1Jumping, p2Jumping);
        }

        if (bot->replaySystem().m_mirrorInputs) {
            p1Jumping |= p2Jumping ^ bot->replaySystem().m_mirrorInverted;
            p2Jumping |= p1Jumping ^ bot->replaySystem().m_mirrorInverted;
        }

        if (this->m_player1->m_jumpBuffered != bot->updater().m_isAutoFlipped) {
            m_queuedButtons.push_back(
                {.m_button = (PlayerButton)1,
                 .m_isPush = bot->updater().m_isAutoFlipped,
                 .m_isPlayer2 = false,
                 .m_step = 0,
                 .m_timestamp = 0.0});
        }
    }

    void processQueuedButtons(float dt, bool clearInputQueue) {
        auto bot = Bot::get();
        if (!bot->isEnabled()) {
            return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        }

        bot->practiceFix().updatePlatformerInputs(m_queuedButtons);

        if (bot->replaySystem().m_ignoreInputs->inner() && bot->isPlaying()) {
            m_queuedButtons.clear();
        }

        if (bot->isRecording()) {
            if (bot->replaySystem().m_maintainGravity) {
                this->performMaintainGravity();
                return;
            }

            this->requeueInverted();
            if (!bot->replaySystem().m_useAlternateHook->inner()) {
                this->saveQueuedButtons();
            }
        } else {
            uint32_t frame = bot->updater().getFrame();

            // Remove the ELL if the current input is a RestartFull on the
            // endscreen
            if (auto input = bot->replaySystem().getCurrentQueuedInput();
                input.has_value()) {
                // FIXME: this doesn't work
                uint32_t delayInFrames = bot->updater().m_tps->inner() * 0.5;

                if (input.value().m_type ==
                        slc::Action::ActionType::RestartFull &&
                    input.value().m_frame == frame + delayInFrames) {
                    if (auto ell = this->getChildByID("EndLevelLayer"); ell) {
                        FMODAudioEngine::sharedEngine()->clearAllAudio();
                        FMODAudioEngine::sharedEngine()->playEffect(
                            // @geode-ignore(unknown-resource)
                            "playSound_01.ogg", 1.0, 0.0, 0.3);
                        ((GJDropDownLayer*)ell)->exitLayer(nullptr);
                    }
                }
            }

            while (auto input = bot->replaySystem().getNextInput(frame)) {
                if (input.has_value()) this->processReplayAction(input.value());
            }
        }

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        if (bot->frameWindow().isActive()) {
            bot->frameWindow().onFrame(bot->updater().getFrame(), this);
        }

        if (bot->isRecording()) {
            if (bot->updater().m_autoFlipOnDeath->inner()) {
                this->performAutoFlipOnDeath();
                this->saveQueuedButtons();
                GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
                return;
            }
        }
    }

    void collisionCheckObjects(PlayerObject* player,
                               gd::vector<GameObject*>* objects, int length,
                               float p3) {
        auto bot = Bot::get();

        if (!bot->trajectory().isFakePlayer(player)) {
            return GJBaseGameLayer::collisionCheckObjects(player, objects,
                                                          length, p3);
        }

        phys::collisionCheckObjects(this, player, objects, length, p3);
    }

    void teleportPlayer(TeleportPortalObject* obj, PlayerObject* player) {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(player)) {
            phys::teleportPlayer(this, obj, player);
        } else {
            GJBaseGameLayer::teleportPlayer(obj, player);
        }
    }

    void flipGravity(PlayerObject* player, bool gravity, bool unk) {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(player)) {
            phys::flipGravity(player, gravity);
        } else {
            GJBaseGameLayer::flipGravity(player, gravity, unk);
        }
    }

    void toggleFlipped(bool flipped, bool noEffects) {
        if (Bot::get()->updater().m_noMirror->inner()) {
            this->m_gameState.m_unkBool10 = flipped;
            return;
        }

        GJBaseGameLayer::toggleFlipped(flipped, noEffects);
    }

    void processMoveActionsStep(float dt, bool visibleFrame) {
        const auto& randomHashObject = [](GameObject* object, int group,
                                          int index) {
            return (Bot::get()->replaySystem().m_startingSeed *
                    (int)(object->m_objectID + 5541) *
                    (int)(fabs(object->m_startPosition.x) + 1.0) *
                    (int)(fabs(object->m_startPosition.y) + 1.0) *
                    ((int)object->m_zLayer + 2137) *
                    (object->m_groupCount + 6969) *
                    ((int)object->m_isObjectBlack * 420 + 14) *
                    ((int)(fabs(object->m_startRotationX) * 6.7 + 1.0) *
                     (index + 67)) *
                    (group + 2137)) %
                   1777;
        };

        const auto& updateAllObjectsForGroup = [&, this](int group) {
            cocos2d::CCArray* objects = this->getGroup(group);
            for (int i = 0; i < (int)objects->count(); i++) {
                GameObject* object = (GameObject*)objects->objectAtIndex(i);
                object->m_varianceIndex = randomHashObject(object, group, i);
            }
        };

        for (auto& a : m_gameState.m_advanceFollowInstances) {
            updateAllObjectsForGroup(a.m_group);
        }

        for (auto& a : m_gameState.m_rotateEffectInstances) {
            updateAllObjectsForGroup(a.m_targetID);
        }

        for (auto& a : m_gameState.m_scaleEffectInstances) {
            updateAllObjectsForGroup(a.m_targetID);
        }

        for (auto& a : m_gameState.m_moveEffectInstances) {
            updateAllObjectsForGroup(a.m_targetID);
        }

        GJBaseGameLayer::processMoveActionsStep(dt, visibleFrame);
    }

    void createBackground(int background) {
        auto bot = Bot::get();
        if (!bot->isEnabled() || LevelEditorLayer::get()) {
            GJBaseGameLayer::createBackground(background);
            return;
        }

        auto& u = bot->updater();
        if (u.m_layoutMode->inner() && !u.m_useRegularBg->inner()) {
            background = 13;
        }

        GJBaseGameLayer::createBackground(background);
    }

    void createMiddleground(int mg) {
        auto bot = Bot::get();
        if (!bot->isEnabled() || !bot->updater().m_layoutMode->inner() ||
            !bot->updater().m_useRegularBg->inner() ||
            LevelEditorLayer::get()) {
            GJBaseGameLayer::createMiddleground(mg);
        } else {
            if (m_middleground) {
                m_middleground->removeFromParent();
                m_middleground = nullptr;
            }
        }
    }

    void createGroundLayer(int ground, int lineType) {
        auto bot = Bot::get();
        if (!bot->isEnabled() || LevelEditorLayer::get() ||
            !bot->updater().m_useRegularBg->inner()) {
            GJBaseGameLayer::createGroundLayer(ground, lineType);
            return;
        }

        if (bot->updater().m_layoutMode->inner()) {
            ground = 18;
            lineType = 2;
        }

        GJBaseGameLayer::createGroundLayer(ground, lineType);
    }

    // :(
    void updateColor(cocos2d::ccColor3B& color, float fadeTime, int colorID,
                     bool blending, float opacity, cocos2d::ccHSVValue& copyHSV,
                     int colorIDToCopy, bool copyOpacity,
                     EffectGameObject* callerObject, int unk1,
                     int unk2) override {
        if (!LevelEditorLayer::get() && Bot::get()->isEnabled() &&
            Bot::get()->updater().m_layoutMode->inner()) {
            return;
        }

        GJBaseGameLayer::updateColor(color, fadeTime, colorID, blending,
                                     opacity, copyHSV, colorIDToCopy,
                                     copyOpacity, callerObject, unk1, unk2);
    }

    void triggerGradientCommand(GradientTriggerObject* obj) {
        if (!LevelEditorLayer::get() && Bot::get()->isEnabled() &&
            Bot::get()->updater().m_layoutMode->inner()) {
            return;
        }

        GJBaseGameLayer::triggerGradientCommand(obj);
    }
};

static void shakeRandomOverride(SafetyHookContext& ctx) {
    uint64_t& state = Bot::get()->replaySystem().m_shakeRandomState;
    state = (int)((214013 * state + 2531011) >> 16) & 0x7FFF;

    ctx.rax = (uintptr_t)state;
}

static void teleportRandomOverride(SafetyHookContext& ctx) {
    uint64_t& state = Bot::get()->replaySystem().m_teleportRandomState;
    state = (int)((214013 * state + 2531011) >> 16) & 0x7FFF;

    ctx.rax = (uintptr_t)state;
}

static void overrideCheckpointPlacement(SafetyHookContext& ctx) {
    ctx.rip += 5;
    PlayLayer::get()->queueCheckpoint();
}

$execute {
    util::midhook(geode::base::get() + 0x23E173, "shakeRandomOverride",
                  shakeRandomOverride);
    util::midhook(geode::base::get() + 0x23E1A1, "shakeRandomOverride",
                  shakeRandomOverride);
    util::midhook(geode::base::get() + 0x23E1CB, "shakeRandomOverride",
                  shakeRandomOverride);
    util::midhook(geode::base::get() + 0x23E1E9, "shakeRandomOverride",
                  shakeRandomOverride);
    util::midhook(geode::base::get() + 0x20FED3, "teleportRandomOverride",
                  teleportRandomOverride);
    util::midhook(geode::base::get() + 0x3A3657, "checkpointPlacement",
                  overrideCheckpointPlacement);

    Bot::get()->updater().m_lockDelta->handle(
        [](bool&) { Bot::get()->updater().m_tpsOverflow = 0.0; });
}
