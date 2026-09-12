#pragma once
#include <nlohmann/json.hpp>
#include <deque>
#include <cstddef>

namespace fwl {
// Bounded, single-run diagnostics. All calls happen on the game's main thread.
// Truncation is explicit, so a long scan never silently claims a full trace.
class Diagnostics {
public:
    using JSON=nlohmann::json;
    struct Limits { std::size_t trials=20000, frames=16000, baseline=5000, events=2000, tail=8, edge=8; };
    Diagnostics();
    explicit Diagnostics(Limits limits);
    void reset(JSON context);
    void event(char const* kind,JSON data=JSON::object());
    void beginTrial(JSON context);
    void frame(JSON state,bool nearEdge,bool baseline);
    void localBoundary(JSON state);
    void finishTrial(JSON result);
    JSON bundle(JSON analysis) const;
    bool empty()const{return m_context.empty();}
private:
    Limits m_limits;
    JSON m_context=JSON::object(),m_trial=JSON::object();
    JSON m_trials=JSON::array(),m_events=JSON::array(),m_baseline=JSON::array(),m_edge=JSON::array();
    std::deque<JSON> m_tail;
    std::size_t m_frames=0,m_droppedFrames=0,m_droppedBaseline=0,m_droppedEvents=0,m_droppedTrials=0,m_sequence=0;
};
}
