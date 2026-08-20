#include <Geode/Geode.hpp>
#include <filesystem>

#include "assist/autoclicker.hpp"
#include "assist/cps.hpp"
#include "assist/hitboxes.hpp"
#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "checkpoint/fix.hpp"
#include "frame_window/analyzer.hpp"
#include "label/label.hpp"
#include "render/renderer.hpp"
#include "replay/system.hpp"
#include "trajectory/trajectory.hpp"
#include "ui/manager.hpp"
#include "util/midhook.hpp"
#include "util/profile.hpp"

using namespace geode::prelude;

#include <Zydis/Zydis.h>

#include <Geode/modify/PlayLayer.hpp>
#include <safetyhook.hpp>

struct SLPlayLayer : Modify<SLPlayLayer, PlayLayer> {
    CheckpointObject* markCheckpoint() {
        if (!Bot::get()->isEnabled()) {
            return PlayLayer::markCheckpoint();
        }

        if (this->m_player1->m_isDead || this->m_player2->m_isDead)
            return nullptr;

        SCOPED_TIMER("createCheckpoint")
        auto cp = createCheckpoint();
        storeCheckpoint(cp);

        if (cp && cp->m_physicalCheckpointObject) {
            cp->m_physicalCheckpointObject->activateObject();
        }

        return cp;
    }

    void storeCheckpoint(CheckpointObject* obj) {
        if (!Bot::get()->isEnabled()) {
            return PlayLayer::storeCheckpoint(obj);
        }

        // this->m_activatedCheckpoint is reset in postUpdate
        if (this->m_activatedCheckpoint) {
            obj->retain();
            addToSection(obj->m_physicalCheckpointObject);

            Bot::get()->practiceFix().m_platformerCheckpoints.push_back(
                {obj, this->m_activatedCheckpoint});
            return;
        }

        obj->retain();
        // m_checkpointArray->addObject(obj);
        addToSection(obj->m_physicalCheckpointObject);
        Bot::get()->practiceFix().saveCurrent(
            obj, Bot::get()->updater().m_frameOnLastAttempt);
    }

    void loadFromCheckpoint(CheckpointObject* obj) {
        if (!Bot::get()->isEnabled()) {
            return PlayLayer::loadFromCheckpoint(obj);
        }

        auto& pf = Bot::get()->practiceFix();

        if (pf.m_forcedState) {
            PlayLayer::loadFromCheckpoint(pf.m_forcedState->m_checkpoint);
            pf.applyCheckpoint(*pf.m_forcedState);
            return;
        }

        // Backwards step
        if (pf.m_loadCheckpoint) {
            pf.restorePreviousFrame(
                [this](auto* cp) { this->PlayLayer::loadFromCheckpoint(cp); });

            // Restored - shouldn't be dead anymore
            pf.m_hasDiedNormally = false;

            return;
        }

        // For some reason m_savedCheckpoints is 0, just fallback to the game
        if (pf.m_savedCheckpoints.size() == 0) {
            PlayLayer::loadFromCheckpoint(obj);

            return;
        }

        // Platformer checkpoints are stored in a separate stack
        if (pf.m_shouldLoadPlatformer) {
            PlayLayer::loadFromCheckpoint(
                pf.m_platformerCheckpoints.back().first);

            return;
        }

        auto checkpoint = pf.m_savedCheckpoints.back();
        // Don't keep stale backsteps
        pf.clearStoredFrames();
        PlayLayer::loadFromCheckpoint(checkpoint.m_checkpoint);

        pf.applyLatest();
    }

    void delayedResetLevel() {
        // delayedResetLevel is only called when respawning from
        // an actual death, and not a restart or full restart
        //
        // therefore it's a safe way to determine if we want a Death or Restart
        // input after this resetLevel

        auto bot = Bot::get();
        if (bot->frameWindow().onDelayedReset(this)) {
            return;
        }
        if (bot->updater().m_canDie->inner()) {
            bot->updater().m_inputIsDeath = true;
        }

        PlayLayer::delayedResetLevel();
    }

    void fakeFullReset() {
        // Naive decompilation without removing checkpoints etc
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        this->m_gameState.m_totalTime = 0.0;
        this->m_gameState.m_unkDouble3 = 0.0;
        this->m_gameState.m_levelTime = 0.0;
        this->m_player1->m_totalTime = 0.0;
        this->m_player2->m_totalTime = 0.0;
        // this->m_queueInterval = 0.0;
        this->m_attempts = 0;
        this->m_jumps = 0;
        this->m_objectsDeactivated = true;
        this->m_freezeStartCamera = true;

        Bot::get()->practiceFix().clearPlatformer(true);
        this->resetLevel();
        this->m_attemptLabel->setPosition(CCPoint{
            winSize.width, winSize.width * 0.5f +
                               (float)this->m_gameState.m_cameraPosition.x});
    }

    void fullReset() {
        auto bot = Bot::get();
        if (!bot->isEnabled()) {
            return PlayLayer::fullReset();
        }

        auto& updater = bot->updater();

        if (updater.m_canDie->inner()) {
            updater.m_fullReset = true;

            this->fakeFullReset();
        } else if (updater.m_expectsDeath) {
            if (auto ell = this->getChildByID("EndLevelLayer"); ell) {
                ell->removeFromParent();
            }

            this->fakeFullReset();
        } else {
            PlayLayer::fullReset();
            bot->practiceFix().clearPlatformer(true);
            updater.m_frameOnLastAttempt = 0;
        }
    }

    void checkIfResetWasExpected(uint64_t deathFrame) {
        auto bot = Bot::get();
        while (auto input = bot->replaySystem().getNextInput(deathFrame)) {
            if (input->m_type == slc::Action::ActionType::Death) {
                bot->updater().m_expectsDeath = true;
            } else {
                break;
            }
        }
    }

    bool isResetIntentional() {
        auto bot = Bot::get();
        return bot->updater().m_canDie->inner() ||
               bot->updater().m_expectsDeath;
    }

    bool handleResetWithCheckpoints(uint64_t deathFrame) {
        auto bot = Bot::get();
        auto& pf = bot->practiceFix();

        if (this->isResetIntentional()) {
            // And then the next attempt starts from this frame + 1
            bot->updater().m_frameOnLastAttempt = deathFrame + 1;

            if (!pf.m_platformerCheckpoints.empty()) {
                this->m_checkpointArray->addObject(
                    pf.m_platformerCheckpoints.back().first);

                pf.m_shouldLoadPlatformer = true;
                return true;
            }

            return false;
        } else if (pf.m_loadCheckpoint) {
            this->m_checkpointArray->addObject(
                pf.m_storedFrames.back().m_checkpoint);
            return true;
        } else if (!pf.m_savedCheckpoints.empty()) {
            auto lastCheckpoint = pf.m_savedCheckpoints.back();
            this->m_checkpointArray->addObject(lastCheckpoint.m_checkpoint);
            return true;
        } else if (pf.m_forcedState) {
            auto check = pf.m_forcedState;
            this->m_checkpointArray->addObject(check->m_checkpoint);
            return true;
        }

        bot->updater().m_frameOnLastAttempt = 0;
        pf.clearPlatformer(true);
        return false;
    }

    void updateRandomSeedOnReset() {
        auto bot = Bot::get();
        auto& rs = bot->replaySystem();
        uint64_t& state = bot->replaySystem().getCurrentRandomState();
        if (!bot->updater().m_expectsDeath) {
            bot->replaySystem().m_startingSeedThisAttempt =
                bot->replaySystem().m_startingSeed;
        }

        if (bot->isRecording()) {
            if (rs.m_overrideSeed) {
                state = rs.m_overriddenSeed;
            }

            bot->replaySystem().m_startingSeedThisAttempt = state;
            if (bot->practiceFix().m_savedCheckpoints.empty() &&
                !bot->practiceFix().m_loadCheckpoint &&
                !bot->updater().m_canDie->inner()) {
                state = 214013 * state + 2531011;
                bot->replaySystem().m_startingSeed = state;
                bot->replaySystem().m_startingSeedThisAttempt = state;

                uint64_t varianceState = state;
                for (auto& v : m_varianceValues) {
                    v = (float)(varianceState & 0xFFFF) / 32768.0 - 1.0;
                    varianceState = 214013 * varianceState + 2531011;
                }
            }
        } else {
            bot->replaySystem().getCurrentRandomState() =
                bot->replaySystem().m_startingSeedThisAttempt;

            uint64_t varianceState =
                bot->replaySystem().getCurrentRandomState();
            for (auto& v : m_varianceValues) {
                v = (float)(varianceState & 0xFFFF) / 32768.0 - 1.0;
                varianceState = 214013 * varianceState + 2531011;
            }
        }
    }

    void restoreHoldOnReset(uint64_t deathFrame) {
        auto bot = Bot::get();
        bot->practiceFix().updatePlatformerInputs(m_queuedButtons);
        m_queuedButtons.clear();

        if (bot->isPlaying()) {
            if (bot->practiceFix().m_savedCheckpoints.empty()) {
                m_player1->releaseAllButtons();
                m_player2->releaseAllButtons();
            }

            if (bot->updater().m_expectsDeath) {
                while (auto input = bot->replaySystem().getNextQueuedInput()) {
                    if (input->m_frame == deathFrame) {
                        bot->replaySystem().advanceInputIndex();
                    } else {
                        break;
                    }
                }
            }

            auto& canDie = bot->updater().m_canDie;
            if (canDie->inner()) {
                canDie->inner() = false;
                canDie->notifyChange();
            }
            bot->updater().m_inputIsDeath = false;
            bot->updater().m_expectsDeath = false;

            this->processQueuedButtons(0.0, true);

            bot->updater().m_tpsOverflow = 0.0;
            return;
        }

        if (bot->isRecording()) {
            if (bot->updater().m_canDie->inner()) {
                m_player1->releaseAllButtons();
                m_player2->releaseAllButtons();
                return;
            }

            auto& rs = bot->replaySystem();

            bool p1Holding = this->m_uiLayer->m_p1Jumping ||
                             this->m_uiLayer->m_p1TouchId != -1;
            bool p2Holding = this->m_uiLayer->m_p2Jumping ||
                             this->m_uiLayer->m_p2TouchId != -1;

            auto& pf = bot->practiceFix();
            bool p1Left = pf.m_p1Left;
            bool p2Left = pf.m_p2Left;
            bool p1Right = pf.m_p1Right;
            bool p2Right = pf.m_p2Right;

            if (rs.hasFlippedControls()) {
                std::swap(p1Holding, p2Holding);
            }

            p1Holding |= p2Holding && !m_levelSettings->m_twoPlayerMode;

#define BUTTON_CHECK(varName, button, player)                                  \
    if (p##player##varName !=                                                  \
        this->m_player##player->m_holdingButtons[button]) {                    \
        this->queueButton(button, p##player##varName,                          \
                          rs.playerFlipped(static_cast<bool>(player - 1)), 0); \
    }

            BUTTON_CHECK(Holding, 1, 1)
            if (m_isPlatformer) {
                BUTTON_CHECK(Left, 2, 1)
                BUTTON_CHECK(Right, 3, 1)
            }

            if (m_levelSettings->m_twoPlayerMode) {
                BUTTON_CHECK(Holding, 1, 2)
                if (m_isPlatformer) {
                    BUTTON_CHECK(Left, 2, 2)
                    BUTTON_CHECK(Right, 3, 2)
                }
            }

#undef BUTTON_CHECK

            rs.m_lastInputs.clear();
        }
    }

    void addDeathInput(uint64_t deathFrame) {
        auto bot = Bot::get();
        if (bot->updater().m_canDie->inner()) {
            if (bot->updater().m_inputIsDeath) {
                (void)bot->replaySystem().m_actionAtom.addAction(
                    deathFrame, slc::Action::ActionType::Death,
                    bot->replaySystem().m_startingSeedThisAttempt);
            } else if (bot->updater().m_fullReset) {
                (void)bot->replaySystem().m_actionAtom.addAction(
                    deathFrame, slc::Action::ActionType::RestartFull,
                    bot->replaySystem().m_startingSeedThisAttempt);
            } else {
                (void)bot->replaySystem().m_actionAtom.addAction(
                    deathFrame, slc::Action::ActionType::Restart,
                    bot->replaySystem().m_startingSeedThisAttempt);
            }
        }
    }

    void intentionalResetDone() {
        auto bot = Bot::get();
        auto updater = bot->updater();
        updater.m_canDie->inner() = false;
        updater.m_canDie->notifyChange();
        updater.m_inputIsDeath = false;
        updater.m_expectsDeath = false;
        updater.m_fullReset = false;
        updater.m_tpsOverflow = 0.0;
        // updater.m_respawning = false;
    }

    void resetLevel() {
        auto bot = Bot::get();
        if (!bot->isEnabled()) {
            m_player1->releaseAllButtons();
            m_player2->releaseAllButtons();

            return PlayLayer::resetLevel();
        }

        bot->frameWindow().onResetBegin(this);

        // Practice music sync should always be on
        m_practiceMusicSync = true;

        bot->updater().m_tpsOverflow = 0.0;
        bot->updater().m_respawnTimer = 2;

        // Increment the frame, because update that'll increment the frame
        // won't be called this death frame and we want to make it distinct
        // from the frame just before
        bot->updater().incrementFrame();

        // That distinct frame
        uint64_t deathFrame = bot->updater().getFrame();

        // Sanity check, should probably be already expected
        this->checkIfResetWasExpected(deathFrame);

        // This is basically a signal that'll determine whether to clean the
        // checkpoint array up again
        bool hasAddedCheckpoint = this->handleResetWithCheckpoints(deathFrame);

        // bot->replaySystem().m_forceNextInput = true;
        // this->processQueuedButtons();
        // bot->replaySystem().m_forceNextInput = false;

        // m_frameOnLastAttempt was updated, we may reset the frame now to the
        // first frame for this given attempt (will be 0 for non-ID)
        bot->updater().resetFrame();

        // Clear hitbox trail (maybe prune it up until this frame later?)
        bot->hitboxes().clearTrail();

        bot->cps().clearActions();

        this->updateRandomSeedOnReset();

        // mod::emit(mod::events::LevelRestart{
        //     .intentional = bot->updater().m_canDie->inner(),
        // });

        PlayLayer::resetLevel();

        // Cleanup any checkpoints added to the array (should be just one!)
        if (hasAddedCheckpoint) {
            Bot::get()->practiceFix().m_shouldLoadPlatformer = false;
            this->m_checkpointArray->removeLastObject();

            // Mitigates the ghost frame
            bot->updater().m_onlyRefresh = true;
            this->m_extraDelta = 0.0f;
            CCScheduler::get()->update(0.0f);

            // Re-update labels after reset
            bool disableLabels = Renderer::get()->isRecording();
            bot->labels().update(disableLabels);
            bot->updater().m_onlyRefresh = false;
        }

        // Reset internal bot systems
        bot->replaySystem().onReset(bot->updater().getFrame());
        bot->autoclicker().reset();

        // And re-update trajectory
        bot->trajectory().update(this);

        this->restoreHoldOnReset(deathFrame);
        this->addDeathInput(deathFrame);

        // Don't re-process inputs if the player has intentionally died
        if (!bot->updater().m_canDie->inner()) {
            bot->replaySystem().m_flipProcessingInputs = true;
            this->processQueuedButtons(0.0, true);
            bot->replaySystem().m_flipProcessingInputs = false;
        }

        // And then just restore ID state for subsequent deaths
        this->intentionalResetDone();

        // Respawned fully
        bot->practiceFix().m_hasDiedNormally = false;
        bot->practiceFix().m_isBackstep = false;

        bot->updater().breakLoop();
    }

    void updateAttempts() {
        if (Bot::get()->practiceFix().m_isBackstep) {
            return;
        }

        PlayLayer::updateAttempts();
    }

    void onQuit() {
        if (Renderer::get()->isRecording()) {
            Renderer::get()->signalStop();
        }

        Renderer::get()->restoreView();

        auto bot = Bot::get();
        bot->frameWindow().onQuit();

        // Deallocate trajectory and hitbox nodes
        // before the PlayLayer gets destroyed
        bot->trajectory().uninit();
        bot->hitboxes().destroy();

        PlayLayer::onQuit();

        // This will free all checkpoint objects, so we just have to clear the
        // vector
        bot->practiceFix().clearStoredFrames();
        bot->practiceFix().removeAll();

        // Reset the input index to 0
        bot->replaySystem().onExit();
    }

    void removeCheckpoint(bool p0) {
        if (!Bot::get()->isEnabled()) {
            return PlayLayer::removeCheckpoint(p0);
        }

        if (Bot::get()->practiceFix().m_savedCheckpoints.empty()) {
            return;
        }

        auto checkpoint = Bot::get()->practiceFix().m_savedCheckpoints.back();

        // Remove the physical checkpoint object
        auto obj = checkpoint.m_checkpoint->m_physicalCheckpointObject;
        this->removeObjectFromSection(obj);

        auto* glowSprite = obj->m_glowSprite;
        if (glowSprite) {
            glowSprite->release();
            glowSprite->removeMeAndCleanup();
            glowSprite = nullptr;
        }

        obj->removeMeAndCleanup();

        checkpoint.m_checkpoint->release();

        Bot::get()->practiceFix().popLatest();
    }

    void removeAllCheckpoints() {
        // Let the game handle it
        if (!Bot::get()->isEnabled()) {
            return PlayLayer::removeAllCheckpoints();
        }

        // Don't remove all checkpoints if this is a "fake" full reset
        if (Bot::get()->updater().m_fullReset) return;

        // Otherwise, just call removeCheckpoint for every checkpoint
        while (!Bot::get()->practiceFix().m_savedCheckpoints.empty()) {
            removeCheckpoint(false);
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!Bot::get()->isEnabled()) {
            return PlayLayer::init(level, useReplay, dontCreateObjects);
        }

        auto bot = Bot::get();
        bot->practiceFix().clearStoredFrames();
        if (bot->trajectory().exists()) {
            bot->trajectory().uninit();
        }
        bot->practiceFix().clearPlatformer(
            false);  // only time when we know for a fact platformer checkpoints
                     // are not loaded

        auto renderer = Renderer::get();
        if (renderer->m_shouldStart) {
            FMODAudioEngine::sharedEngine()->stopAllMusic(true);
            FMODAudioEngine::sharedEngine()->stopAllEffects();
            renderer->acquireView();
        }

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        m_clickOnSteps = false;
        m_clickBetweenSteps = false;

        bot->trajectory().init();
        bot->hitboxes().init(this);
        bot->updater().m_frameOnLastAttempt = 0;
        bot->updater().m_lastTfp = 0.0f;
        bot->labels().m_requiresRefresh = true;

        return true;
    }

    void conditionalDestroyPlayer(PlayerObject* player,
                                  GameObject* gameObject) {
        if (gameObject == m_anticheatSpike) {
            return PlayLayer::destroyPlayer(player, gameObject);
        }

        auto bot = Bot::get();

        using N = BotUpdater::NoclipType;
        bool shouldDie = (bot->updater().m_noclipType == N::Player1 &&
                          (player == m_player2)) ||
                         (bot->updater().m_noclipType == N::Player2 &&
                          (player == m_player1));

        if (!bot->updater().m_noclip->inner() || shouldDie) {
            return PlayLayer::destroyPlayer(player, gameObject);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        auto bot = Bot::get();
        // destroyPlayer can be triggered by multiple objects in the same tick
        if (!bot->trajectory().hasDied(player) && !this->m_player1->m_isDead &&
            !this->m_player2->m_isDead) {
            this->conditionalDestroyPlayer(player, gameObject);

            if (bot->updater().m_preventDeath->inner() &&
                !bot->updater().m_noclip->inner() &&
                gameObject != m_anticheatSpike) {
                bot->updater().backwardsStep();
                this->processQueuedButtons(0.0, true);

                if (bot->updater().m_autoFlipOnDeath->inner()) {
                    if (player == m_player1) {
                        this->queueButton(1, !player->m_jumpBuffered, false,
                                          0.0);
                    } else {
                        this->queueButton(1, !player->m_jumpBuffered, true,
                                          0.0);
                    }

                    // bot->updater().stepOnce();
                    bot->updater().setPaused(false);
                }
            }

            if (!bot->practiceFix().m_loadCheckpoint) {
                bot->practiceFix().m_hasDiedNormally = true;
            }

            if (player == this->m_player1 || player == this->m_player2) {
                bot->frameWindow().onDeath(player, gameObject);
            }
        }
    }

    void pauseGame(bool p0) {
        m_gameState.m_pauseCounter = 0;
        Renderer::get()->restoreView();
        PlayLayer::pauseGame(p0);
    }

    void updateVisibility(float dt) {
        auto& updater = Bot::get()->updater();
        if (updater.m_extrapolateFrames->inner()) {
            dt = CCDirector::get()->getDeltaTime();
        }

        PlayLayer::updateVisibility(dt);

        if (!updater.m_shouldRender) {
            return;
        }

        if (updater.m_layoutMode->inner()) {
            constexpr std::array<int, 5> colors = {1000, 1001, 1009, 1013,
                                                   1014};
            for (const int color : colors) {
                if (ColorAction* action =
                        m_effectManager->getColorAction(color)) {
                    switch (color) {
                        case 1000: {
                            auto& col = SLSettings::get()->layoutBgColor;
                            action->m_color = {
                                static_cast<GLubyte>(col[0] * 255),
                                static_cast<GLubyte>(col[1] * 255),
                                static_cast<GLubyte>(col[2] * 255),
                            };
                            break;
                        }
                        case 1001:
                        case 1009: {
                            auto& col = SLSettings::get()->layoutGroundColor;
                            action->m_color = {
                                static_cast<GLubyte>(col[0] * 255),
                                static_cast<GLubyte>(col[1] * 255),
                                static_cast<GLubyte>(col[2] * 255),
                            };
                            break;
                        }
                        default: {
                            action->m_color = {40, 125, 255};
                        }
                    }
                }
            }
        }

        Bot::get()->hitboxes().draw(this);
    }

    void levelComplete() {
        PlayLayer::levelComplete();

        auto bot = Bot::get();
        if (bot->frameWindow().onLevelComplete(this)) return;
        if (!bot->replaySystem().m_autosaveAtLevelEnd->inner()) return;

        auto path = Mod::get()->getPersistentDir() / "replays" /
                    (bot->ui().m_state.m_replayName + ".slc");
        if (std::filesystem::exists(path)) {
            bot->replaySystem().createBackup();
        } else {
            bot->replaySystem().save(path,
                                     true);  // don't overwrite existing replays
        }
    }

    void setupHasCompleted() {
        auto bot = Bot::get();
        if (!bot->isEnabled()) {
            return PlayLayer::setupHasCompleted();
        }

        Renderer* renderer = Renderer::get();
        renderer->startIfQueued();

        PlayLayer::setupHasCompleted();

        bot->hitboxes().raiseGameUI(this);
    }

    void showCompleteText() {
        auto bot = Bot::get();
        if (!bot->isEnabled()) {
            return PlayLayer::showCompleteText();
        }

        std::unordered_set<CCNode*> before;
        this->collectChildren(before);

        PlayLayer::showCompleteText();

        std::unordered_set<CCNode*> after;
        this->collectChildren(after);

        for (CCNode* child : after) {
            if (!before.contains(child)) {
                bot->hitboxes().raiseAbove(child);
            }
        }
    }

    void collectChildren(std::unordered_set<CCNode*>& out) {
        CCArray* children = this->getChildren();
        if (!children) return;

        for (unsigned int i = 0; i < children->count(); i++) {
            if (auto child = static_cast<CCNode*>(children->objectAtIndex(i))) {
                out.insert(child);
            }
        }
    }

    void addObject(GameObject* obj) {
        if (!Bot::get()->isEnabled() ||
            !Bot::get()->updater().m_layoutMode->inner()) {
            return PlayLayer::addObject(obj);
        }

        static const std::unordered_set<int> OTHER_OBJECT_IDS = {
            32,   33,   1006, 1007, 29,   30,   104,  105,  221,  717,
            718,  743,  744,  899,  915,  2903, 2904, 2905, 2907, 2909,
            2910, 2911, 2912, 2913, 2914, 2915, 2916, 2917, 2919, 2920,
            2921, 2922, 2923, 2924, 3029, 3030, 3031, 3606, 3612};

        if (OTHER_OBJECT_IDS.contains(obj->m_objectID)) {
            obj->m_isHide = true;
            // return;
        } else {
            obj->setOpacity(255);
            obj->m_isHide = false;
            obj->m_hasNoGlow = true;
            obj->m_activeMainColorID = -1;
            obj->m_activeDetailColorID = -1;
            obj->m_baseUsesHSV = false;
            obj->m_detailUsesHSV = false;
            obj->m_isDontFade = true;
            obj->m_isDontEnter = true;
        }

        PlayLayer::addObject(obj);
    }
};

constexpr int QUEUE_CHECKPOINT_OFFSET = 0x4ce060;

// this basically removes the one frame delay with placing checkpoints
void PlayLayer_queueCheckpoint(void* unk, void* unk2) {
    if (!Bot::get()->isEnabled()) {
        return reinterpret_cast<void (*)(void*, void*)>(
            geode::base::get() + QUEUE_CHECKPOINT_OFFSET)(unk, unk2);
    }

    auto pl = PlayLayer::get();

    if (!pl) return;
    if (pl->m_player1->m_isDead || pl->m_player2->m_isDead) {
        return;
    }

    Bot::get()->updater().scheduleFrozenFunction(
        [pl](float) { pl->markCheckpoint(); });
}

$execute {
    (void)Mod::get()->hook(
        reinterpret_cast<void*>(geode::base::get() + QUEUE_CHECKPOINT_OFFSET),
        &PlayLayer_queueCheckpoint, "PlayLayer::queueCheckpoint",
        tulip::hook::TulipConvention::Default);
}

static void resetLevelSeedMidhook(SafetyHookContext&) {
    if (!Bot::get()->isEnabled()) {
        return;  // don't modify rng
    }

    Bot::get()->replaySystem().getCurrentRandomState() =
        Bot::get()->replaySystem().m_startingSeedThisAttempt;
    Bot::get()->replaySystem().m_shakeRandomState =
        Bot::get()->replaySystem().m_startingSeedThisAttempt & 0x7FFF;
    Bot::get()->practiceFix().reseedAdvancedRandom(
        Bot::get()->replaySystem().m_startingSeedThisAttempt);
    // srand(static_cast<unsigned int>(
    //     Bot::get()->replaySystem().m_startingSeedThisAttempt));
}

$execute {
    util::midhook(geode::base::get() + 0x3b90fb, "resetLevelSeed",
                  resetLevelSeedMidhook);

    Bot::get()->updater().m_tps->handle([](double& tps) {
        if (tps <= 0.0) {
            tps = 240.0;
        }

        if (auto pl = GJBaseGameLayer::get(); pl) {
            Bot::get()->updater().scheduleFrozenFunction([pl](float) {
                if (auto t = Bot::get()->trajectory().unsafeInner(); t) {
                    t->invalidateCache();
                }
                Bot::get()->trajectory().update(pl);
            });
        }
    });
}
