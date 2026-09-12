#include "Model.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
namespace fwl {
using json=nlohmann::json;
std::string reportJSON(Replay const&r,Config const&c,std::vector<Row> const&rows,std::string const&status,std::string const&fingerprint,Tick actualEnd){
    json out={{"schema","frame-window-lab/2"},{"status",status},{"levelFingerprint",fingerprint},
        {"game","2.2081"},{"geodeSDK","5.8.2"},{"physics","240 TPS; native whole-step input; no CBF or corrections"},
        {"windowMeaning","Two observation horizons: local survival to localEndTick, and conditional survival to goalEndTick. Neither proves independently achievable geometric precision."},
        {"legacyWindowField","windowAtOriginalInput is the goal window, not the local window"},
        {"boundaryConvention","End tick H requires alive at the start of H; collision at reported boundary H is during tick H-1 and fails."},
        {"baselineCheck","Repeated per-tick fingerprints of active players, clocks and RNG; not a proof of full hidden-state determinism"},
        {"macro",{{"file",r.source},{"format",r.format},{"bot",r.bot},{"author",r.author},{"levelName",r.levelName},{"levelID",r.levelID},{"seed",r.seed},{"tps",r.tps},{"extensionsIgnored",r.extensionsIgnored}}},
        {"settings",{{"firstInput",c.first+1},{"lastInput",c.last+1},{"radius",c.radius},{"endTick",c.endTick},{"localEndTick",c.localEndTick},{"localEndPolicy",c.localEndTick?"explicit":"next-same-button-edge"},{"actualEndTick",actualEnd},{"frameOffset",c.frameOffset},{"repeats",c.repeats},{"baselineRepeats",c.baselineRepeats},{"mode",c.mode==ScanMode::Edge?"edge":"hold-pair"}}}};
    out["rows"]=json::array();
    for(auto const&row:rows){
        auto const& in=r.inputs[row.index];
        json j={{"input",row.index+1},{"frame",in.frame+c.frameOffset},{"timeSecondsAt240TPS",(in.frame+c.frameOffset)/240.0},
            {"button",in.button},{"player",in.player2?2:1},{"edge",in.down?"press":"release"},{"gameMode",row.gameMode},{"complete",row.done},
            {"scannedOffsetRange",{row.lower,row.upper}},{"passingTicks",countPasses(row)},
            {"leftSearchLimit",row.leftSearchLimit},{"rightSearchLimit",row.rightSearchLimit},
            {"leftSequenceOrEndpointLimit",row.leftSequenceLimit},{"rightSequenceOrEndpointLimit",row.rightSequenceLimit}};
        j["localEndTick"]=row.localEnd?json(row.localEnd):json(nullptr);
        j["localWindowAtOriginalInput"]=row.localTargetInterval?json(row.localTargetInterval->last-row.localTargetInterval->first+1):json(nullptr);
        j["localLeftSearchLimit"]=row.localLeftSearchLimit;j["localRightSearchLimit"]=row.localRightSearchLimit;
        j["localRightEndpointLimit"]=row.localUpper<row.upper;
        j["localIntervals"]=json::array();j["laterFailureOffsets"]=json::array();
        for(auto const&i:row.localIntervals)j["localIntervals"].push_back({{"earlyOffset",i.first},{"lateOffset",i.last},{"firstTick",in.frame+c.frameOffset+i.first},{"lastTick",in.frame+c.frameOffset+i.last},{"count",i.last-i.first+1}});
        for(auto const&p:row.probes)if(p.localVerdict==Verdict::Pass&&p.verdict==Verdict::Fail)j["laterFailureOffsets"].push_back(p.offset);
        j["pairedInput"]=row.pair?json(*row.pair+1):json(nullptr);
        j["windowAtOriginalInput"]=row.targetInterval?json(row.targetInterval->last-row.targetInterval->first+1):json(nullptr);
        j["removalResult"]=row.removal?json(verdictName(*row.removal)):json(nullptr);
        j["intervals"]=json::array();j["probes"]=json::array();
        for(auto const&i:row.intervals)j["intervals"].push_back({{"earlyOffset",i.first},{"lateOffset",i.last},{"firstTick",in.frame+c.frameOffset+i.first},{"lastTick",in.frame+c.frameOffset+i.last},{"count",i.last-i.first+1}});
        for(auto const&p:row.probes)j["probes"].push_back({{"offset",p.offset},{"result",verdictName(p.verdict)},{"stoppedAt",p.stoppedAt},{"localResult",verdictName(p.localVerdict)}});
        out["rows"].push_back(std::move(j));
    }
    return out.dump(2);
}
static std::string quote(std::string s){std::string o="\"";for(char ch:s){o+=ch;if(ch=='"')o+='"';}return o+'"';}
std::string reportCSV(Replay const&r,Config const&c,std::vector<Row> const&rows){
    std::ostringstream s;s<<"input,frame,time_seconds,player,button,edge,mode,paired_input,window_at_original,total_passing_ticks,offset_intervals,complete,left_search_limit,right_search_limit,left_sequence_limit,right_sequence_limit,removal,local_end_tick,local_window,local_left_open,local_right_open\n";
    s<<std::setprecision(12);
    for(auto const&row:rows){auto const&i=r.inputs[row.index];auto frame=i.frame+c.frameOffset;
        s<<row.index+1<<','<<frame<<','<<frame/240.0<<','<<(i.player2?2:1)<<','<<i.button<<','<<(i.down?"press":"release")<<','<<quote(row.gameMode)<<',';
        if(row.pair)s<<*row.pair+1;
        s<<',';
        if(row.targetInterval)s<<row.targetInterval->last-row.targetInterval->first+1;
        s<<','<<countPasses(row)<<','<<quote(intervalText(row))<<','<<row.done<<','<<row.leftSearchLimit<<','<<row.rightSearchLimit<<','<<row.leftSequenceLimit<<','<<row.rightSequenceLimit<<',';
        if(row.removal)s<<verdictName(*row.removal);
        s<<','<<row.localEnd<<',';
        if(row.localTargetInterval)s<<row.localTargetInterval->last-row.localTargetInterval->first+1;
        s<<','<<row.localLeftSearchLimit<<','<<row.localRightSearchLimit<<'\n';
    }return s.str();
}
}
