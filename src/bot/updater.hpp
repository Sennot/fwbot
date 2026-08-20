#ifndef UPDATER_HPP
#define UPDATER_HPP

#include <Windows.h>

#include <forward_list>
#include <functional>

#include "settings/settings.hpp"
#include "shared/value/value.hpp"

constexpr uintptr_t TPS_STEPS_OFFSET = 0x6074e0;

class BotUpdater {
   private:
    double m_fps = 240.0;
    uint32_t m_frame = 0;

    bool _stepOnce = false;
    bool _stepBackwards = false;
    bool _paused = false;
    bool _intentionalDeath = false;
    bool _noMirror = false;
    bool _preventDeath = false;
    bool _autoFlipOnDeath = false;
    bool _predictBestPath = false;
    bool _extrapolation = false;
    bool _layoutMode = false;
    bool _noclip = false;

    std::forward_list<std::function<void(float)>> m_frozenScheduledFunctions;

   public:
    enum LockDeltaMode { Performance, Accuracy };

    int m_respawnTimer = 0;
    bool m_haltActions = false;
    bool m_onlyRefresh = false;
    void* m_actionMgr = 0;

    double m_timewarpTime = 0.0;

    SLValuePtr<bool> m_stepOnce =
        SLValue<bool>::create("updater.advance_one", &_stepOnce);

    SLValuePtr<bool> m_stepBackwards =
        SLValue<bool>::create("updater.advance_back", &_stepBackwards);

    SLValuePtr<double> m_tps =
        SLValue<double>::create("updater.tps", &SLSettings::get()->tps);

    SLValuePtr<double> m_speedhack =
        SLValue<double>::create("updater.speedhack", &SLSettings::get()->speed);

    SLValuePtr<bool> m_paused =
        SLValue<bool>::create("updater.frame_advance", &_paused);

    SLValuePtr<bool> m_realTime = SLValue<bool>::create(
        "updater.real_time", &SLSettings::get()->realTime);
    SLValuePtr<uint32_t> m_maxUPR = SLValue<uint32_t>::create(
        "updater.max_upr", &SLSettings::get()->maxUpr);
    uint32_t m_stepLimit = 1;
    SLValuePtr<bool> m_useVisualUpdates = SLValue<bool>::create(
        "updater.visual_updates", &SLSettings::get()->useVisualUpdates);
    SLValuePtr<bool> m_dynamicUpr = SLValue<bool>::create(
        "updater.dynamic_upr", &SLSettings::get()->dynamicUpr);
    SLValuePtr<double> m_fpsTarget = SLValue<double>::create(
        "updater.fps_target", &SLSettings::get()->fpsTarget);

    SLValuePtr<bool> m_layoutMode =
        SLValue<bool>::create("updater.layout_mode", &_layoutMode);
    SLValuePtr<bool> m_useRegularBg = SLValue<bool>::create(
        "updater.layout_use_bg", &SLSettings::get()->useRegularBg);

    double m_tpsOverflow = 0.0;
    bool m_shouldRender = true;

    SLValuePtr<bool> m_lockDelta = SLValue<bool>::create(
        "updater.lock_delta", &SLSettings::get()->lockDelta);

    bool m_allowedToProcessActions = true;

    SLValuePtr<int> m_lockDeltaMode = SLValue<int>::create(
        "updater.lock_delta_mode", &SLSettings::get()->lockDeltaMode);

    SLValuePtr<bool> m_speedhackAudio = SLValue<bool>::create(
        "updater.speedhack_audio", &SLSettings::get()->speedhackAudio);

    SLValuePtr<bool> m_showLabelsWhileRendering =
        SLValue<bool>::create("updater.show_labels_while_rendering",
                              &SLSettings::get()->showLabelsWhileRendering);

    int savedStepCount = 0;
    int totalStepCount = 0;
    int estimatedStepCount = 1;
    float currentDelta = 0.0;

    // Whether the player can die in the macro or not, adds a "Death" or
    // "Restart" input
    SLValuePtr<bool> m_canDie =
        SLValue<bool>::create("updater.intentional_death", &_intentionalDeath);
    SLValuePtr<bool> m_ssbFix = SLValue<bool>::create(
        "updater.ssb_fix", &SLSettings::get()->scrollSpeedBugFix);

    SLValuePtr<bool> m_backwardsStepping = SLValue<bool>::create(
        "updater.backwards_stepping", &SLSettings::get()->backwardsStepping);
    bool m_inputIsDeath = false;
    bool m_fullReset = false;
    float m_lastTfp = 0.0f;

    SLValuePtr<bool> m_extrapolateFrames =
        SLValue<bool>::create("updater.frame_extrapolation", &_extrapolation);
    cocos2d::CCPoint m_lastCameraPos;
    cocos2d::CCPoint m_currentCameraPos;

    float m_lastPlayerX;
    float m_currentPlayerX;
    float m_currentPlayerRot;

    // Whether the current input is Death
    bool m_expectsDeath = false;
    uint64_t m_frameOnLastAttempt = 0;

    SLValuePtr<bool> m_noMirror =
        SLValue<bool>::create("updater.no_mirror", &_noMirror);

    SLValuePtr<bool> m_preventDeath =
        SLValue<bool>::create("updater.prevent_death", &_preventDeath);
    SLValuePtr<bool> m_autoFlipOnDeath =
        SLValue<bool>::create("updater.auto_flip_on_death", &_autoFlipOnDeath);

    SLValuePtr<bool> m_predictBestPath =
        SLValue<bool>::create("updater.predict_best_path", &_predictBestPath);
    SLValuePtr<bool> m_fullGamePrediction = SLValue<bool>::create(
        "updater.full_game_prediction", &SLSettings::get()->fullGamePrediction);
    SLValuePtr<float> m_acceptablePrediction =
        SLValue<float>::create("updater.acceptable_prediction",
                               &SLSettings::get()->acceptablePrediction);

    enum class NoclipType { Player1, Player2, Both };

    SLValuePtr<bool> m_noclip =
        SLValue<bool>::create("updater.noclip", &_noclip);
    NoclipType m_noclipType = NoclipType::Player1;

    bool m_isAutoFlipped = false;
    bool m_predicting = false;

    cocos2d::CCLabelBMFont* m_frameLabel = nullptr;

    float getSSB();
    float getTimeWarp();

    void calculateSteps(float dt, float targetDt);
    void breakLoop();
    void runUpdates(std::function<void(float)> update, float realDt,
                    bool frozen);
    void runFrozenTick();

    void scheduleFrozenFunction(std::function<void(float)> func) {
        m_frozenScheduledFunctions.push_front(func);
    }

    void setFps(double fps);
    void setSpeed(double speed);
    void setTps(double tps) {
        if (tps <= 0.0) return;

        // double tpsSteps = std::max(tps / 60.0, 4.0);

        // uintptr_t addr = geode::base::get() + TPS_STEPS_OFFSET;
        // DWORD oldProtect;

        // VirtualProtect((void*)addr, sizeof(double), PAGE_EXECUTE_READWRITE,
        // &oldProtect);
        // *(double*)addr = tpsSteps;
        // VirtualProtect((void*)addr, sizeof(double), oldProtect, &oldProtect);

        m_tps->inner() = tps;
    }
    void setPaused(bool paused) { m_paused->inner() = paused; }
    void togglePaused() { m_paused->inner() = !m_paused->inner(); }
    void stepOnce() { m_stepOnce->inner() = true; }
    bool consumeStep() {
        bool shouldStep = m_stepOnce->inner();
        m_stepOnce->inner() = false;
        return shouldStep;
    }

    void backwardsStep(int n = 1);

    void updateAudioSpeedhack();

    inline double getTps() const { return m_tps->inner(); }
    inline double getDt() { return 1. / m_fps; }
    inline double getVisualDt();
    inline double getPhysicsDt() { return 1. / m_tps->inner(); };

    [[nodiscard]] uint32_t getFrame();

    /**
     * ONLY USE IF YOU KNOW WHAT YOU ARE DOING.
     * Incrementing the frame WILL forward time in the macro.
     */
    void incrementFrame() { m_frame++; }
    void resetFrame() {
        m_frame = 0;
        m_timewarpTime = 0.0;
    }
    void setFrame(uint32_t frame) { m_frame = frame; }

    bool isPaused() { return m_paused->inner(); }
    bool isLockDelta();
    bool useFastLockDelta();

    void findBestFrameCandidate();
};

#endif  // UPDATER_HPP
