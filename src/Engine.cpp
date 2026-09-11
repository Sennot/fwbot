#include "Engine.hpp"
#include <Geode/binding/GameToolbox.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/utils/file.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>

using namespace geode::prelude;
namespace fwl {
Engine& Engine::get(){static Engine e;return e;}
static std::string modeOf(PlayerObject* p){
    if(!p)return "none";
    std::string m=p->m_isShip?"ship":p->m_isDart?"wave":p->m_isBird?"ufo":p->m_isBall?"ball":p->m_isRobot?"robot":p->m_isSpider?"spider":p->m_isSwing?"swing":"cube";
    if(p->m_vehicleSize<0.8f)m="mini "+m;
    if(p->m_isUpsideDown)m+=" inverted";
    return m;
}
void Engine::load(std::filesystem::path const&p){
    if(active||protectedRun)throw std::runtime_error("Stop the current analysis before importing.");
    auto parsed=readReplay(p);replay=std::move(parsed);loaded=true;rows.clear();baseline.clear();actualEnd=0;
    reportPath.clear();fingerprint.clear();analysisStatus.clear();phase=Phase::Baseline;
    config.first=0;config.last=replay.inputs.size()-1;
    status=fmt::format("{} inputs | {} | {} TPS",replay.inputs.size(),replay.format,replay.tps);
    if(replay.extensionsIgnored)status+=" | corrections/extensions ignored";
    ++revision;
}
void Engine::checkEnvironment(PlayLayer*pl){
    if(!pl||!pl->m_player1||!pl->m_level)throw std::runtime_error("Open the level, pause, then open Frame Window Lab.");
    if(!loaded)throw std::runtime_error("Import a replay first.");
    if(pl->m_startPosObject)throw std::runtime_error("Use a full-start copy without StartPos. All trials replay from the beginning.");
    if(pl->m_isPracticeMode)throw std::runtime_error("Switch to normal mode before starting. Practice checkpoints are not used.");
    if(replay.levelID && replay.levelID!=static_cast<std::uint32_t>(int(pl->m_level->m_levelID)) && !allowLevelMismatch)
        throw std::runtime_error("Macro level ID differs. If this is the same level's copy, enable Allow copy ID in Options.");
    if(replay.ldm!=pl->m_lowDetailMode)throw std::runtime_error("Set the level Low Detail Mode to match the replay before starting.");
    if(replay.platformer!=pl->m_levelSettings->m_platformerMode)throw std::runtime_error("Macro and level disagree about platformer mode.");
    if(std::abs(CCScheduler::get()->getTimeScale()-1.f)>0.0001f)throw std::runtime_error("Disable external speedhack before analysis.");
    for(auto*mod:Loader::get()->getAllMods()){
        std::string id=mod->getID();
        if(!Loader::get()->getLoadedMod(id)||id==std::string(Mod::get()->getID()))continue;
        if(id.find("click_between_frames")!=std::string::npos||id.find("xdbot")!=std::string::npos||
           id.find("silifork")!=std::string::npos||id.find("silicate")!=std::string::npos||
           id.find("megahack")!=std::string::npos||id.find("eclipse")!=std::string::npos)
            throw std::runtime_error("Disable conflicting mod in Geode and restart: "+id+". Import its macro here instead.");
    }
}
void Engine::captureEnvironment(PlayLayer*pl){
    layer=pl;originalTest=pl->m_isTestMode;originalCBS=pl->m_clickBetweenSteps;originalCOS=pl->m_clickOnSteps;
    originalSeed=GameToolbox::getfast_srand();originalDontSave=pl->m_level->m_dontSave;
    levelCounters={int(pl->m_level->m_attempts),int(pl->m_level->m_jumps),int(pl->m_level->m_clicks),int(pl->m_level->m_attemptTime)};
    protectedRun=true;pl->m_isTestMode=true;pl->m_level->m_dontSave=true;
}
void Engine::start(PlayLayer*pl,Config const&c){
    if(active||protectedRun)throw std::runtime_error("Analysis already running.");
    checkEnvironment(pl);auto planned=planRows(replay,c);
    auto const&levelText=pl->m_level->m_levelString;
    fingerprint=hexHash(hashBytes(levelText.data(),levelText.size()));
    config=c;rows=std::move(planned);baseline.clear();actualEnd=0;currentRow=0;completedTrials=0;
    baselineRuns=0;repeat=0;repeatedResult.reset();phase=Phase::Baseline;
    captureEnvironment(pl);active=true;finishing=false;pendingReset=true;trialDone=false;
    started=std::chrono::steady_clock::now();status="Checking baseline replay...";analysisStatus=status;restartAfterPause=false;
    auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    reportPath=Mod::get()->getSaveDir()/fmt::format("analysis-{}.json",stamp);++revision;
}
void Engine::preview(PlayLayer*pl,std::size_t row,Tick off){
    if(active||protectedRun)throw std::runtime_error("Stop analysis before preview.");
    checkEnvironment(pl);
    if(row>=rows.size())throw std::runtime_error("Select a measured row first.");
    if(off<rows[row].lower||off>rows[row].upper)throw std::runtime_error("Preview offset is outside this row's allowed range.");
    auto const&levelText=pl->m_level->m_levelString;
    if(fingerprint!=hexHash(hashBytes(levelText.data(),levelText.size())))
        throw std::runtime_error("Preview requires the exact level used for this report.");
    if(phase!=Phase::Preview)analysisStatus=status;
    previewRow=row;previewOffset=off;phase=Phase::Preview;restartAfterPause=false;
    captureEnvironment(pl);active=true;finishing=false;pendingReset=true;trialDone=false;previewAccumulator=0;
    status="Preview running at normal speed. Esc pauses.";++revision;
}
void Engine::stop(std::string reason){if(!protectedRun)return;status=std::move(reason);active=false;finishing=true;++revision;}
void Engine::restoreEnvironment(bool reset){
    auto*pl=layer;if(!pl)return;
    if(reset){
        resetting=true;
        try{pl->resetLevelFromStart();}catch(...){resetting=false;throw;}
        resetting=false;
    }
    pl->m_queuedButtons.clear();pl->m_queuedRecordedButtons.clear();pl->m_queuedReplayButtons.clear();
    injecting=true;
    for(int b=1;b<=3;++b){pl->handleButton(false,b,true);pl->handleButton(false,b,false);}
    injecting=false;
    pl->m_isTestMode=originalTest;pl->m_clickBetweenSteps=originalCBS;pl->m_clickOnSteps=originalCOS;
    if(pl->m_level){
        pl->m_level->m_dontSave=originalDontSave;
        pl->m_level->m_attempts=levelCounters[0];pl->m_level->m_jumps=levelCounters[1];
        pl->m_level->m_clicks=levelCounters[2];pl->m_level->m_attemptTime=levelCounters[3];
    }
    GameToolbox::fast_srand(originalSeed);protectedRun=false;
}
void Engine::detach(PlayLayer*pl){
    if(layer!=pl)return;
    if(protectedRun){status="Level closed; partial results saved.";active=false;saveReport();restoreEnvironment(false);}
    layer=nullptr;active=false;protectedRun=false;finishing=false;driving=false;
}
void Engine::saveReport(){
    if(reportPath.empty())return;
    try{
        auto text=reportJSON(replay,config,rows,phase==Phase::Preview?analysisStatus:status,fingerprint,actualEnd);
        auto result=file::writeStringSafe(reportPath,text);
        if(!result)throw std::runtime_error(result.unwrapErr());
        auto csv=reportPath;csv.replace_extension(".csv");
        auto csvResult=file::writeStringSafe(csv,reportCSV(replay,config,rows));
        if(!csvResult)throw std::runtime_error(csvResult.unwrapErr());
    }catch(std::exception const&e){log::error("Frame Window Lab export: {}",e.what());status+=" | Export failed: ";status+=e.what();}
}
void Engine::finishJob(){
    finishing=false;active=false;saveReport();auto*pl=layer;restoreEnvironment(true);
    if(pl&&!pl->m_isPaused)pl->pauseGame(false);
    ++revision;
}
void Engine::prepareTrial(){
    auto*pl=layer;pendingReset=false;trialDone=false;tick=0;cursor=0;modeCursor=0;idleUpdates=0;
    bool changed=phase==Phase::Scan||phase==Phase::Removal||phase==Phase::Preview;
    auto*row=changed?&rows[phase==Phase::Preview?previewRow:currentRow]:nullptr;
    auto delta=phase==Phase::Preview?previewOffset:offset;
    trial=makeTrial(replay,config,row,changed?delta:0,phase==Phase::Removal);
    firstChanged=row?replay.inputs[row->index].frame+config.frameOffset+std::min<Tick>(0,delta):maxTick;
    if(phase==Phase::Removal)firstChanged=replay.inputs[row->index].frame+config.frameOffset;
    auto last=replay.inputs.back().frame+config.frameOffset;
    timeoutTick=config.endTick?config.endTick:std::min<Tick>(maxTick,std::max<Tick>(last+240*60,static_cast<Tick>(std::min(replay.duration,36000.0)*240)+240*60));
    resetting=true;GameToolbox::fast_srand(replay.seed);pl->resetLevelFromStart();
    pl->m_isTestMode=true;pl->m_level->m_dontSave=true;pl->m_clickBetweenSteps=false;pl->m_clickOnSteps=true;
    pl->m_resumeTimer=0;pl->m_queuedButtons.clear();pl->m_queuedRecordedButtons.clear();pl->m_queuedReplayButtons.clear();
    // Remove held physical input carried through a reset, then restore trial seed.
    injecting=true;for(int b=1;b<=3;++b){pl->handleButton(false,b,true);pl->handleButton(false,b,false);}injecting=false;
    GameToolbox::fast_srand(replay.seed);resetting=false;
}
std::uint64_t Engine::snapshot()const{
    std::uint64_t h=14695981039346656037ull;
    auto add=[&](auto const&value){h=hashBytes(&value,sizeof(value),h);};
    for(auto*p:{layer->m_player1,layer->m_player2}){
        bool exists=p && (p==layer->m_player1 || layer->m_gameState.m_isDualMode);
        add(exists);if(!exists)continue;
        auto pos=p->getPosition();add(pos.x);add(pos.y);add(p->m_yVelocity);add(p->m_playerSpeed);
        add(p->m_isShip);add(p->m_isDart);add(p->m_isBall);add(p->m_isBird);add(p->m_isRobot);add(p->m_isSpider);add(p->m_isSwing);
        add(p->m_vehicleSize);add(p->m_isUpsideDown);add(p->m_isSideways);add(p->m_isGoingLeft);
        add(p->m_isOnGround);add(p->m_isOnSlope);add(p->m_isDashing);add(p->m_jumpBuffered);
        for(int b=1;b<=3;++b){auto it=p->m_holdingButtons.find(b);bool held=it!=p->m_holdingButtons.end()&&it->second;add(held);}
    }
    add(layer->m_gameState.m_isDualMode);add(layer->m_gameState.m_timeWarp);
    add(layer->m_gameState.m_levelTime);add(layer->m_gameState.m_currentProgress);
    auto seed=GameToolbox::getfast_srand();add(seed);return h;
}
void Engine::finishTrial(Verdict result,Tick at){if(trialDone)return;trialDone=true;trialResult=result;trialStopped=at;}
bool Engine::beforeCommands(GJBaseGameLayer*base,bool half){
    if(!active||resetting||base!=layer)return true;
    if(trialDone)return false;
    // Half steps belong to the same 240 TPS command tick; do not double-count.
    if(half)return true;
    if(layer->m_resumeTimer>0)return true;
    idleUpdates=0;
    auto state=snapshot();
    if(phase==Phase::Baseline&&baselineRuns==0)baseline.push_back(state);
    else if(phase!=Phase::Preview){
        bool compare=phase==Phase::Baseline||phase==Phase::RowCheck||tick<firstChanged;
        if(compare&&(std::size_t(tick)>=baseline.size()||baseline[static_cast<std::size_t>(tick)]!=state)){
            finishTrial(Verdict::Desync,tick);return false;
        }
    }
    if(config.endTick&&tick>=config.endTick){finishTrial(Verdict::Pass,tick);return false;}
    if(tick>=timeoutTick){finishTrial(Verdict::Timeout,tick);return false;}
    if(phase==Phase::Baseline&&baselineRuns==0){
        while(modeCursor<rows.size()&&replay.inputs[rows[modeCursor].index].frame+config.frameOffset<=tick){
            auto&row=rows[modeCursor++];
            row.gameMode=modeOf(replay.inputs[row.index].player2?layer->m_player2:layer->m_player1);
            if(layer->m_gameState.m_isDualMode)row.gameMode+=" / dual";
        }
    }
    layer->m_queuedButtons.clear(); // discard live keyboard/mouse commands
    injecting=true;
    while(cursor<trial.size()&&trial[cursor].frame<=tick){
        auto const&i=trial[cursor++];
        // Current GD binding uses isPlayer1, while GDR stores player2.
        layer->handleButton(i.down,i.button,!i.player2);
    }
    injecting=false;++tick;return true;
}
void Engine::died(PlayLayer*pl){if(active&&pl==layer&&!resetting)finishTrial(Verdict::Fail,tick);}
void Engine::completed(PlayLayer*pl){
    if(!active||pl!=layer||resetting)return;
    // A shifted edge that never ran is not a measured passing input.
    if(phase==Phase::Scan||phase==Phase::Preview){
        auto const&row=rows[phase==Phase::Preview?previewRow:currentRow];
        auto delta=phase==Phase::Preview?previewOffset:offset;
        if(replay.inputs[row.pair.value_or(row.index)].frame+config.frameOffset+delta>=tick){
            finishTrial(Verdict::Unstable,tick);return;
        }
    }
    finishTrial(Verdict::Pass,tick);
}
void Engine::unexpectedReset(PlayLayer*pl){if(active&&pl==layer&&!resetting)stop("External restart interrupted analysis; results saved.");}
void Engine::consumeTrial(){
    ++completedTrials;
    if(phase==Phase::Preview){stop(fmt::format("Preview: {} at tick {} (offset {:+}).",verdictName(trialResult),trialStopped,previewOffset));return;}
    if(trialResult==Verdict::Desync){stop(fmt::format("DESYNC at tick {}. Check macro offset, seed and other mods; partial results saved.",trialStopped));return;}
    if(phase==Phase::Baseline||phase==Phase::RowCheck){
        if(trialResult!=Verdict::Pass){stop(fmt::format("Baseline {} at tick {}. No valid window measurement. Check offset/physics/level.",verdictName(trialResult),trialStopped));return;}
        if(baselineRuns==0)actualEnd=trialStopped;
        else if(actualEnd!=trialStopped){stop("Baseline endpoint changed between attempts.");return;}
        if(phase==Phase::Baseline&&++baselineRuns<config.baselineRepeats){pendingReset=true;return;}
        if(phase==Phase::Baseline){
            // Never scan events that the successful baseline never reached.
            constrainEndpoint(replay,config,rows,actualEnd);
            if(rows.empty()){stop("No selected inputs occur before the baseline endpoint.");return;}
        }
        phase=Phase::Scan;offset=rows[currentRow].lower;repeat=0;repeatedResult.reset();pendingReset=true;return;
    }
    if(!repeatedResult){repeatedResult=trialResult;repeatedStop=trialStopped;}
    else if(*repeatedResult!=trialResult||repeatedStop!=trialStopped)repeatedResult=Verdict::Unstable;
    if(++repeat<config.repeats){pendingReset=true;return;}
    auto result=*repeatedResult;repeat=0;repeatedResult.reset();auto&row=rows[currentRow];
    if(phase==Phase::Scan){
        row.probes.push_back({offset,result,trialStopped});summarize(row);++revision;
        if(offset==0&&result!=Verdict::Pass){stop("Zero-offset replay stopped matching the baseline. Analysis stopped.");return;}
        if(++offset<=row.upper){pendingReset=true;return;}
        phase=Phase::Removal;offset=0;pendingReset=true;return;
    }
    row.removal=result;row.done=true;summarize(row);saveReport();++revision;
    if(++currentRow>=rows.size()){stop("Analysis complete. JSON and CSV saved.");return;}
    phase=Phase::RowCheck;pendingReset=true;
}
void Engine::runScheduler(float realDt,std::function<void(float)>const&original){
    if(driving){original(realDt);return;}
    if(finishing){finishJob();original(realDt);return;}
    if(!active||!layer){original(realDt);return;}
    if(layer->m_isPaused){restartAfterPause=true;original(realDt);return;}
    if(restartAfterPause){
        restartAfterPause=false;pendingReset=true;
        if(phase==Phase::Baseline&&baselineRuns==0)baseline.clear();
    }
    driving=true;
    auto began=std::chrono::steady_clock::now();
    int limit=config.ticksPerRender;
    if(phase==Phase::Preview){previewAccumulator+=std::clamp(double(realDt),0.0,0.1)*240;limit=std::min(24,int(previewAccumulator));previewAccumulator-=limit;}
    for(int i=0;i<limit&&active&&layer&&!layer->m_isPaused;++i){
        if(pendingReset)prepareTrial();
        if(!active)break;
        original(1.f/240.f);
        if(trialDone)consumeTrial();
        if(++idleUpdates>2400){stop("No physics commands received. Check pause state and conflicting mods.");break;}
        if(std::chrono::steady_clock::now()-began>std::chrono::milliseconds(10))break;
    }
    driving=false;
    if(finishing)finishJob();
}
std::string Engine::progress()const{
    if(!active)return status;
    if(phase==Phase::Preview)return fmt::format("Preview | tick {} | Esc to pause",tick);
    if(phase==Phase::Baseline)return fmt::format("Baseline {}/{} | tick {} | Esc to pause",baselineRuns+1,config.baselineRepeats,tick);
    auto label=phase==Phase::RowCheck?"baseline check":phase==Phase::Removal?"removal test":"scan";
    return fmt::format("Row {}/{} | {} | offset {:+} | repeat {}/{} | tick {}",currentRow+1,rows.size(),label,offset,repeat+1,config.repeats,tick);
}
}
