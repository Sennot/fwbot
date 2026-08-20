#pragma once

#include <Geode/Geode.hpp>
#include <slc/slc.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class GJBaseGameLayer;
class PlayLayer;
class PlayerObject;
class GameObject;

class FrameWindowAnalyzer {
   public:
    enum class Phase {
        Idle,
        Baseline,
        Individual,
        Sequence,
        Finished,
        Error,
    };

    enum class TrialOutcome {
        Pass,
        Death,
        Timeout,
        Invalid,
        Cancelled,
    };

    struct Config {
        uint32_t firstInput = 1;
        uint32_t lastInput = 0;  // 0 = all
        uint32_t searchRadius = 12;
        uint32_t stopAfterFailures = 2;
        bool sequenceContext = true;
        uint32_t denseGap = 6;
        uint32_t maxSequencePairs = 128;
        double analysisSpeed = 20.0;
        double baselineTimeoutSeconds = 30.0;
        double trialTimeoutSeconds = 8.0;
        bool exportFrameNumbers = true;
        double respawnTime = 0.0;
    };

    struct PlayerSnapshot {
        bool valid = false;
        double x = 0.0;
        double y = 0.0;
        double yVelocity = 0.0;
        double xVelocity = 0.0;
        double gravity = 0.0;
        bool upsideDown = false;
        bool platformer = false;
        std::string mode = "Unknown";
    };

    struct TrialRecord {
        std::vector<std::pair<size_t, int>> offsets;
        TrialOutcome outcome = TrialOutcome::Invalid;
        uint64_t endFrame = 0;
        std::string reason;
        int deathObjectId = 0;
        PlayerSnapshot player;
    };

    struct InputResult {
        uint32_t inputNumber = 0;
        size_t actionIndex = 0;
        uint64_t originalFrame = 0;
        bool player2 = false;
        bool holding = false;
        int button = 0;

        std::string gameMode = "Unknown";
        double originalX = 0.0;
        double originalY = 0.0;
        double tpsAtInput = 240.0;

        std::vector<int> passingOffsets = {0};
        std::vector<TrialRecord> trials;

        int earliestOffset = 0;
        int latestOffset = 0;
        uint32_t frameWindow = 1;
        bool discontinuous = false;
        bool earlyTruncated = false;
        bool lateTruncated = false;
        bool coupled = false;
        uint32_t coupledPassingVariants = 0;
        bool imported = false;
    };

    struct SequenceResult {
        uint32_t firstInputNumber = 0;
        uint32_t secondInputNumber = 0;
        size_t firstResultIndex = 0;
        size_t secondResultIndex = 0;
        uint64_t frameGap = 0;
        uint32_t testedVariants = 0;
        uint32_t passingVariants = 0;
        uint32_t rescuedVariants = 0;
        bool coupled = false;
        std::vector<TrialRecord> trials;
    };

    FrameWindowAnalyzer();

    Config m_config;

    bool start();
    void cancel(const std::string& reason = "Cancelled by user");
    void clearResults();

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool hasResults() const { return !m_results.empty(); }
    [[nodiscard]] Phase phase() const { return m_phase; }
    [[nodiscard]] const std::string& status() const { return m_status; }
    [[nodiscard]] const std::string& error() const { return m_error; }
    [[nodiscard]] const std::vector<InputResult>& results() const {
        return m_results;
    }
    [[nodiscard]] const std::vector<SequenceResult>& sequenceResults() const {
        return m_sequenceResults;
    }
    [[nodiscard]] uint32_t currentInputNumber() const;
    [[nodiscard]] uint32_t totalInputCount() const {
        return static_cast<uint32_t>(m_results.size());
    }
    [[nodiscard]] uint64_t completedTrials() const { return m_completedTrials; }
    [[nodiscard]] uint64_t baselineCompletionFrame() const {
        return m_baselineCompletionFrame;
    }

    // Hooks called by existing Silicate gameplay code.
    void onReplayAction(size_t actionIndex, const slc::Action& action,
                        GJBaseGameLayer* layer);
    void onFrame(uint64_t frame, GJBaseGameLayer* layer);
    void onDeath(PlayerObject* player, GameObject* object);
    bool onLevelComplete(PlayLayer* layer);
    void onResetBegin(PlayLayer* layer);
    bool onDelayedReset(PlayLayer* layer);
    void onQuit();

    // Export / import helpers. NaNDL export intentionally contains only
    // canonical FW data plus common schema aliases so the file remains easy
    // to inspect and resilient to minor calculator schema changes.
    std::filesystem::path exportNaNDL();
    std::filesystem::path exportFullDiagnostics();
    bool importNaNDL(const std::filesystem::path& path);

    [[nodiscard]] std::filesystem::path diagnosticsDir() const;
    [[nodiscard]] std::filesystem::path latestLogPath() const;
    [[nodiscard]] std::filesystem::path latestStatePath() const;
    [[nodiscard]] std::filesystem::path defaultNaNDLImportPath() const;

    static std::string phaseName(Phase phase);
    static std::string outcomeName(TrialOutcome outcome);
    static std::string buttonName(int button);

   private:
    struct TrialSpec {
        enum class Kind { Individual, Sequence } kind = Kind::Individual;
        std::vector<std::pair<size_t, int>> offsets;
        size_t resultIndex = 0;
        size_t sequenceIndex = 0;
        std::string label;
    };

    Phase m_phase = Phase::Idle;
    std::string m_status = "Idle";
    std::string m_error;

    std::vector<slc::Action> m_originalActions;
    std::vector<size_t> m_selectedActionIndices;
    std::vector<InputResult> m_results;
    std::vector<SequenceResult> m_sequenceResults;

    std::optional<TrialSpec> m_currentTrial;
    bool m_trialInstalled = false;
    bool m_forceFullReset = false;
    bool m_resetScheduled = false;
    bool m_runtimeCaptured = false;

    uint64_t m_runStartFrame = 0;
    uint64_t m_baselineCompletionFrame = 0;
    uint64_t m_baselineLastActionFrame = 0;
    uint64_t m_completedTrials = 0;

    // Individual scan state.
    size_t m_resultCursor = 0;
    int m_scanDirection = -1;
    uint32_t m_scanStep = 1;
    uint32_t m_consecutiveFailures = 0;
    bool m_earlyDone = false;
    bool m_lateDone = false;

    // Sequence scan state.
    size_t m_sequenceCursor = 0;
    size_t m_sequenceVariantCursor = 0;
    std::vector<std::pair<int, int>> m_sequenceVariants;

    // Saved runtime settings restored after analysis.
    double m_savedSpeed = 1.0;
    double m_savedTps = 240.0;
    double m_analysisInitialTps = 240.0;
    bool m_savedRealTime = true;
    bool m_savedVisualUpdates = true;
    bool m_savedIgnoreInputs = true;
    bool m_savedPaused = false;
    bool m_savedNoclip = false;
    bool m_savedPreventDeath = false;
    bool m_savedAutoFlipOnDeath = false;
    bool m_savedCanDie = false;
    bool m_savedAutoclicker = false;
    bool m_savedMirrorInputs = false;
    bool m_savedMaintainGravity = false;
    bool m_savedWasRecording = true;
    bool m_variableTps = false;

    std::vector<std::string> m_recentEvents;
    std::ofstream m_logStream;

    void fail(const std::string& message);
    void finish();
    void restoreRuntime();
    void restoreOriginalActions();
    void normalizeOriginalActions();

    void scheduleFullReset();
    void prepareBaseline();
    void prepareNextIndividualTrial();
    void handleIndividualOutcome(const TrialRecord& record);
    void finalizeCurrentInput();
    void startSequencePhase();
    void buildSequencePairs();
    void prepareNextSequenceTrial();
    void handleSequenceOutcome(const TrialRecord& record);

    [[nodiscard]] bool makeTrialActions(const TrialSpec& spec,
                                        std::vector<slc::Action>& out,
                                        std::string& invalidReason) const;
    [[nodiscard]] bool isPlayerAction(const slc::Action& action) const;
    [[nodiscard]] bool isOffsetPassing(const InputResult& result,
                                       int offset) const;

    TrialRecord makeOutcome(TrialOutcome outcome, uint64_t frame,
                            const std::string& reason,
                            PlayerObject* player = nullptr,
                            GameObject* object = nullptr) const;
    void completeCurrentTrial(const TrialRecord& record);

    static PlayerSnapshot snapshotPlayer(PlayerObject* player);
    static std::string detectGameMode(PlayerObject* player);

    void logEvent(const std::string& text);
    void writeLatestState(const std::string& event = "");
    void writeFullDiagnosticsTo(const std::filesystem::path& path) const;
    static std::string jsonEscape(const std::string& value);
    static std::string nowStamp();
};
