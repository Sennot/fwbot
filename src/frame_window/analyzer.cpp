#include "analyzer.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <tuple>

#include "assist/autoclicker.hpp"
#include "bot/bot.hpp"
#include "bot/scheduler.hpp"
#include "bot/updater.hpp"
#include "replay/system.hpp"

using namespace geode::prelude;

namespace {
constexpr const char* FW_BUILD = "silicate-1.0.2-fw.1";
constexpr const char* REQUIRED_GEODE = "5.8.2";
constexpr const char* REQUIRED_GD = "2.2081";
#ifndef SILICATE_FW_GIT_SHA
#define SILICATE_FW_GIT_SHA "unknown"
#endif
constexpr const char* FW_GIT_SHA = SILICATE_FW_GIT_SHA;

bool containsOffset(const std::vector<int>& offsets, int value) {
    return std::find(offsets.begin(), offsets.end(), value) != offsets.end();
}

std::optional<double> findNumberForKeys(const std::string& text,
                                        const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
        const std::regex re("\\\"" + key +
                            "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)",
                            std::regex::icase);
        std::smatch match;
        if (std::regex_search(text, match, re) && match.size() >= 2) {
            try {
                return std::stod(match[1].str());
            } catch (...) {
            }
        }
    }
    return std::nullopt;
}

std::optional<bool> findBoolForKeys(const std::string& text,
                                    const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
        const std::regex re("\\\"" + key +
                                "\\\"\\s*:\\s*(true|false)",
                            std::regex::icase);
        std::smatch match;
        if (std::regex_search(text, match, re) && match.size() >= 2) {
            auto value = match[1].str();
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) {
                               return static_cast<char>(std::tolower(c));
                           });
            return value == "true";
        }
    }
    return std::nullopt;
}
}  // namespace

FrameWindowAnalyzer::FrameWindowAnalyzer() {
    m_sequenceVariants = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
}

bool FrameWindowAnalyzer::isActive() const {
    return m_phase == Phase::Baseline || m_phase == Phase::Individual ||
           m_phase == Phase::Sequence;
}

uint32_t FrameWindowAnalyzer::currentInputNumber() const {
    if (m_phase == Phase::Individual && m_resultCursor < m_results.size()) {
        return m_results[m_resultCursor].inputNumber;
    }
    if (m_phase == Phase::Sequence &&
        m_sequenceCursor < m_sequenceResults.size()) {
        return m_sequenceResults[m_sequenceCursor].firstInputNumber;
    }
    return 0;
}

void FrameWindowAnalyzer::clearResults() {
    if (isActive()) return;
    m_results.clear();
    m_sequenceResults.clear();
    m_completedTrials = 0;
    m_baselineCompletionFrame = 0;
    m_status = "Idle";
    m_error.clear();
    m_phase = Phase::Idle;
}

bool FrameWindowAnalyzer::start() {
    if (isActive()) return false;

    auto* pl = PlayLayer::get();
    auto* bot = Bot::get();
    if (!pl) {
        fail("Open a level before starting Frame Window analysis.");
        return false;
    }

    auto& rs = bot->replaySystem();
    if (rs.m_actionAtom.m_actions.empty()) {
        fail("No replay actions are loaded.");
        return false;
    }

    m_error.clear();
    m_status = "Preparing baseline";
    m_phase = Phase::Baseline;
    m_completedTrials = 0;
    m_baselineCompletionFrame = 0;
    m_baselineLastActionFrame = 0;
    m_currentTrial.reset();
    m_trialInstalled = false;
    m_forceFullReset = false;
    m_resetScheduled = false;
    m_runtimeCaptured = false;
    m_results.clear();
    m_sequenceResults.clear();
    m_selectedActionIndices.clear();
    m_recentEvents.clear();
    m_variableTps = false;

    std::filesystem::create_directories(diagnosticsDir());
    if (m_logStream.is_open()) m_logStream.close();
    m_logStream.open(latestLogPath(), std::ios::trunc);

    m_originalActions = rs.m_actionAtom.m_actions;
    normalizeOriginalActions();
    restoreOriginalActions();

    for (const auto& action : m_originalActions) {
        if (action.m_type == slc::Action::ActionType::Restart ||
            action.m_type == slc::Action::ActionType::RestartFull ||
            action.m_type == slc::Action::ActionType::Death) {
            fail("Frame Window analysis requires a clean one-attempt replay "
                 "without Death/Restart actions.");
            return false;
        }
        if (action.m_type == slc::Action::ActionType::TPS) {
            m_variableTps = true;
        }
    }

    uint32_t playerInputNumber = 0;
    for (size_t i = 0; i < m_originalActions.size(); ++i) {
        const auto& action = m_originalActions[i];
        m_baselineLastActionFrame =
            std::max<uint64_t>(m_baselineLastActionFrame, action.m_frame);
        if (!isPlayerAction(action)) continue;

        ++playerInputNumber;
        if (playerInputNumber < std::max<uint32_t>(1, m_config.firstInput)) {
            continue;
        }
        if (m_config.lastInput != 0 && playerInputNumber > m_config.lastInput) {
            continue;
        }

        m_selectedActionIndices.push_back(i);
        InputResult result;
        result.inputNumber = playerInputNumber;
        result.actionIndex = i;
        result.originalFrame = action.m_frame;
        result.player2 = action.m_player2;
        result.holding = action.m_holding;
        result.button = static_cast<int>(action.m_type);
        m_results.push_back(std::move(result));
    }

    if (m_results.empty()) {
        fail("The selected input range contains no player inputs.");
        return false;
    }

    m_savedSpeed = bot->updater().m_speedhack->inner();
    m_savedTps = bot->updater().m_tps->inner();
    m_analysisInitialTps = std::max(1.0, m_savedTps);
    m_savedRealTime = bot->updater().m_realTime->inner();
    m_savedVisualUpdates = bot->updater().m_useVisualUpdates->inner();
    m_savedIgnoreInputs = rs.m_ignoreInputs->inner();
    m_savedPaused = bot->updater().m_paused->inner();
    m_savedNoclip = bot->updater().m_noclip->inner();
    m_savedPreventDeath = bot->updater().m_preventDeath->inner();
    m_savedAutoFlipOnDeath = bot->updater().m_autoFlipOnDeath->inner();
    m_savedCanDie = bot->updater().m_canDie->inner();
    m_savedAutoclicker = bot->autoclicker().m_enabled->inner();
    m_savedMirrorInputs = rs.m_mirrorInputs;
    m_savedMaintainGravity = rs.m_maintainGravity;
    m_savedWasRecording = bot->isRecording();
    m_runtimeCaptured = true;

    bot->setMode(Bot::Playing);
    rs.m_ignoreInputs->inner() = true;
    rs.m_ignoreInputs->notifyChange();
    bot->updater().m_paused->inner() = false;
    bot->updater().m_paused->notifyChange();
    bot->updater().m_realTime->inner() = false;
    bot->updater().m_realTime->notifyChange();
    bot->updater().m_useVisualUpdates->inner() = false;
    bot->updater().m_useVisualUpdates->notifyChange();
    bot->updater().m_speedhack->inner() =
        std::clamp(m_config.analysisSpeed, 1.0, 1000.0);
    bot->updater().m_speedhack->notifyChange();
    bot->updater().m_noclip->inner() = false;
    bot->updater().m_noclip->notifyChange();
    bot->updater().m_preventDeath->inner() = false;
    bot->updater().m_preventDeath->notifyChange();
    bot->updater().m_autoFlipOnDeath->inner() = false;
    bot->updater().m_autoFlipOnDeath->notifyChange();
    bot->updater().m_canDie->inner() = false;
    bot->updater().m_canDie->notifyChange();
    bot->autoclicker().m_enabled->inner() = false;
    bot->autoclicker().m_enabled->notifyChange();
    rs.m_mirrorInputs = false;
    rs.m_maintainGravity = false;

    logEvent(fmt::format(
        "START build={} sha={} replay={} selected_inputs={} radius={} fail_margin={} "
        "sequence={} dense_gap={} speed={}x",
        FW_BUILD, FW_GIT_SHA, rs.m_replayName, m_results.size(), m_config.searchRadius,
        m_config.stopAfterFailures, m_config.sequenceContext,
        m_config.denseGap, m_config.analysisSpeed));
    if (m_variableTps) {
        logEvent(
            "WARNING replay contains TPS actions. FW trials still use Silicate "
            "playback correctly, but NaNDL has one global Window FPS; verify "
            "the exported calculator settings for variable-TPS macros.");
    }

    prepareBaseline();
    writeLatestState("analysis-start");
    scheduleFullReset();
    return true;
}

void FrameWindowAnalyzer::prepareBaseline() {
    m_phase = Phase::Baseline;
    m_status = "Baseline replay: waiting for a clean completion";
    m_currentTrial.reset();
    m_trialInstalled = false;
    restoreOriginalActions();
}

void FrameWindowAnalyzer::normalizeOriginalActions() {
    std::stable_sort(m_originalActions.begin(), m_originalActions.end(),
                     [](const slc::Action& a, const slc::Action& b) {
                         return a.m_frame < b.m_frame;
                     });
    uint64_t previous = 0;
    for (auto& action : m_originalActions) {
        action.recalculateDelta(previous);
        previous = action.m_frame;
    }
}

void FrameWindowAnalyzer::restoreOriginalActions() {
    auto& rs = Bot::get()->replaySystem();
    rs.m_actionAtom.m_actions = m_originalActions;
    rs.m_inputIndex = 0;
}

void FrameWindowAnalyzer::restoreRuntime() {
    if (!m_runtimeCaptured) return;
    auto* bot = Bot::get();
    auto& rs = bot->replaySystem();
    restoreOriginalActions();

    rs.m_ignoreInputs->inner() = m_savedIgnoreInputs;
    rs.m_ignoreInputs->notifyChange();
    bot->updater().m_speedhack->inner() = m_savedSpeed;
    bot->updater().m_speedhack->notifyChange();
    bot->updater().m_tps->inner() = m_savedTps;
    bot->updater().m_tps->notifyChange();
    bot->updater().m_realTime->inner() = m_savedRealTime;
    bot->updater().m_realTime->notifyChange();
    bot->updater().m_useVisualUpdates->inner() = m_savedVisualUpdates;
    bot->updater().m_useVisualUpdates->notifyChange();
    bot->updater().m_paused->inner() = m_savedPaused;
    bot->updater().m_paused->notifyChange();
    bot->updater().m_noclip->inner() = m_savedNoclip;
    bot->updater().m_noclip->notifyChange();
    bot->updater().m_preventDeath->inner() = m_savedPreventDeath;
    bot->updater().m_preventDeath->notifyChange();
    bot->updater().m_autoFlipOnDeath->inner() = m_savedAutoFlipOnDeath;
    bot->updater().m_autoFlipOnDeath->notifyChange();
    bot->updater().m_canDie->inner() = m_savedCanDie;
    bot->updater().m_canDie->notifyChange();
    bot->autoclicker().m_enabled->inner() = m_savedAutoclicker;
    bot->autoclicker().m_enabled->notifyChange();
    rs.m_mirrorInputs = m_savedMirrorInputs;
    rs.m_maintainGravity = m_savedMaintainGravity;
    bot->setMode(m_savedWasRecording ? Bot::Recording : Bot::Playing);
    m_runtimeCaptured = false;
}

void FrameWindowAnalyzer::cancel(const std::string& reason) {
    if (!isActive()) return;
    logEvent("CANCEL " + reason);
    restoreRuntime();
    m_currentTrial.reset();
    m_forceFullReset = false;
    m_resetScheduled = false;
    m_phase = Phase::Finished;
    m_status = reason;
    writeLatestState("cancelled");
    exportFullDiagnostics();
    if (m_logStream.is_open()) m_logStream.flush();
}

void FrameWindowAnalyzer::fail(const std::string& message) {
    m_error = message;
    m_status = "Error: " + message;
    m_phase = Phase::Error;
    logEvent("ERROR " + message);
    restoreRuntime();
    writeLatestState("error");
    exportFullDiagnostics();
    if (m_logStream.is_open()) m_logStream.flush();
    geode::log::error("[FrameWindow] {}", message);
}

void FrameWindowAnalyzer::finish() {
    restoreRuntime();
    m_phase = Phase::Finished;
    m_status = fmt::format("Done: {} inputs, {} trials", m_results.size(),
                           m_completedTrials);
    m_currentTrial.reset();
    m_forceFullReset = false;
    logEvent("FINISH " + m_status);
    exportNaNDL();
    exportFullDiagnostics();
    writeLatestState("finished");
    if (m_logStream.is_open()) m_logStream.flush();
}

void FrameWindowAnalyzer::scheduleFullReset() {
    if (m_resetScheduled) return;
    m_resetScheduled = true;
    Bot::get()->scheduler().schedule(
        [this]() {
            m_resetScheduled = false;
            auto* pl = PlayLayer::get();
            if (!pl || !isActive()) return;
            if (auto* ell = pl->getChildByID("EndLevelLayer")) {
                ell->removeFromParent();
            }
            pl->fullReset();
        },
        0.0, false);
}

bool FrameWindowAnalyzer::isPlayerAction(const slc::Action& action) const {
    const auto type = static_cast<int>(action.m_type);
    return type >= static_cast<int>(slc::Action::ActionType::Jump) &&
           type <= static_cast<int>(slc::Action::ActionType::Right);
}

std::string FrameWindowAnalyzer::detectGameMode(PlayerObject* player) {
    if (!player) return "Unknown";
    std::string mode = "Cube";
    if (player->m_isShip)
        mode = "Ship";
    else if (player->m_isBird)
        mode = "UFO";
    else if (player->m_isDart)
        mode = "Wave";
    else if (player->m_isSwing)
        mode = "Swing";
    else if (player->m_isBall)
        mode = "Ball";
    else if (player->m_isSpider)
        mode = "Spider";
    else if (player->m_isRobot)
        mode = "Robot";

    if (player->m_isPlatformer) {
        mode += " / Platformer";
    }
    return mode;
}

FrameWindowAnalyzer::PlayerSnapshot FrameWindowAnalyzer::snapshotPlayer(
    PlayerObject* player) {
    PlayerSnapshot snap;
    if (!player) return snap;
    snap.valid = true;
    const auto position = player->getPosition();
    snap.x = position.x;
    snap.y = position.y;
    snap.yVelocity = player->m_yVelocity;
    snap.xVelocity = player->m_platformerXVelocity;
    snap.gravity = player->m_gravityMod;
    snap.upsideDown = player->m_isUpsideDown;
    snap.platformer = player->m_isPlatformer;
    snap.mode = detectGameMode(player);
    return snap;
}

void FrameWindowAnalyzer::onReplayAction(size_t actionIndex,
                                         const slc::Action& action,
                                         GJBaseGameLayer* layer) {
    if (!isActive() || !isPlayerAction(action)) return;

    auto* player = action.m_player2 ? layer->m_player2 : layer->m_player1;
    if (m_phase == Phase::Baseline) {
        auto it = std::find_if(m_results.begin(), m_results.end(),
                               [actionIndex](const InputResult& result) {
                                   return result.actionIndex == actionIndex;
                               });
        if (it != m_results.end()) {
            it->gameMode = detectGameMode(player);
            it->tpsAtInput = Bot::get()->updater().getTps();
            if (player) {
                const auto position = player->getPosition();
                it->originalX = position.x;
                it->originalY = position.y;
            }
        }
    }

    const auto snap = snapshotPlayer(player);
    logEvent(fmt::format(
        "ACTION phase={} exec_index={} frame={} button={} state={} p={} "
        "mode={} x={:.5f} y={:.5f} vy={:.7f}",
        phaseName(m_phase), actionIndex, action.m_frame,
        buttonName(static_cast<int>(action.m_type)),
        action.m_holding ? "press" : "release", action.m_player2 ? 2 : 1,
        snap.mode, snap.x, snap.y, snap.yVelocity));
}

void FrameWindowAnalyzer::onFrame(uint64_t frame, GJBaseGameLayer* layer) {
    if (!isActive()) return;

    if (m_runStartFrame == 0 && frame <= 2) m_runStartFrame = frame;

    const double tps = std::max(1.0, Bot::get()->updater().getTps());
    if (m_phase == Phase::Baseline) {
        const uint64_t limit =
            m_baselineLastActionFrame + static_cast<uint64_t>(
                                            m_config.baselineTimeoutSeconds * tps);
        if (frame > limit) {
            fail("Baseline did not complete before the timeout. The replay may "
                 "not be a valid completion macro.");
        }
        return;
    }

    if (!m_currentTrial || !m_trialInstalled) return;
    const uint64_t limit =
        m_baselineCompletionFrame + static_cast<uint64_t>(
                                        m_config.trialTimeoutSeconds * tps);
    if (m_baselineCompletionFrame != 0 && frame > limit) {
        PlayerObject* player = layer ? layer->m_player1 : nullptr;
        auto record = makeOutcome(TrialOutcome::Timeout, frame,
                                  "Replay did not complete before trial timeout",
                                  player, nullptr);
        completeCurrentTrial(record);
        scheduleFullReset();
    }
}

FrameWindowAnalyzer::TrialRecord FrameWindowAnalyzer::makeOutcome(
    TrialOutcome outcome, uint64_t frame, const std::string& reason,
    PlayerObject* player, GameObject* object) const {
    TrialRecord record;
    if (m_currentTrial) record.offsets = m_currentTrial->offsets;
    record.outcome = outcome;
    record.endFrame = frame;
    record.reason = reason;
    record.deathObjectId = object ? object->m_objectID : 0;
    record.player = snapshotPlayer(player);
    return record;
}

void FrameWindowAnalyzer::onDeath(PlayerObject* player, GameObject* object) {
    if (!isActive()) return;
    const uint64_t frame = Bot::get()->updater().getFrame();
    if (m_phase == Phase::Baseline) {
        fail(fmt::format("Baseline replay died at frame {} (object {}).", frame,
                         object ? object->m_objectID : 0));
        return;
    }
    if (!m_currentTrial || !m_trialInstalled) return;

    auto record = makeOutcome(TrialOutcome::Death, frame, "Player died", player,
                              object);
    completeCurrentTrial(record);
    m_forceFullReset = isActive();
}

bool FrameWindowAnalyzer::onDelayedReset(PlayLayer* layer) {
    if (!isActive() || !m_forceFullReset) return false;
    m_forceFullReset = false;
    if (auto* ell = layer->getChildByID("EndLevelLayer")) {
        ell->removeFromParent();
    }
    layer->fullReset();
    return true;
}

bool FrameWindowAnalyzer::onLevelComplete(PlayLayer* layer) {
    if (!isActive()) return false;
    const uint64_t frame = Bot::get()->updater().getFrame();

    if (m_phase == Phase::Baseline) {
        m_baselineCompletionFrame = frame;
        logEvent(fmt::format("BASELINE PASS completion_frame={}", frame));
        m_resultCursor = 0;
        m_scanDirection = -1;
        m_scanStep = 1;
        m_consecutiveFailures = 0;
        m_earlyDone = false;
        m_lateDone = false;
        m_phase = Phase::Individual;
        m_status = fmt::format("Input {}/{}: searching early boundary",
                               m_resultCursor + 1, m_results.size());
        prepareNextIndividualTrial();
        if (isActive()) scheduleFullReset();
        return true;
    }

    if (!m_currentTrial || !m_trialInstalled) return true;
    PlayerObject* player = nullptr;
    if (!m_currentTrial->offsets.empty()) {
        const auto idx = m_currentTrial->offsets.front().first;
        if (idx < m_originalActions.size()) {
            player = m_originalActions[idx].m_player2 ? layer->m_player2
                                                      : layer->m_player1;
        }
    }
    auto record = makeOutcome(TrialOutcome::Pass, frame, "Level completed",
                              player, nullptr);
    completeCurrentTrial(record);
    if (isActive()) scheduleFullReset();
    return true;
}

void FrameWindowAnalyzer::onResetBegin(PlayLayer*) {
    if (!isActive()) return;

    if (m_phase == Phase::Baseline) {
        restoreOriginalActions();
        m_runStartFrame = 0;
        writeLatestState("baseline-reset");
        return;
    }

    if (!m_currentTrial) {
        if (m_phase == Phase::Individual) prepareNextIndividualTrial();
        if (m_phase == Phase::Sequence) prepareNextSequenceTrial();
    }
    if (!m_currentTrial || !isActive()) return;

    std::vector<slc::Action> trialActions;
    std::string invalidReason;
    while (m_currentTrial &&
           !makeTrialActions(*m_currentTrial, trialActions, invalidReason)) {
        auto invalid = makeOutcome(TrialOutcome::Invalid, 0, invalidReason);
        completeCurrentTrial(invalid);
        if (!isActive()) return;
        invalidReason.clear();
    }

    if (!m_currentTrial || !isActive()) return;
    if (trialActions.empty() &&
        !makeTrialActions(*m_currentTrial, trialActions, invalidReason)) {
        fail("Unable to construct trial replay: " + invalidReason);
        return;
    }

    auto& rs = Bot::get()->replaySystem();
    rs.m_actionAtom.m_actions = std::move(trialActions);
    rs.m_inputIndex = 0;
    m_trialInstalled = true;
    m_runStartFrame = 0;

    logEvent(fmt::format("TRIAL START phase={} label={} offsets={}",
                         phaseName(m_phase), m_currentTrial->label,
                         [&]() {
                             std::string out;
                             for (size_t i = 0;
                                  i < m_currentTrial->offsets.size(); ++i) {
                                 if (i) out += ",";
                                 out += fmt::format(
                                     "{}:{:+}",
                                     m_currentTrial->offsets[i].first,
                                     m_currentTrial->offsets[i].second);
                             }
                             return out;
                         }()));
    writeLatestState("trial-start");
}

bool FrameWindowAnalyzer::makeTrialActions(const TrialSpec& spec,
                                           std::vector<slc::Action>& out,
                                           std::string& invalidReason) const {
    out = m_originalActions;

    std::set<size_t> changed;
    for (const auto& [actionIndex, offset] : spec.offsets) {
        if (actionIndex >= out.size()) {
            invalidReason = "Action index outside replay";
            return false;
        }
        if (!isPlayerAction(out[actionIndex])) {
            invalidReason = "Attempted to offset a non-player action";
            return false;
        }
        const int64_t shifted = static_cast<int64_t>(out[actionIndex].m_frame) +
                                static_cast<int64_t>(offset);
        if (shifted < 0) {
            invalidReason = "Input would move before frame 0";
            return false;
        }
        out[actionIndex].m_frame = static_cast<uint64_t>(shifted);
        changed.insert(actionIndex);
    }

    // Reject physically impossible same-button sequences. The state before a
    // press/release is taken from the unchanged macro; crossing the opposite
    // state of the same player+button would no longer represent one input's
    // timing window.
    for (size_t changedIndex : changed) {
        const auto& changedAction = out[changedIndex];
        for (size_t i = 0; i < out.size(); ++i) {
            if (i == changedIndex) continue;
            const auto& other = out[i];
            if (!isPlayerAction(other)) continue;
            if (other.m_player2 != changedAction.m_player2 ||
                other.m_type != changedAction.m_type ||
                other.m_holding == changedAction.m_holding) {
                continue;
            }

            if (i < changedIndex && other.m_frame > changedAction.m_frame) {
                invalidReason = "Input crossed the previous opposite state";
                return false;
            }
            if (i > changedIndex && other.m_frame < changedAction.m_frame) {
                invalidReason = "Input crossed the next opposite state";
                return false;
            }
        }
    }

    std::stable_sort(out.begin(), out.end(),
                     [](const slc::Action& a, const slc::Action& b) {
                         return a.m_frame < b.m_frame;
                     });
    uint64_t previous = 0;
    for (auto& action : out) {
        action.recalculateDelta(previous);
        previous = action.m_frame;
    }
    return true;
}

void FrameWindowAnalyzer::completeCurrentTrial(const TrialRecord& record) {
    if (!m_currentTrial) return;
    m_trialInstalled = false;
    ++m_completedTrials;

    logEvent(fmt::format("TRIAL END phase={} label={} outcome={} frame={} "
                         "reason={} death_object={}",
                         phaseName(m_phase), m_currentTrial->label,
                         outcomeName(record.outcome), record.endFrame,
                         record.reason, record.deathObjectId));

    if (m_phase == Phase::Individual) {
        handleIndividualOutcome(record);
    } else if (m_phase == Phase::Sequence) {
        handleSequenceOutcome(record);
    }
    writeLatestState("trial-end");
}

void FrameWindowAnalyzer::prepareNextIndividualTrial() {
    while (m_phase == Phase::Individual && m_resultCursor < m_results.size()) {
        if (m_earlyDone && m_lateDone) {
            finalizeCurrentInput();
            continue;
        }

        if (!m_earlyDone && m_scanDirection != -1) {
            m_scanDirection = -1;
            m_scanStep = 1;
            m_consecutiveFailures = 0;
        }
        if (m_earlyDone && !m_lateDone && m_scanDirection != 1) {
            m_scanDirection = 1;
            m_scanStep = 1;
            m_consecutiveFailures = 0;
        }

        if (m_scanStep > std::max<uint32_t>(1, m_config.searchRadius)) {
            auto& result = m_results[m_resultCursor];
            const int boundary = m_scanDirection *
                                 static_cast<int>(m_config.searchRadius);
            if (containsOffset(result.passingOffsets, boundary)) {
                if (m_scanDirection < 0)
                    result.earlyTruncated = true;
                else
                    result.lateTruncated = true;
            }
            if (m_scanDirection < 0) {
                m_earlyDone = true;
                m_scanDirection = 1;
                m_scanStep = 1;
                m_consecutiveFailures = 0;
                continue;
            }
            m_lateDone = true;
            continue;
        }

        const int offset = m_scanDirection * static_cast<int>(m_scanStep);
        TrialSpec spec;
        spec.kind = TrialSpec::Kind::Individual;
        spec.resultIndex = m_resultCursor;
        spec.offsets = {{m_results[m_resultCursor].actionIndex, offset}};
        spec.label = fmt::format("input={} offset={:+}",
                                 m_results[m_resultCursor].inputNumber, offset);
        m_currentTrial = std::move(spec);
        m_status = fmt::format(
            "Input {}/{} (#{}): testing {:+} tick{}",
            m_resultCursor + 1, m_results.size(),
            m_results[m_resultCursor].inputNumber, offset,
            std::abs(offset) == 1 ? "" : "s");
        return;
    }

    if (m_phase == Phase::Individual && m_resultCursor >= m_results.size()) {
        if (m_config.sequenceContext) {
            startSequencePhase();
        } else {
            finish();
        }
    }
}

void FrameWindowAnalyzer::handleIndividualOutcome(const TrialRecord& record) {
    if (!m_currentTrial || m_resultCursor >= m_results.size()) return;
    auto& result = m_results[m_resultCursor];
    result.trials.push_back(record);

    int offset = 0;
    if (!m_currentTrial->offsets.empty()) {
        offset = m_currentTrial->offsets.front().second;
    }
    const bool passed = record.outcome == TrialOutcome::Pass;
    if (passed && !containsOffset(result.passingOffsets, offset)) {
        result.passingOffsets.push_back(offset);
    }

    ++m_scanStep;
    if (passed) {
        m_consecutiveFailures = 0;
    } else {
        ++m_consecutiveFailures;
    }

    if (m_consecutiveFailures >=
        std::max<uint32_t>(1, m_config.stopAfterFailures)) {
        if (m_scanDirection < 0) {
            m_earlyDone = true;
            m_scanDirection = 1;
            m_scanStep = 1;
            m_consecutiveFailures = 0;
        } else {
            m_lateDone = true;
        }
    }

    m_currentTrial.reset();
    prepareNextIndividualTrial();
}

void FrameWindowAnalyzer::finalizeCurrentInput() {
    if (m_resultCursor >= m_results.size()) return;
    auto& result = m_results[m_resultCursor];
    std::sort(result.passingOffsets.begin(), result.passingOffsets.end());
    result.passingOffsets.erase(
        std::unique(result.passingOffsets.begin(), result.passingOffsets.end()),
        result.passingOffsets.end());

    int early = 0;
    while (containsOffset(result.passingOffsets, early - 1)) --early;
    int late = 0;
    while (containsOffset(result.passingOffsets, late + 1)) ++late;

    result.earliestOffset = early;
    result.latestOffset = late;
    result.frameWindow = static_cast<uint32_t>(late - early + 1);
    result.discontinuous = std::any_of(
        result.passingOffsets.begin(), result.passingOffsets.end(),
        [early, late](int offset) { return offset < early || offset > late; });

    logEvent(fmt::format(
        "INPUT RESULT input={} frame={} fw={} early={:+} late={:+} "
        "discontinuous={} truncated_early={} truncated_late={} mode={}",
        result.inputNumber, result.originalFrame, result.frameWindow,
        result.earliestOffset, result.latestOffset, result.discontinuous,
        result.earlyTruncated, result.lateTruncated, result.gameMode));

    ++m_resultCursor;
    m_scanDirection = -1;
    m_scanStep = 1;
    m_consecutiveFailures = 0;
    m_earlyDone = false;
    m_lateDone = false;
}

bool FrameWindowAnalyzer::isOffsetPassing(const InputResult& result,
                                          int offset) const {
    return containsOffset(result.passingOffsets, offset);
}

void FrameWindowAnalyzer::buildSequencePairs() {
    m_sequenceResults.clear();
    if (m_results.size() < 2) return;

    uint32_t pairs = 0;
    for (size_t i = 0; i + 1 < m_results.size(); ++i) {
        const auto& first = m_results[i];
        const auto& second = m_results[i + 1];
        const uint64_t gap = second.originalFrame >= first.originalFrame
                                 ? second.originalFrame - first.originalFrame
                                 : first.originalFrame - second.originalFrame;
        if (gap > m_config.denseGap) continue;

        if (pairs >= m_config.maxSequencePairs) {
            logEvent(fmt::format(
                "SEQUENCE pair cap reached at {} pairs; remaining dense pairs "
                "are not contextual-tested",
                pairs));
            break;
        }

        SequenceResult seq;
        seq.firstInputNumber = first.inputNumber;
        seq.secondInputNumber = second.inputNumber;
        seq.firstResultIndex = i;
        seq.secondResultIndex = i + 1;
        seq.frameGap = gap;
        m_sequenceResults.push_back(std::move(seq));
        ++pairs;
    }
}

void FrameWindowAnalyzer::startSequencePhase() {
    buildSequencePairs();
    if (m_sequenceResults.empty()) {
        finish();
        return;
    }

    m_phase = Phase::Sequence;
    m_sequenceCursor = 0;
    m_sequenceVariantCursor = 0;
    m_status = fmt::format("Sequence context: pair 1/{}",
                           m_sequenceResults.size());
    prepareNextSequenceTrial();
}

void FrameWindowAnalyzer::prepareNextSequenceTrial() {
    while (m_phase == Phase::Sequence &&
           m_sequenceCursor < m_sequenceResults.size()) {
        if (m_sequenceVariantCursor >= m_sequenceVariants.size()) {
            auto& seq = m_sequenceResults[m_sequenceCursor];
            seq.coupled = seq.rescuedVariants > 0;
            if (seq.coupled) {
                auto& first = m_results[seq.firstResultIndex];
                auto& second = m_results[seq.secondResultIndex];
                first.coupled = true;
                second.coupled = true;
                first.coupledPassingVariants += seq.rescuedVariants;
                second.coupledPassingVariants += seq.rescuedVariants;
            }
            logEvent(fmt::format(
                "SEQUENCE RESULT inputs={}/{} gap={} tested={} pass={} "
                "rescued={} coupled={}",
                seq.firstInputNumber, seq.secondInputNumber, seq.frameGap,
                seq.testedVariants, seq.passingVariants, seq.rescuedVariants,
                seq.coupled));
            ++m_sequenceCursor;
            m_sequenceVariantCursor = 0;
            continue;
        }

        auto& seq = m_sequenceResults[m_sequenceCursor];
        const auto [a, b] = m_sequenceVariants[m_sequenceVariantCursor];
        TrialSpec spec;
        spec.kind = TrialSpec::Kind::Sequence;
        spec.sequenceIndex = m_sequenceCursor;
        spec.offsets = {
            {m_results[seq.firstResultIndex].actionIndex, a},
            {m_results[seq.secondResultIndex].actionIndex, b},
        };
        spec.label = fmt::format("pair={}/{} offsets={:+},{:+}",
                                 seq.firstInputNumber, seq.secondInputNumber, a,
                                 b);
        m_currentTrial = std::move(spec);
        m_status = fmt::format(
            "Sequence pair {}/{} (inputs #{} + #{}), variant {}/{}",
            m_sequenceCursor + 1, m_sequenceResults.size(),
            seq.firstInputNumber, seq.secondInputNumber,
            m_sequenceVariantCursor + 1, m_sequenceVariants.size());
        return;
    }

    if (m_phase == Phase::Sequence &&
        m_sequenceCursor >= m_sequenceResults.size()) {
        finish();
    }
}

void FrameWindowAnalyzer::handleSequenceOutcome(const TrialRecord& record) {
    if (!m_currentTrial || m_sequenceCursor >= m_sequenceResults.size()) return;
    auto& seq = m_sequenceResults[m_sequenceCursor];
    seq.trials.push_back(record);
    ++seq.testedVariants;

    if (record.outcome == TrialOutcome::Pass) {
        ++seq.passingVariants;
        if (m_currentTrial->offsets.size() == 2) {
            const int firstOffset = m_currentTrial->offsets[0].second;
            const int secondOffset = m_currentTrial->offsets[1].second;
            const bool firstIndividuallyPasses =
                isOffsetPassing(m_results[seq.firstResultIndex], firstOffset);
            const bool secondIndividuallyPasses =
                isOffsetPassing(m_results[seq.secondResultIndex], secondOffset);
            if (!firstIndividuallyPasses || !secondIndividuallyPasses) {
                ++seq.rescuedVariants;
            }
        }
    }

    ++m_sequenceVariantCursor;
    m_currentTrial.reset();
    prepareNextSequenceTrial();
}

void FrameWindowAnalyzer::onQuit() {
    if (isActive()) cancel("Analysis stopped because the level was closed.");
}

std::filesystem::path FrameWindowAnalyzer::diagnosticsDir() const {
    return Mod::get()->getPersistentDir(true) / "frame-window";
}

std::filesystem::path FrameWindowAnalyzer::latestLogPath() const {
    return diagnosticsDir() / "frame-window.log";
}

std::filesystem::path FrameWindowAnalyzer::latestStatePath() const {
    return diagnosticsDir() / "latest-trial.json";
}

std::filesystem::path FrameWindowAnalyzer::defaultNaNDLImportPath() const {
    return diagnosticsDir() / "nandl-import.json";
}

std::string FrameWindowAnalyzer::nowStamp() {
    const auto now = std::chrono::system_clock::now();
    return fmt::format("{:%Y%m%d_%H%M%S}", now);
}

void FrameWindowAnalyzer::logEvent(const std::string& text) {
    const auto line = fmt::format("{} | {}", nowStamp(), text);
    geode::log::info("[FrameWindow] {}", text);
    m_recentEvents.push_back(line);
    if (m_recentEvents.size() > 128) m_recentEvents.erase(m_recentEvents.begin());
    if (m_logStream.is_open()) {
        m_logStream << line << '\n';
        m_logStream.flush();
    }
}

std::string FrameWindowAnalyzer::jsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char c : value) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
        }
    }
    return out;
}

void FrameWindowAnalyzer::writeLatestState(const std::string& event) {
    std::filesystem::create_directories(diagnosticsDir());
    std::ofstream out(latestStatePath(), std::ios::trunc);
    if (!out) return;

    out << "{\n";
    out << "  \"build\": \"" << FW_BUILD << "\",\n";
    out << "  \"gitSha\": \"" << FW_GIT_SHA << "\",\n";
    out << "  \"phase\": \"" << phaseName(m_phase) << "\",\n";
    out << "  \"status\": \"" << jsonEscape(m_status) << "\",\n";
    out << "  \"event\": \"" << jsonEscape(event) << "\",\n";
    out << "  \"replayName\": \""
        << jsonEscape(Bot::get()->replaySystem().m_replayName) << "\",\n";
    out << "  \"completedTrials\": " << m_completedTrials << ",\n";
    out << "  \"currentInput\": " << currentInputNumber() << ",\n";
    out << "  \"baselineCompletionFrame\": " << m_baselineCompletionFrame
        << ",\n";
    out << "  \"trial\": ";
    if (!m_currentTrial) {
        out << "null\n";
    } else {
        out << "{\"label\": \"" << jsonEscape(m_currentTrial->label)
            << "\", \"offsets\": [";
        for (size_t i = 0; i < m_currentTrial->offsets.size(); ++i) {
            if (i) out << ", ";
            out << "{\"actionIndex\": " << m_currentTrial->offsets[i].first
                << ", \"offset\": " << m_currentTrial->offsets[i].second
                << "}";
        }
        out << "]}\n";
    }
    out << "}\n";
    out.flush();
}

std::filesystem::path FrameWindowAnalyzer::exportNaNDL() {
    std::filesystem::create_directories(diagnosticsDir());
    const auto path = diagnosticsDir() / "nandl-export.json";
    std::ofstream out(path, std::ios::trunc);
    if (!out) return {};

    const double gameFps = std::max(1.0, m_analysisInitialTps);
    const double windowFps = gameFps;

    // The current NaNDL calculator documents that its JSON contains rows,
    // Game FPS, Window FPS, respawn time and the frame-number toggle. The
    // calculator's exact field casing has changed during development, so this
    // exporter includes the canonical camelCase keys plus harmless aliases.
    out << "{\n";
    out << "  \"gameFPS\": " << gameFps << ",\n";
    out << "  \"gameFps\": " << gameFps << ",\n";
    out << "  \"windowFPS\": " << windowFps << ",\n";
    out << "  \"windowFps\": " << windowFps << ",\n";
    out << "  \"respawnTime\": " << m_config.respawnTime << ",\n";
    out << "  \"useFrameNumbers\": "
        << (m_config.exportFrameNumbers ? "true" : "false") << ",\n";
    out << "  \"silicateVariableTPS\": "
        << (m_variableTps ? "true" : "false") << ",\n";

    auto writeRows = [&](const char* key) {
        out << "  \"" << key << "\": [\n";
        for (size_t i = 0; i < m_results.size(); ++i) {
            const auto& r = m_results[i];
            const double time = m_config.exportFrameNumbers
                                    ? static_cast<double>(r.originalFrame)
                                    : static_cast<double>(r.originalFrame) /
                                          gameFps;
            out << "    {\"inputNumber\": " << r.inputNumber
                << ", \"number\": " << r.inputNumber
                << ", \"time\": " << std::setprecision(15) << time
                << ", \"timePosition\": " << time
                << ", \"frameWindow\": " << r.frameWindow
                << ", \"window\": " << r.frameWindow << "}";
            if (i + 1 < m_results.size()) out << ',';
            out << '\n';
        }
        out << "  ]";
    };

    writeRows("inputs");
    out << ",\n";
    writeRows("rows");
    out << "\n}\n";
    out.flush();
    logEvent("EXPORT NaNDL " + path.string());
    return path;
}

bool FrameWindowAnalyzer::importNaNDL(const std::filesystem::path& path) {
    if (isActive()) return false;
    std::ifstream in(path);
    if (!in) {
        m_status = "NaNDL import file not found: " + path.string();
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());

    if (auto value = findNumberForKeys(text, {"respawnTime", "respawn_time"}))
        m_config.respawnTime = *value;
    if (auto value = findBoolForKeys(text, {"useFrameNumbers", "frameNumbers"}))
        m_config.exportFrameNumbers = *value;

    std::vector<std::tuple<uint32_t, double, uint32_t>> rows;
    const std::regex objectRe("\\{[^{}]*\\}");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), objectRe);
         it != std::sregex_iterator(); ++it) {
        const std::string object = it->str();
        const auto fw = findNumberForKeys(
            object, {"frameWindow", "frame_window", "window", "fw"});
        const auto number = findNumberForKeys(
            object, {"inputNumber", "input_number", "number", "input"});
        const auto time = findNumberForKeys(
            object, {"timePosition", "time_position", "time", "frame"});
        if (!fw || !number || !time || *fw < 0.0 || *number < 1.0) continue;
        rows.emplace_back(static_cast<uint32_t>(std::llround(*number)), *time,
                          static_cast<uint32_t>(std::llround(*fw)));
    }

    if (rows.empty()) {
        m_status = "NaNDL import: no frame-window rows found";
        return false;
    }

    const double gameFps =
        findNumberForKeys(text, {"gameFPS", "gameFps", "game_fps"})
            .value_or(240.0);
    const bool frameNumbers =
        findBoolForKeys(text, {"useFrameNumbers", "frameNumbers"})
            .value_or(true);

    if (m_results.empty()) {
        m_results.reserve(rows.size());
        for (const auto& [number, time, fw] : rows) {
            InputResult r;
            r.inputNumber = number;
            r.originalFrame = static_cast<uint64_t>(std::llround(
                frameNumbers ? time : time * std::max(1.0, gameFps)));
            r.frameWindow = fw;
            r.imported = true;
            r.passingOffsets.clear();
            m_results.push_back(std::move(r));
        }
    } else {
        for (const auto& [number, time, fw] : rows) {
            auto it = std::find_if(m_results.begin(), m_results.end(),
                                   [number](const InputResult& r) {
                                       return r.inputNumber == number;
                                   });
            if (it == m_results.end()) continue;
            it->frameWindow = fw;
            it->imported = true;
        }
    }

    m_phase = Phase::Finished;
    m_status = fmt::format("Imported {} NaNDL rows", rows.size());
    logEvent("IMPORT NaNDL " + path.string());
    return true;
}

void FrameWindowAnalyzer::writeFullDiagnosticsTo(
    const std::filesystem::path& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;

    out << "{\n";
    out << "  \"schema\": 1,\n";
    out << "  \"build\": \"" << FW_BUILD << "\",\n";
    out << "  \"gitSha\": \"" << FW_GIT_SHA << "\",\n";
    out << "  \"requiredGeode\": \"" << REQUIRED_GEODE << "\",\n";
    out << "  \"requiredGD\": \"" << REQUIRED_GD << "\",\n";
    out << "  \"phase\": \"" << phaseName(m_phase) << "\",\n";
    out << "  \"status\": \"" << jsonEscape(m_status) << "\",\n";
    out << "  \"error\": \"" << jsonEscape(m_error) << "\",\n";
    out << "  \"replayName\": \""
        << jsonEscape(Bot::get()->replaySystem().m_replayName) << "\",\n";
    out << "  \"completedTrials\": " << m_completedTrials << ",\n";
    out << "  \"baselineCompletionFrame\": " << m_baselineCompletionFrame
        << ",\n";
    out << "  \"config\": {\"firstInput\": " << m_config.firstInput
        << ", \"lastInput\": " << m_config.lastInput
        << ", \"searchRadius\": " << m_config.searchRadius
        << ", \"stopAfterFailures\": " << m_config.stopAfterFailures
        << ", \"sequenceContext\": "
        << (m_config.sequenceContext ? "true" : "false")
        << ", \"denseGap\": " << m_config.denseGap
        << ", \"maxSequencePairs\": " << m_config.maxSequencePairs
        << ", \"analysisSpeed\": " << m_config.analysisSpeed << "},\n";
    out << "  \"analysisInitialTPS\": " << m_analysisInitialTps << ",\n";
    out << "  \"variableTPS\": " << (m_variableTps ? "true" : "false")
        << ",\n";

    out << "  \"originalReplayActions\": [\n";
    for (size_t i = 0; i < m_originalActions.size(); ++i) {
        const auto& a = m_originalActions[i];
        out << "    {\"index\": " << i << ", \"frame\": " << a.m_frame
            << ", \"type\": " << static_cast<int>(a.m_type)
            << ", \"button\": \""
            << jsonEscape(buttonName(static_cast<int>(a.m_type)))
            << "\", \"holding\": " << (a.m_holding ? "true" : "false")
            << ", \"player\": " << (a.m_player2 ? 2 : 1);
        if (a.m_type == slc::Action::ActionType::TPS) {
            out << ", \"tps\": " << a.m_tps;
        }
        out << "}";
        if (i + 1 < m_originalActions.size()) out << ',';
        out << '\n';
    }
    out << "  ],\n";

    out << "  \"inputs\": [\n";
    for (size_t i = 0; i < m_results.size(); ++i) {
        const auto& r = m_results[i];
        out << "    {\"inputNumber\": " << r.inputNumber
            << ", \"actionIndex\": " << r.actionIndex
            << ", \"frame\": " << r.originalFrame
            << ", \"player\": " << (r.player2 ? 2 : 1)
            << ", \"button\": \"" << buttonName(r.button)
            << "\", \"state\": \"" << (r.holding ? "press" : "release")
            << "\", \"gameMode\": \"" << jsonEscape(r.gameMode)
            << "\", \"x\": " << r.originalX << ", \"y\": "
            << r.originalY << ", \"tps\": " << r.tpsAtInput
            << ", \"frameWindow\": " << r.frameWindow
            << ", \"earliestOffset\": " << r.earliestOffset
            << ", \"latestOffset\": " << r.latestOffset
            << ", \"discontinuous\": "
            << (r.discontinuous ? "true" : "false")
            << ", \"earlyTruncated\": "
            << (r.earlyTruncated ? "true" : "false")
            << ", \"lateTruncated\": "
            << (r.lateTruncated ? "true" : "false")
            << ", \"coupled\": " << (r.coupled ? "true" : "false")
            << ", \"coupledPassingVariants\": "
            << r.coupledPassingVariants << ", \"imported\": "
            << (r.imported ? "true" : "false") << ", \"passingOffsets\": [";
        for (size_t j = 0; j < r.passingOffsets.size(); ++j) {
            if (j) out << ", ";
            out << r.passingOffsets[j];
        }
        out << "], \"trials\": [";
        for (size_t j = 0; j < r.trials.size(); ++j) {
            const auto& t = r.trials[j];
            if (j) out << ", ";
            out << "{\"outcome\": \"" << outcomeName(t.outcome)
                << "\", \"endFrame\": " << t.endFrame
                << ", \"reason\": \"" << jsonEscape(t.reason)
                << "\", \"deathObjectId\": " << t.deathObjectId
                << ", \"playerState\": {\"valid\": "
                << (t.player.valid ? "true" : "false")
                << ", \"x\": " << t.player.x << ", \"y\": " << t.player.y
                << ", \"yVelocity\": " << t.player.yVelocity
                << ", \"xVelocity\": " << t.player.xVelocity
                << ", \"gravity\": " << t.player.gravity
                << ", \"upsideDown\": "
                << (t.player.upsideDown ? "true" : "false")
                << ", \"platformer\": "
                << (t.player.platformer ? "true" : "false")
                << ", \"mode\": \"" << jsonEscape(t.player.mode)
                << "\"}, \"offsets\": [";
            for (size_t k = 0; k < t.offsets.size(); ++k) {
                if (k) out << ", ";
                out << "{\"actionIndex\": " << t.offsets[k].first
                    << ", \"offset\": " << t.offsets[k].second << "}";
            }
            out << "]}";
        }
        out << "]}";
        if (i + 1 < m_results.size()) out << ',';
        out << '\n';
    }
    out << "  ],\n";

    out << "  \"sequence\": [\n";
    for (size_t i = 0; i < m_sequenceResults.size(); ++i) {
        const auto& s = m_sequenceResults[i];
        out << "    {\"firstInput\": " << s.firstInputNumber
            << ", \"secondInput\": " << s.secondInputNumber
            << ", \"frameGap\": " << s.frameGap
            << ", \"testedVariants\": " << s.testedVariants
            << ", \"passingVariants\": " << s.passingVariants
            << ", \"rescuedVariants\": " << s.rescuedVariants
            << ", \"coupled\": " << (s.coupled ? "true" : "false")
            << ", \"trials\": [";
        for (size_t j = 0; j < s.trials.size(); ++j) {
            const auto& t = s.trials[j];
            if (j) out << ", ";
            out << "{\"outcome\": \"" << outcomeName(t.outcome)
                << "\", \"endFrame\": " << t.endFrame
                << ", \"reason\": \"" << jsonEscape(t.reason)
                << "\", \"deathObjectId\": " << t.deathObjectId
                << ", \"offsets\": [";
            for (size_t k = 0; k < t.offsets.size(); ++k) {
                if (k) out << ", ";
                out << "{\"actionIndex\": " << t.offsets[k].first
                    << ", \"offset\": " << t.offsets[k].second << "}";
            }
            out << "]}";
        }
        out << "]}";
        if (i + 1 < m_sequenceResults.size()) out << ',';
        out << '\n';
    }
    out << "  ],\n";

    out << "  \"recentEvents\": [\n";
    for (size_t i = 0; i < m_recentEvents.size(); ++i) {
        out << "    \"" << jsonEscape(m_recentEvents[i]) << "\"";
        if (i + 1 < m_recentEvents.size()) out << ',';
        out << '\n';
    }
    out << "  ]\n";
    out << "}\n";
}

std::filesystem::path FrameWindowAnalyzer::exportFullDiagnostics() {
    std::filesystem::create_directories(diagnosticsDir());
    const auto path = diagnosticsDir() / "frame-window-debug.json";
    writeFullDiagnosticsTo(path);
    return path;
}

std::string FrameWindowAnalyzer::phaseName(Phase phase) {
    switch (phase) {
        case Phase::Idle:
            return "Idle";
        case Phase::Baseline:
            return "Baseline";
        case Phase::Individual:
            return "Individual";
        case Phase::Sequence:
            return "Sequence";
        case Phase::Finished:
            return "Finished";
        case Phase::Error:
            return "Error";
    }
    return "Unknown";
}

std::string FrameWindowAnalyzer::outcomeName(TrialOutcome outcome) {
    switch (outcome) {
        case TrialOutcome::Pass:
            return "PASS";
        case TrialOutcome::Death:
            return "DEATH";
        case TrialOutcome::Timeout:
            return "TIMEOUT";
        case TrialOutcome::Invalid:
            return "INVALID";
        case TrialOutcome::Cancelled:
            return "CANCELLED";
    }
    return "UNKNOWN";
}

std::string FrameWindowAnalyzer::buttonName(int button) {
    switch (button) {
        case 1:
            return "Jump";
        case 2:
            return "Left";
        case 3:
            return "Right";
        default:
            return "Unknown";
    }
}
