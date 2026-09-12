#include "Model.hpp"
#include "Diagnostics.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace fwl;
using nlohmann::json;
static int checks=0;
static void check(bool value,char const*what){++checks;if(!value)throw std::runtime_error(what);}
template<class F>static void rejects(F fn,char const*what){bool thrown=false;try{fn();}catch(std::exception const&){thrown=true;}check(thrown,what);}
static std::vector<std::uint8_t> bytes(std::string const&s){return {s.begin(),s.end()};}
static Replay simple(){Replay r;r.inputs={{100,1,false,true},{110,1,false,false},{130,1,false,true},{145,1,false,false}};return r;}
static Config settings(Replay const&r){Config c;c.last=r.inputs.size()-1;return c;}
static void parser(){
    json j={{"framerate",240},{"duration",2.5},{"seed",123},{"level",{{"id",12345},{"name","Fixture level"}}},{"inputs",json::array({
        {{"frame",100},{"btn",1},{"2p",false},{"down",true}},
        {{"frame",110},{"btn",1},{"2p",false},{"down",false}}
    })}};
    auto a=parseReplay(bytes(j.dump()));auto b=parseReplay(json::to_msgpack(j));
    check(a.inputs.size()==2&&b.inputs.size()==2,"GDR1 JSON and MessagePack");
    check(b.inputs[0].frame==100&&b.inputs[0].down&&!b.inputs[1].down,"GDR1 edges");
    check(b.tps==240&&b.seed==123&&b.duration==2.5&&b.levelID==12345,"GDR1 metadata");
    auto invalid=j;invalid["inputs"][0]["frame"]=-1;rejects([&]{parseReplay(bytes(invalid.dump()));},"Reject negative frame");
    invalid=j;invalid["inputs"][0]["frame"]=1.25;rejects([&]{parseReplay(bytes(invalid.dump()));},"Reject fractional frame instead of rounding");
    invalid=j;invalid["inputs"][0]["btn"]=0;rejects([&]{parseReplay(bytes(invalid.dump()));},"Reject invalid button");
    rejects([]{parseReplay({});},"Reject empty data");
    rejects([]{parseReplay(bytes("GDR\x03"));},"Reject unsupported GDR version");
    rejects([]{parseReplay(bytes("GDR\x02"));},"Reject truncated GDR header");
    rejects([]{parseReplay(bytes("{}"));},"Reject missing inputs");
    auto dual=readReplay(std::filesystem::path(FWL_FIXTURE_DIR)/"dual.gdr2");
    check(dual.format=="gdr2"&&dual.inputs.size()==4,"Official writer GDR2 fixture");
    check(dual.tps==240&&dual.duration==2.5&&dual.seed==123&&dual.levelID==12345,"GDR2 IEEE754 and metadata");
    check(dual.inputs[1].frame==105&&dual.inputs[1].player2&&dual.inputs[2].frame==110&&!dual.inputs[2].player2,"P2 delta reset and stable interleave");
    auto ext=readReplay(std::filesystem::path(FWL_FIXTURE_DIR)/"platformer-extensions.gdr2");
    check(ext.platformer&&ext.extensionsIgnored&&ext.inputs.size()==6,"Skip unfamiliar GDR2 extensions");
    check(ext.inputs.front().frame==0&&ext.inputs.front().button==2&&ext.inputs.back().frame==70,"Platformer packed buttons");
    std::ifstream f(std::filesystem::path(FWL_FIXTURE_DIR)/"dual.gdr2",std::ios::binary);
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)),{});
    for(std::size_t n=0;n<data.size();++n){auto cut=data;cut.resize(n);rejects([&]{parseReplay(cut);},"Every truncated fixture rejected");}
    auto extra=data;extra.push_back(0);rejects([&]{parseReplay(extra);},"Reject trailing input bytes");
    std::vector<std::uint8_t> over={'G','D','R'};over.insert(over.end(),11,255);rejects([&]{parseReplay(over);},"Reject overflowing varint");
    // Finite malformed-input corpus catches non-consuming binary readers/hangs.
    std::mt19937 random(42);
    for(int n=0;n<300;++n){auto fuzz=data;fuzz[3+(random()%(fuzz.size()-3))]=std::uint8_t(random());try{auto parsed=parseReplay(fuzz);validateReplay(parsed);}catch(std::exception const&){}++checks;}
}
static void windows(){
    auto r=simple();auto c=settings(r);auto rows=planRows(r,c);
    check(rows.size()==4,"All edges planned");check(rows[0].lower==-12&&rows[0].upper==9,"Press bounded by paired release");
    check(rows[1].lower==-9&&rows[1].upper==12,"Release bounded by neighboring press");
    c.mode=ScanMode::HoldPair;rows=planRows(r,c);
    check(rows.size()==2&&rows[0].pair==1,"Hold pairs found");
    auto trial=makeTrial(r,c,&rows[0],3);check(trial[0].frame==103&&trial[1].frame==113&&trial[2].frame==130,"Pair preserves hold duration and other inputs");
    trial=makeTrial(r,c,&rows[0],0,true);check(trial.size()==2&&trial[0].frame==130,"Pair removal removes exactly two events");
    c.mode=ScanMode::Edge;c.includeRelease=false;rows=planRows(r,c);check(rows.size()==2&&rows[1].index==2,"Press filter");
    c=settings(r);c.endTick=120;rows=planRows(r,c);check(rows.size()==2&&rows[1].upper==9,"Endpoint cannot produce untested later events");
    c=settings(r);rows=planRows(r,c);constrainEndpoint(r,c,rows,120);
    check(rows.size()==2&&rows[1].upper==9&&rows[1].rightSequenceLimit,"Discovered completion bounds shifted inputs");
    c.mode=ScanMode::HoldPair;rows=planRows(r,c);constrainEndpoint(r,c,rows,105);
    check(rows.empty(),"Pair tail must execute before completion");
    c=settings(r);c.frameOffset=-101;rejects([&]{planRows(r,c);},"Reject inputs before zero");
    c=settings(r);c.first=2;c.last=1;rejects([&]{planRows(r,c);},"Reject reversed input range");
    c=settings(r);c.repeats=0;rejects([&]{planRows(r,c);},"Reject zero repeated checks");
    c=settings(r);r.tps=360;rejects([&]{planRows(r,c);},"Reject non240 TPS without silent conversion");
    r=simple();r.deaths={55};rejects([&]{planRows(r,c);},"Reject macro with planned deaths");
    r=simple();r.inputs[1].frame=100;rejects([&]{planRows(r,c);},"Reject ambiguous same-tick edges");
    r=simple();r.inputs[1].down=true;rejects([&]{planRows(r,c);},"Reject repeated press without silently dropping it");
    r=simple();r.inputs.insert(r.inputs.begin()+1,{100,1,true,true});c=settings(r);validateReplay(r);
    check(planRows(r,c).size()==5,"Same-tick input on separate players is preserved");
    c.playerFilter=2;check(planRows(r,c).size()==1,"P2 filter");
    Row row;row.lower=-3;row.upper=3;row.probes={{-3,Verdict::Pass,0},{-2,Verdict::Pass,0},{-1,Verdict::Fail,0},{0,Verdict::Pass,0},{1,Verdict::Pass,0},{2,Verdict::Unstable,0},{3,Verdict::Pass,0}};
    summarize(row);check(row.intervals.size()==3&&countPasses(row)==5,"Disjoint and unstable windows are not bridged");
    check(row.targetInterval&&row.targetInterval->first==0&&row.targetInterval->last==1,"Original interval not sum of holes");
    check(row.leftSearchLimit&&row.rightSearchLimit,"Unbounded search is explicit");
    row.leftSequenceLimit=true;summarize(row);check(!row.leftSearchLimit,"Sequence boundary distinguished from search limit");
    auto jsonText=reportJSON(simple(),settings(simple()),{row},"partial","test",200);
    auto doc=json::parse(jsonText);check(doc["rows"][0]["windowAtOriginalInput"]==2&&doc["rows"][0]["passingTicks"]==5,"Export does not overstate window");
    check(doc["status"]=="partial"&&!doc["rows"][0]["complete"].get<bool>(),"Incomplete report marked");
}
static void exhaustiveOrder(){
    // Property: every generated trial retains strict alternation and ordering on
    // each physical channel, regardless of interleaving with the other player.
    Replay r;r.inputs={{4,1,false,true},{5,1,true,true},{9,1,false,false},{12,1,true,false},{20,1,false,true},{25,1,false,false}};
    auto c=settings(r);c.radius=30;
    for(auto mode:{ScanMode::Edge,ScanMode::HoldPair}){
        c.mode=mode;auto rows=planRows(r,c);
        for(auto const&row:rows)for(Tick off=row.lower;off<=row.upper;++off){
            auto trial=makeTrial(r,c,&row,off);std::array<Tick,6> last;last.fill(-1);std::array<bool,6> held{};
            for(auto const&i:trial){auto ch=(i.player2?3:0)+i.button-1;check(i.frame>last[ch]&&i.frame>=0,"Generated trial preserves per-channel order");check(i.down!=held[ch],"Generated trial preserves alternating state");last[ch]=i.frame;held[ch]=i.down;}
        }
    }
}
static void localWindows(){
    std::ifstream f(std::filesystem::path(FWL_FIXTURE_DIR)/"local-goal-regression.json");
    json fixture;f>>fixture;
    Row row;row.lower=-12;row.upper=12;row.localUpper=12;row.localEnd=fixture["localEnd"].get<Tick>();
    auto frame=fixture["inputFrame"].get<Tick>();
    for(auto const&p:fixture["probes"]){
        auto v=p["result"]=="pass"?Verdict::Pass:Verdict::Fail;
        auto off=p["offset"].get<Tick>(),stop=p["stoppedAt"].get<Tick>();
        row.probes.push_back({off,v,stop,classifyLocal(v,stop,row.localEnd,frame+off)});
    }
    summarize(row);
    check(row.targetInterval&&row.targetInterval->first==0&&row.targetInterval->last==0,"Observed goal window remains one tick");
    check(row.localTargetInterval&&row.localTargetInterval->first==-1&&row.localTargetInterval->last==1,"Observed local window at explicit boundary 324 is three ticks");
    check(classifyLocal(Verdict::Fail,324,324,280)==Verdict::Fail,"Collision on last simulated tick fails local boundary");
    check(classifyLocal(Verdict::Fail,325,324,280)==Verdict::Pass,"Later collision does not invalidate earlier local survival");
    check(classifyLocal(Verdict::Pass,324,324,280)==Verdict::Pass,"Exact reached boundary passes");
    check(classifyLocal(Verdict::Pass,300,324,280)==Verdict::NotMeasured,"Completion before local boundary is not local success");
    check(classifyLocal(Verdict::Pass,960,324,324)==Verdict::NotMeasured,"Unexecuted affected edge not measured locally");
    check(classifyLocal(Verdict::Desync,960,324,280)==Verdict::Desync,"Desync never converted to local pass");
    check(classifyLocal(Verdict::Timeout,324,324,280)==Verdict::Pass,"Timeout after reaching local boundary retains local measurement");
    auto r=simple();auto c=settings(r);auto rows=planRows(r,c);configureLocalEndpoints(r,c,rows,200);
    check(rows[0].localEnd==110&&rows[1].localEnd==130&&rows.back().localEnd==200,"Auto local boundary uses next same-channel edge or goal");
    c.localEndTick=120;configureLocalEndpoints(r,c,rows,200);
    check(rows[1].localEnd==120&&rows[1].localUpper==9&&rows[2].localEnd==0,"Explicit local boundary labels later rows not applicable");
    c=settings(r);c.mode=ScanMode::HoldPair;rows=planRows(r,c);configureLocalEndpoints(r,c,rows,200);
    check(rows[0].localEnd==130,"Pair local goal lies after paired release");
    row.probes[12].localVerdict=Verdict::Unstable;summarize(row);
    check(!row.localTargetInterval,"Unstable local zero never bridged");
    auto doc=json::parse(reportJSON(simple(),settings(simple()),{row},"complete","test",960));
    check(doc["rows"][0].contains("localIntervals")&&doc["rows"][0]["localEndTick"]==324,"Both observation horizons exported");
}
static void diagnostics(){
    Diagnostics d({2,3,2,2,2,2});d.reset({{"test",true}});
    d.event("one");d.event("two");d.event("three");
    d.beginTrial({{"trial",1}});
    for(int i=0;i<5;++i)d.frame({{"tick",i}},true,true);
    d.localBoundary({{"tick",3}});
    d.finishTrial({{"result","fail"}});
    d.beginTrial({{"trial",2}});d.frame({{"tick",0}},true,false);
    auto running=d.bundle({{"status","running"}});
    check(running["runningTrial"]["trial"]==2,"Current trial included in manual debug export");
    d.beginTrial({{"trial",3}}); // automatic interrupted record for trial 2
    d.finishTrial({{"result","pass"}});
    auto out=d.bundle({{"status","complete"}});
    check(out["trials"].size()==2&&out["baselineStates"].size()==2,"Diagnostic arrays stay bounded");
    check(out["dropped"]["trialSummaries"]==1&&out["dropped"]["baselineStates"]==3&&out["dropped"]["events"]==1,"Diagnostic truncation explicit");
    check(out["dropped"]["trialStates"].get<int>()>0,"Trace budget exhaustion recorded");
    check(out["trials"][1]["outcome"]["result"]=="interrupted_before_completion","Pause/restart does not erase interrupted trial");
    check(out["trials"][0]["localBoundaryState"]["tick"]==3,"Observed local boundary state retained independently of tail trace");
    check(out["analysis"]["status"]=="complete"&&out["context"]["test"]==true,"One-file bundle embeds report and reproduction context");
    d.reset({{"newRun",true}});out=d.bundle(json::object());
    check(out["trials"].empty()&&out["dropped"]["events"]==0,"New run clears diagnostics counters");
}
int main(){try{parser();windows();exhaustiveOrder();localWindows();diagnostics();std::cout<<"PASS: "<<checks<<" checks\n";return 0;}catch(std::exception const&e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
