#pragma once
#include "core/Model.hpp"
#include "core/Diagnostics.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <chrono>
#include <functional>

namespace fwl {
class Engine {
public:
    static Engine& get();
    Replay replay;
    Config config;
    std::vector<Row> rows;
    std::string status="Import a GDR or GDR2 replay.", fingerprint;
    std::filesystem::path reportPath, debugPath;
    bool loaded=false, active=false, driving=false, injecting=false, resetting=false, protectedRun=false;
    bool allowLevelMismatch=false;
    Tick tick=0, actualEnd=0;
    std::size_t currentRow=0, completedTrials=0;
    std::uint64_t revision=0;
    PlayLayer* layer=nullptr;

    void load(std::filesystem::path const&);
    void start(PlayLayer*,Config const&);
    void preview(PlayLayer*,std::size_t row,Tick offset);
    void stop(std::string reason="Cancelled; partial results saved.");
    void detach(PlayLayer*);
    void runScheduler(float realDt,std::function<void(float)> const& original);
    bool beforeCommands(GJBaseGameLayer*,float dt,bool half,bool last);
    void observedDelta(GJBaseGameLayer*,float input,double output);
    void died(PlayLayer*,PlayerObject*,GameObject*);
    void completed(PlayLayer*);
    void unexpectedReset(PlayLayer*);
    void saveReport(bool withDebug=true);
    void logIssue(std::string const&);
    std::string progress() const;
private:
    enum class Phase { Baseline, RowCheck, Scan, Removal, Preview } phase=Phase::Baseline;
    std::vector<Input> trial;
    std::vector<std::uint64_t> baseline;
    std::size_t cursor=0, modeCursor=0;
    int baselineRuns=0, repeat=0;
    Tick offset=0, firstChanged=0, trialStopped=0, timeoutTick=0;
    Tick previewOffset=0;
    std::size_t previewRow=0;
    bool pendingReset=false, trialDone=false, finishing=false, restartAfterPause=false;
    std::string analysisStatus;
    Verdict trialResult=Verdict::Fail;
    std::optional<Verdict> repeatedResult, repeatedLocal;
    Verdict trialLocalResult=Verdict::NotMeasured;
    Diagnostics diagnostic;
    Diagnostics::JSON sourceFileContext=Diagnostics::JSON::object();
    Diagnostics::JSON failureDetails=Diagnostics::JSON::object();
    int commandCalls=0,halfCalls=0,schedulerSteps=0;
    float commandDt=0,rawDelta=0;
    double modifiedDelta=0;
    bool lastCommand=false;
    Tick repeatedStop=0;
    bool originalTest=false, originalCBS=false, originalCOS=false, originalDontSave=false;
    std::uint64_t originalSeed=0;
    std::array<int,4> levelCounters{};
    double previewAccumulator=0;
    int idleUpdates=0;
    std::chrono::steady_clock::time_point started;
    void checkEnvironment(PlayLayer*);
    void beginDiagnostics(PlayLayer*);
    Diagnostics::JSON traceState() const;
    char const* phaseName() const;
    void captureEnvironment(PlayLayer*);
    void restoreEnvironment(bool reset);
    void prepareTrial();
    void finishTrial(Verdict,Tick);
    void consumeTrial();
    void finishJob();
    std::uint64_t snapshot() const;
};
void showLab(geode::Ref<PauseLayer> const& pause);
}
