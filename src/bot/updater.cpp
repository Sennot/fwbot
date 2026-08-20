#include "updater.hpp"

#include <Zydis/Zydis.h>

#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <chrono>
#include <safetyhook.hpp>

#include "Geode/cocos/CCScheduler.h"
#include "assist/autoclicker.hpp"
#include "assist/cps.hpp"
#include "assist/hitboxes.hpp"
#include "bot.hpp"
#include "bot/updater.hpp"
#include "checkpoint/fix.hpp"
#include "label/label.hpp"
#include "render/renderer.hpp"
#include "replay/system.hpp"
#include "trajectory/trajectory.hpp"
#include "util/midhook.hpp"
#include "util/profile.hpp"

using namespace geode::prelude;

float BotUpdater::getSSB() {
    const static float speeds[5] = {251.1601f, 311.5801f, 387.4201f, 468.0002f,
                                    576.0002f};

    int playerSpeed = 1;

    if (GJBaseGameLayer::get()->m_player1->m_playerSpeed == 0.7f)
        playerSpeed = 0;
    else if (GJBaseGameLayer::get()->m_player1->m_playerSpeed == 0.9f)
        playerSpeed = 1;
    else if (GJBaseGameLayer::get()->m_player1->m_playerSpeed == 1.1f)
        playerSpeed = 2;
    else if (GJBaseGameLayer::get()->m_player1->m_playerSpeed == 1.3f)
        playerSpeed = 3;
    else if (GJBaseGameLayer::get()->m_player1->m_playerSpeed == 1.6f)
        playerSpeed = 4;

    // formula given by ij9
    float f = GJBaseGameLayer::get()->m_player1->getPositionX();
    float a = speeds[playerSpeed] / Bot::get()->updater().getTps();
    float c = (f + a) - f;
    float factor = c / a;

    if (factor <= 0 || GJBaseGameLayer::get()->m_levelEndAnimationStarted)
        factor = 1;

    return factor;
}

float BotUpdater::getTimeWarp() {
    if (auto pl = PlayLayer::get(); pl) {
        return pl->m_gameState.m_timeWarp;
    }

    return 1.0;
}

/**
 * Calculate the number of steps to take based on the delta time.
 * This also handles overflow from previous frames and manages
 * rendering behavior.
 */
void BotUpdater::calculateSteps(float dt, float targetDt) {
    dt += m_tpsOverflow;

    float wantedDt = targetDt * fminf(getTimeWarp(), 1.0);
    if (wantedDt == 0.0) {
        return;
    }

    int steps = std::floor(dt / wantedDt);

    if (!m_realTime->inner() && !m_dynamicUpr->inner()) {
        m_stepLimit = m_maxUPR->inner();
    }

    int stepLimit = m_stepLimit;

    if (m_respawnTimer > 0) {
        // steps = 0;
        // dt = 0.0f;             // but also don't add overflow
        m_tpsOverflow = 0.0f;  // reset overflow
    }

    // Aka "how many steps can we take in one VISUAL frame"
    if (m_useVisualUpdates->inner() && !m_dynamicUpr->inner()) {
        float fps = GameManager::get()->m_customFPSTarget;
        if (fps <= 10.0) {
            fps = 240.0;
        }

        int modifier = (int)m_tps->inner() / (int)fps;

        stepLimit *= std::max(1, modifier);
    }

    // Rendering shouldn't have an UPR limit
    bool rendering = Renderer::get()->isRecording();

    if (!m_realTime->inner() && !rendering) {
        steps = std::min(steps, stepLimit);
    }

    m_tpsOverflow = dt - steps * wantedDt;
    m_shouldRender = false;

    // Again, rendering doesn't have an UPR limit so no need
    // to reset tps overflow here
    if (!m_realTime->inner() && !rendering) {
        if (steps == stepLimit) {
            m_tpsOverflow = 0.0;
        }
    }

    if (isPaused() && !rendering) {
        steps = 1;  // ALWAYS frame advance one step
        m_tpsOverflow = 0;
        m_shouldRender = true;
        if (PlayLayer::get()) {
            PlayLayer::get()->m_extraDelta = 0.0f;  // reset extra delta
        }
    }

    totalStepCount = steps;
    estimatedStepCount = steps;
}

static void runFastLockDeltaUpdates(float realDt,
                                    std::function<void(float)> update) {
    auto bot = Bot::get();
    auto& updater = bot->updater();

    auto& nextInput = Bot::get()->replaySystem().getCurrentQueuedInput();

    SCOPED_TIMER("fastLockDelta")
    updater.m_allowedToProcessActions = false;
    if (!nextInput.has_value()) {
        // run updates normally
        updater.calculateSteps(
            realDt * updater.getTimeWarp() * updater.m_speedhack->inner(),
            updater.getPhysicsDt());

        if (updater.estimatedStepCount >= 1) {
            updater.m_shouldRender = true;
            update(realDt * updater.m_speedhack->inner());
        }
    } else {
        updater.calculateSteps(
            realDt * updater.getTimeWarp() * updater.m_speedhack->inner(),
            updater.getPhysicsDt());

        int steps = updater.totalStepCount;

        while (steps > 0) {
            auto& input = Bot::get()->replaySystem().getCurrentQueuedInput();
            uint64_t safeSteps =
                input.has_value() ? input->m_frame - updater.getFrame() : steps;
            safeSteps = std::min(safeSteps, static_cast<uint64_t>(steps));

            if (safeSteps > 0) {
                updater.estimatedStepCount = safeSteps;
                if (steps - safeSteps == 0) {
                    updater.m_shouldRender = true;
                }
                update(updater.getPhysicsDt() * safeSteps);
                steps -= safeSteps;
            }

            if (steps > 0) {
                updater.estimatedStepCount = 1;
                updater.m_shouldRender = true;
                update(realDt * updater.m_speedhack->inner());
                steps -= 1;
            }
        }
    }
}

static void runSlowLockDeltaUpdates(float realDt,
                                    std::function<void(float)> update,
                                    bool calculateSsb) {
    GJBaseGameLayer* pl = PlayLayer::get();
    if (!pl) {
        pl = LevelEditorLayer::get();
    }

    auto bot = Bot::get();
    auto& updater = bot->updater();

    float newTfx = pl->timeForPos(pl->m_player1->m_position, 0,
                                  pl->m_gameState.m_currentChannel, true, 0);
    float ssbDelta = std::abs(newTfx - updater.m_lastTfp);

    if (!calculateSsb || ssbDelta <= 0) {
        ssbDelta = updater.getPhysicsDt();
    }

    updater.m_lastTfp = newTfx;
    float delta = updater.getPhysicsDt();

    updater.calculateSteps(
        realDt * updater.getTimeWarp() * updater.m_speedhack->inner(),
        ssbDelta);

    SCOPED_TIMER("slowLockDelta")
    updater.m_shouldRender = false;

    for (int i = 0; i < updater.estimatedStepCount - 1; i++) {
        update(delta);
        if (!((PlayLayer*)pl)->m_hasCompletedLevel &&
            updater.estimatedStepCount > 0) {
            if (calculateSsb) {
                float new_tfx =
                    pl->timeForPos(pl->m_player1->m_position, 0,
                                   pl->m_gameState.m_currentChannel, true, 0);

                ssbDelta = std::abs(new_tfx - updater.m_lastTfp);
                if (ssbDelta <= 0) {
                    ssbDelta = updater.getPhysicsDt();
                }
                updater.m_lastTfp = new_tfx;
                updater.calculateSteps(realDt * updater.getTimeWarp() *
                                           updater.m_speedhack->inner(),
                                       ssbDelta);  // yes we have to actively
                                                   // recalculate steps
            }
        }
    }

    updater.m_shouldRender = true;
    if (updater.estimatedStepCount > 0) {
        update(delta);
    } else if (updater.m_extrapolateFrames->inner()) {
        update(realDt *
               updater.m_speedhack
                   ->inner());  // guaranteed to be LESS than one physics tick
    }
}

void BotUpdater::runUpdates(std::function<void(float)> update, float realDt,
                            bool frozen) {
    auto bot = Bot::get();
    m_allowedToProcessActions = true;

    // Frozen updates are with dt=0 and shouldn't run any physics updates
    if (frozen) {
        SCOPED_TIMER("frozenGameUpdate")
        m_onlyRefresh = true;
        update(realDt);
        m_onlyRefresh = false;
        if (PlayLayer::get()) {
            bot->trajectory().update(PlayLayer::get());
        }
        return;
    }

    SCOPED_TIMER("fullGameUpdate")

    GJBaseGameLayer* pl = PlayLayer::get();
    bool isPlayLayer = true;
    if (!pl) {
        pl = LevelEditorLayer::get();
        isPlayLayer = false;
    }

    if (!pl || (isPlayLayer && ((PlayLayer*)pl)->m_isPaused)) {
        this->m_tpsOverflow = 0;
        update(realDt);  // be quiet
        return;
    }

    int visualFpsFactor =
        std::max(std::min(Renderer::get()->m_settings.m_visualFps,
                          (int)Bot::get()->updater().getTps()) /
                     Renderer::get()->m_settings.m_fps,
                 1);

    if (auto renderer = Renderer::get();
        renderer->isRecording() && renderer->m_collectAudio) {
        realDt = renderer->getDt() / visualFpsFactor /
                 ((Bot::get()->updater().m_ssbFix->inner())
                      ? Bot::get()->updater().getSSB()
                      : 1.f);
    }

    if (this->isPaused() && !Renderer::get()->isRecording()) {
        this->m_tpsOverflow = 0;
        m_respawnTimer = 0;
        if (this->consumeStep()) {
            this->m_shouldRender = true;
        } else {
            return;
        }
    }

    // cleanup later (or never since we're doing v2 anyways)
    bool calculateSsb = false;

    bool useAccLockDelta =
        m_lockDelta->inner() &&
        (m_lockDeltaMode->inner() == LockDeltaMode::Accuracy ||
         Renderer::get()->isRecording());

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < (Renderer::get()->isRecording() ? visualFpsFactor : 1);
         i++) {
        if ((m_lockDelta->inner() || useAccLockDelta) && isPlayLayer) {
            if (this->useFastLockDelta()) {
                runFastLockDeltaUpdates(realDt, update);
            } else {
                runSlowLockDeltaUpdates(realDt, update, calculateSsb);
            }
        } else {
            m_shouldRender = true;
            if (m_respawnTimer > 0) {
                m_tpsOverflow = 0.0;
                pl->m_extraDelta = 0.0;
                // this->estimatedStepCount = 0;
                // this->totalStepCount = 0;
            }

            update(realDt * m_speedhack->inner());
        }
    }

    bool disableLabels =
        Renderer::get()->isRecording() && !m_showLabelsWhileRendering->inner();

    bot->labels().update(disableLabels);
    bot->trajectory().update(PlayLayer::get());

    if (m_dynamicUpr->inner() && this->totalStepCount > 0) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double secondsPerStep =
            std::chrono::duration<double>(endTime - startTime).count() /
            this->totalStepCount;
        double dynamicDt = m_fpsTarget->inner() * secondsPerStep;
        // guard against non-finite results before the int cast
        if (std::isfinite(dynamicDt) && dynamicDt > 0.0) {
            m_stepLimit =
                std::max(1, static_cast<int>(std::floor(1.0 / dynamicDt)));
        }
    }

    if (Renderer::get()->m_shouldQuit && PlayLayer::get()) {
        PlayLayer::get()->onQuit();
        Renderer::get()->m_shouldQuit = false;
    }
}

void BotUpdater::breakLoop() {
    this->estimatedStepCount = 0;
    this->totalStepCount = 0;
    this->m_tpsOverflow = 0.0;
}

bool BotUpdater::isLockDelta() { return m_lockDelta->inner(); }

void BotUpdater::setFps(double fps) {
    if (fps <= 0.0) {
        return;
    }

    m_fps = fps;

    CCDirector::sharedDirector()->setAnimationInterval(1. / m_fps);

    auto gm = GameManager::sharedState();
    gm->m_customFPSTarget = m_fps;
    gm->setGameVariable("0116", true);
}

void BotUpdater::setSpeed(double speed) {
    if (speed <= 0.0) {
        return;
    }

    CCDirector::sharedDirector()->setAnimationInterval(1. / m_fps);
    auto gm = GameManager::sharedState();
    gm->m_customFPSTarget = m_fps;
    gm->setGameVariable("0116", true);
}

void BotUpdater::updateAudioSpeedhack() {
    FMOD::ChannelGroup* master;
    FMODAudioEngine::get()->m_system->getMasterChannelGroup(&master);

    if (m_speedhackAudio->inner()) {
        master->setPitch(m_speedhack->inner());
    } else {
        master->setPitch(1.0);
    }
}

void BotUpdater::runFrozenTick() {
    if (m_frozenScheduledFunctions.empty()) {
        return;  // no need to waste resources
    }

    for (auto& func : m_frozenScheduledFunctions) {
        func(0.0f);
    }

    runUpdates(
        [](float dt) {
            cocos2d::CCDirector::sharedDirector()->getScheduler()->update(dt);
        },
        0.0f, true);

    m_frozenScheduledFunctions.clear();
    // CCEGLView::get()->swapBuffers();
}

inline double BotUpdater::getVisualDt() {
    return 1. / (m_fps * m_speedhack->inner());
}

uint32_t BotUpdater::getFrame() {
    // Playing a level, with intentional death
    if (auto pl = PlayLayer::get(); pl) {
        return m_frame + m_frameOnLastAttempt;
    }

    // No intentional death in editor, use built in frame counter
    if (auto lel = LevelEditorLayer::get(); lel) {
        return static_cast<uint32_t>(std::max(
            0, static_cast<int>(lel->m_gameState.m_currentProgress / 2) - 1));
    }

    return 0;
}

bool BotUpdater::useFastLockDelta() {
    return this->m_lockDelta->inner() &&
           this->m_lockDeltaMode->inner() ==
               BotUpdater::LockDeltaMode::Performance &&
           Bot::get()->isPlaying() && !Renderer::get()->isRecording();
}

void BotUpdater::backwardsStep(int n /* = 1 */) {
    m_paused->inner() = true;

    if (n <= 0) {
        return;
    }

    if (m_backwardsStepping->inner()) {
        this->scheduleFrozenFunction([n](float) {
            auto pl = PlayLayer::get();
            auto bot = Bot::get();

            for (int i = 0;
                 i < n - 1 && bot->practiceFix().m_storedFrames.size() > 1;
                 i++) {
                bot->practiceFix().dropLastStoredFrame();
            }

            if (bot->practiceFix().m_storedFrames.empty()) {
                return;
            }
            bot->practiceFix().m_loadCheckpoint = true;
            bot->practiceFix().m_isBackstep = true;
            pl->resetLevel();
            bot->practiceFix().m_loadCheckpoint = false;
        });
    }
}

void BotUpdater::findBestFrameCandidate() {
    auto bot = Bot::get();
    auto pl = PlayLayer::get();
    if (!pl) return;
    if (!m_backwardsStepping->inner()) return;
    auto trajectory = bot->trajectory().unsafeInner();
    if (!trajectory) return;

    int bestPred = 0;
    uint32_t bestFrame = 0;
    uint32_t iters = 0;

    this->setPaused(true);

    int lowestAcceptable =
        static_cast<int>((float)trajectory->getPredictionLength() *
                         m_acceptablePrediction->inner());

    m_predicting = true;

    const uint32_t maxIters =
        std::max<uint32_t>(1u, bot->practiceFix().m_maxStoredFrames->inner());
    while (!pl->m_playerDied && iters < maxIters) {
        using Mode = SLSettings::TrajectorySettings::Mode;

        int mode = pl->m_player1->m_jumpBuffered ? Mode::Release : Mode::Hold;

        auto pred = trajectory->simulate(pl, true, Mode::Player1 | mode, false,
                                         {
                                             .m_bypassConfig = true,
                                         });
        if (pred.score > bestPred) {
            bestPred = pred.score;
            bestFrame = this->getFrame();
        }

        if (bestPred >= lowestAcceptable) {
            break;
        }

        this->stepOnce();
        CCScheduler::get()->update(this->getPhysicsDt());
        iters++;
    }

    uint32_t steps = std::min(this->getFrame() - bestFrame, iters);
    this->backwardsStep(steps);

    m_predicting = false;

    // this->stepOnce();
    // CCScheduler::get()->update(this->getPhysicsDt());
}

// THE tps bypass

// DEAR GEODE REVIEWER or whoever is reading this
//
// this does NOT conflict with geode's hooking system / other mods
// these specific places in memory will NOT be hooked by other mods
// (at least i don't think so)
//
// everything DOES work fine with this bypass
// anyways yea yapping over heres the code:::::::::::::
//
//
// addendum (15th july 2026)
// this code sucks
// there's one billion percent a better way to do this
// 1. put the frameUpdateMidhook as a regular hook of processCommands
// 2. use patches like a normal person
// 3. profit from free performance gain because you're not saving/restoring
// every register per frame
//
// yeah this is tiny perf impact but like
// i don't like it
// this bot is built upon hacks and hacks
// this is why i'm rewriting it
//
// and yes past natalie. everything works fine. but it works like shit

constexpr int ACTIONMGR_UPDATE_OFFSET = 0x38B90;
static void* CCActionManager_update;

$execute {
    CCActionManager_update = reinterpret_cast<void*>(geode::base::getCocos() +
                                                     ACTIONMGR_UPDATE_OFFSET);
}

static void backstepUpdateMidhook(SafetyHookContext&) {
    Bot* bot = Bot::get();
    if (bot->updater().m_onlyRefresh) return;

    PlayLayer* pl = PlayLayer::get();
    if (!pl) return;

    if (!pl->m_playerDied && bot->updater().m_backwardsStepping->inner() &&
        !Renderer::get()->isRecording()) {
        CheckpointObject* checkpoint = pl->createCheckpoint();
        checkpoint->retain();
        bot->practiceFix().saveState(
            checkpoint, Bot::get()->updater().m_frameOnLastAttempt);
    }
}

static void frameUpdateMidhook(SafetyHookContext&) {
    SCOPED_TIMER("frameUpdate")

    auto bot = Bot::get();
    if (!bot->isEnabled()) return;

    auto pl = GJBaseGameLayer::get();
    if (!pl) return;

    if (pl->m_resumeTimer > 0) {
        return;
    }

    // geode::log::info("le xmm13: {}", ctx.xmm13.f64[0]);

    PlayLayer* pll = PlayLayer::get();
    auto& updater = bot->updater();

    if (!pl->m_playerDied) {
        if (pll) {
            updater.incrementFrame();
            updater.m_timewarpTime +=
                updater.getPhysicsDt() * pll->m_gameState.m_timeWarp;
        }

        bot->cps().update();
        bot->hitboxes().saveToTrail(pl);

        auto trajectory = bot->trajectory().unsafeInner();
        if (trajectory && pll && updater.m_fullGamePrediction->inner() &&
            updater.m_preventDeath->inner()) {
            using Mode = SLSettings::TrajectorySettings::Mode;

            {
                int mode = Mode::Player1;
                mode |=
                    pl->m_player1->m_jumpBuffered ? Mode::Hold : Mode::Release;
                auto pred = trajectory->simulate(
                    pl, true, mode, false,
                    {.m_bypassConfig = true, .m_maxLength = 3});
                if (pred.score <= 3) {
                    bot->updater().setPaused(true);
                    // bot->updater().breakLoop();
                }

                if (bot->updater().m_autoFlipOnDeath->inner()) {
                    pl->queueButton(1, !pl->m_player1->m_jumpBuffered, false,
                                    0.0);
                    bot->updater().setPaused(false);
                }
            }

            if (pl->m_gameState.m_isDualMode) {
                int mode = Mode::Player2;
                mode |=
                    pl->m_player2->m_jumpBuffered ? Mode::Hold : Mode::Release;
                auto pred = trajectory->simulate(
                    pl, false, mode, false,
                    {.m_bypassConfig = true, .m_maxLength = 3});
                if (pred.score <= 3) {
                    bot->updater().setPaused(true);
                    // bot->updater().breakLoop();
                }

                if (bot->updater().m_autoFlipOnDeath->inner()) {
                    pl->queueButton(1, !pl->m_player2->m_jumpBuffered, true,
                                    0.0);
                    bot->updater().setPaused(false);
                }
            }
        }
    }

    if (pll) {
        bot->autoclicker().update(pll);
    }
    // bot->updater().runFrozenTick();
}

static void physDtMidhook(SafetyHookContext& ctx) {
    auto pl = GJBaseGameLayer::get();
    if (!pl) return;

    auto bot = Bot::get();
    if (!bot->isEnabled()) return;
    if (bot->updater().m_onlyRefresh) return;

    ctx.xmm1.f64[0] *= bot->updater().m_tps->inner() / 60.0;
    ctx.rip += 0x08;
}

// static void physStepCountMidhook(SafetyHookContext& ctx) {
//     auto pl = GJBaseGameLayer::get();
//     if (!pl) return;

//     auto bot = Bot::get();
//     bool fastBypass = bot->updater().useFastLockDelta() ||
//                       !bot->updater().m_lockDelta->inner();
//     if (!fastBypass && PlayLayer::get()) return;

//     // ctx.r11 = bot->updater().estimatedStepCount;
//     ctx.rdi = bot->updater().estimatedStepCount;
// }

// static void physStepCount2Midhook(SafetyHookContext& ctx) {
//     auto pl = GJBaseGameLayer::get();
//     if (!pl) return;

//     auto bot = Bot::get();
//     bool fastBypass = bot->updater().useFastLockDelta() ||
//                       !bot->updater().m_lockDelta->inner();
//     if (!fastBypass && PlayLayer::get()) return;

//     // ctx.r11 = std::min(bot->updater().estimatedStepCount, 1);
//     // ctx.rdx = std::min(bot->updater().estimatedStepCount, 1);
// }

static void physStepCountMidhook(SafetyHookContext& ctx) {
    auto bot = Bot::get();
    if (!bot->isEnabled()) return;
    bool fastBypass = bot->updater().useFastLockDelta() ||
                      !bot->updater().m_lockDelta->inner();
    if (!fastBypass && PlayLayer::get()) return;

    // to explain what's going on here;
    // basically the game does an incrementing step count up to but not
    // including 2
    //
    // the thing is, this can technically be negative
    // so for instance for a step count of 3
    // we wanna set it to -1
    // so the steps are gonna be: -1, 0, 1 respectively
    //
    // thank you robtop :mending_heart:
    ctx.rdx = 2 - bot->updater().estimatedStepCount;  // lmao
}

static void restorePhysDtHook(SafetyHookContext& ctx) {
    auto pl = GJBaseGameLayer::get();
    if (!pl) return;

    auto bot = Bot::get();
    if (!bot->isEnabled()) return;
    bool fastBypass = bot->updater().useFastLockDelta() ||
                      !bot->updater().m_lockDelta->inner();
    if (!fastBypass && PlayLayer::get()) return;

    double newDt =
        bot->updater().getPhysicsDt() * bot->updater().estimatedStepCount;

    ctx.xmm9.f64[0] = newDt;
}

$execute {
    util::midhook(geode::base::get() + 0x237A7C, "physDt", physDtMidhook);
    util::midhook(geode::base::get() + 0x237DCE, "physStepCount",
                  physStepCountMidhook);
    util::midhook(geode::base::get() + 0x238F6E, "restorePhysDt",
                  restorePhysDtHook);
    util::midhook(geode::base::get() + 0x237E42, "backstepUpdate",
                  backstepUpdateMidhook);
    util::midhook(geode::base::get() + 0x238BAA, "frameUpdate",
                  frameUpdateMidhook);

    Bot::get()->updater().m_speedhack->handle(
        [](double) { Bot::get()->updater().updateAudioSpeedhack(); });

    Bot::get()->updater().m_paused->handle([](bool& paused) {
        auto pl = PlayLayer::get();
        if (!pl || pl->m_isPaused || Renderer::get()->isRecording()) {
            paused = false;
        }
    });

    Bot::get()->updater().m_stepOnce->handle([](bool&) {
        auto& paused = Bot::get()->updater().m_paused;

        paused->inner() = true;
        paused->notifyChange();
    });

    Bot::get()->updater().m_stepBackwards->handle([](bool& paused) {
        auto pl = PlayLayer::get();
        if (pl && paused && Bot::get()->practiceFix().canRestoreState() &&
            !Renderer::get()->isRecording()) {
            Bot::get()->updater().backwardsStep();
        }

        paused = false;
    });

    Bot::get()->updater().m_noMirror->handle([](bool& noMirror) {
        auto pl = PlayLayer::get();
        if (!pl) return;
        if (noMirror) {
            pl->m_gameState.m_levelFlipping = 0.0;
        } else {
            pl->m_gameState.m_levelFlipping =
                (float)pl->m_gameState.m_unkBool10;
        }
    });

    Bot::get()->updater().m_predictBestPath->handle(
        [](bool&) { Bot::get()->updater().findBestFrameCandidate(); });
}
