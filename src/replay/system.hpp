#ifndef REPLAY_SYSTEM_HPP
#define REPLAY_SYSTEM_HPP

#include <slc/slc.hpp>
#include <unordered_map>

#include "../settings/settings.hpp"
#include "../shared/value/value.hpp"
#include "bot/bot.hpp"
#include "bot/scheduler.hpp"

struct ReplayMeta {
    uint64_t seed;
    char reserved[56];
};

class ReplaySystem {
   private:
    void processSlc3(slc::v3::Replay<>& replay);
    void processSlc2(slc::v2::Replay<ReplayMeta>& replay);

   public:
    size_t m_inputIndex;

    slc::ActionAtom m_actionAtom;
    bool m_collectInputs = true;
    SLValuePtr<bool> m_ignoreInputs = SLValue<bool>::create(
        "replay.ignore_inputs", &SLSettings::get()->blockInputs);
    SLValuePtr<bool> m_autosaveAtLevelEnd = SLValue<bool>::create(
        "replay.autosave", &SLSettings::get()->autosaveAtLevelEnd);

    SLValuePtr<bool> m_useAlternateHook = SLValue<bool>::create(
        "replay.althook", &SLSettings::get()->useAlternateHook);

    bool m_overrideSeed = false;
    uint64_t m_overriddenSeed = 2137;
    uint64_t m_startingSeed = 0;
    uint64_t m_startingSeedThisAttempt = 0;
    uint64_t m_shakeRandomState = 0;
    uint64_t m_teleportRandomState = 0;
    double m_initialTPS = 240.0;
    bool m_flipProcessingInputs = false;

    bool m_forceNextInput = false;
    std::unordered_map<int, slc::Action> m_lastInputs;

    bool m_mirrorInputs = false;
    bool m_mirrorInverted = false;
    bool m_maintainGravity = false;

    std::string m_replayName = "";

    BotScheduler::JobId m_autosaveId;
    SLValuePtr<bool> m_autosaveAtInterval = SLValue<bool>::create(
        "replay.autosave_at_interval", &SLSettings::get()->autosaveAtInterval);
    SLValuePtr<double> m_autosaveInterval = SLValue<double>::create(
        "replay.autosave_interval", &SLSettings::get()->autosaveInterval);

    size_t getInputIndex() const { return m_inputIndex; }
    void onReset(uint32_t newFrame);
    void onExit();

    void advanceInputIndex() { m_inputIndex++; }
    bool hasFlippedControls() {
        return GameManager::get()->getGameVariable("0010");
    }
    bool playerFlipped(bool player) { return player ^ hasFlippedControls(); }

    uint64_t& getCurrentRandomState();
    uint64_t& getCurrentShakeState();

    [[nodiscard]] const std::optional<slc::Action> getNextQueuedInput() {
        if (m_inputIndex + 1 >= m_actionAtom.m_actions.size())
            return std::nullopt;

        return m_actionAtom.m_actions.at(m_inputIndex + 1);
    }

    [[nodiscard]] const std::optional<slc::Action> getCurrentQueuedInput() {
        if (m_inputIndex >= m_actionAtom.m_actions.size()) return std::nullopt;

        return m_actionAtom.m_actions.at(m_inputIndex);
    }

    [[nodiscard]] const std::optional<slc::Action> getNextInput(uint32_t frame);

    void load(std::filesystem::path path);
    void save(std::filesystem::path path, bool noOverwrite = false);

    std::filesystem::path getCurrentPath();
    void backupExisting(std::filesystem::path path);
    void createBackup();
};

#endif  // REPLAY_SYSTEM_HPP
