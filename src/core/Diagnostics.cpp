#include "Diagnostics.hpp"
#include <utility>
namespace fwl {
Diagnostics::Diagnostics():Diagnostics(Limits{}){}
Diagnostics::Diagnostics(Limits limits):m_limits(limits){}
void Diagnostics::reset(JSON context){
    m_context=std::move(context);m_trial=JSON::object();
    m_trials=JSON::array();m_events=JSON::array();m_baseline=JSON::array();m_edge=JSON::array();m_tail.clear();
    m_frames=m_droppedFrames=m_droppedBaseline=m_droppedEvents=m_droppedTrials=m_sequence=0;
}
void Diagnostics::event(char const*kind,JSON data){
    auto entry=JSON{{"sequence",++m_sequence},{"kind",kind},{"data",std::move(data)}};
    if(m_events.size()<m_limits.events)m_events.push_back(std::move(entry));else ++m_droppedEvents;
}
void Diagnostics::beginTrial(JSON context){
    if(!m_trial.empty())finishTrial({{"result","interrupted_before_completion"}});
    m_trial=std::move(context);m_tail.clear();m_edge=JSON::array();
}
void Diagnostics::frame(JSON state,bool nearEdge,bool firstBaseline){
    if(firstBaseline){if(m_baseline.size()<m_limits.baseline)m_baseline.push_back(state);else ++m_droppedBaseline;}
    if(nearEdge&&m_edge.size()<m_limits.edge)m_edge.push_back(state);
    if(m_limits.tail){if(m_tail.size()==m_limits.tail)m_tail.pop_front();m_tail.push_back(std::move(state));}
}
void Diagnostics::localBoundary(JSON state){
    if(!m_trial.empty())m_trial["localBoundaryState"]=std::move(state);
}
void Diagnostics::finishTrial(JSON result){
    if(m_trial.empty())return;
    m_trial["outcome"]=std::move(result);
    m_trial["nearChangedInput"]=JSON::array();m_trial["lastStates"]=JSON::array();
    auto save=[&](JSON& dest,JSON const& state){if(m_frames<m_limits.frames){dest.push_back(state);++m_frames;}else ++m_droppedFrames;};
    if(m_trials.size()<m_limits.trials){
        for(auto const&s:m_edge)save(m_trial["nearChangedInput"],s);
        for(auto const&s:m_tail)save(m_trial["lastStates"],s);
        m_trials.push_back(std::move(m_trial));
    }else ++m_droppedTrials;
    m_trial=JSON::object();m_tail.clear();m_edge=JSON::array();
}
Diagnostics::JSON Diagnostics::bundle(JSON analysis)const{
    JSON running=m_trial;
    if(!running.empty()){
        running["nearChangedInput"]=m_edge;running["lastStates"]=JSON::array();
        for(auto const&s:m_tail)running["lastStates"].push_back(s);
    }
    return {{"schema","frame-window-lab/debug/1"},{"context",m_context},{"analysis",std::move(analysis)},
        {"events",m_events},{"baselineStates",m_baseline},{"trials",m_trials},{"runningTrial",running.empty()?JSON(nullptr):running},
        {"traceSemantics","States are captured before command injection unless positionPhase says death callback. Native death at boundary N is during tick N-1."},
        {"limits",{{"trialSummaries",m_limits.trials},{"trialStates",m_limits.frames},{"baselineStates",m_limits.baseline},{"events",m_limits.events}}},
        {"dropped",{{"trialSummaries",m_droppedTrials},{"trialStates",m_droppedFrames},{"baselineStates",m_droppedBaseline},{"events",m_droppedEvents}}}};
}
}
