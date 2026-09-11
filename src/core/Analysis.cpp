#include "Model.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace fwl {
void validateReplay(Replay& r) {
    if (r.inputs.empty() || r.inputs.size() > maxInputs) throw std::runtime_error("Replay must contain 1..100000 inputs.");
    if (!std::isfinite(r.tps) || r.tps < 1 || r.tps > 10000) throw std::runtime_error("Invalid replay TPS.");
    if (!std::isfinite(r.duration) || r.duration < 0) throw std::runtime_error("Invalid replay duration.");
    std::stable_sort(r.inputs.begin(), r.inputs.end(), [](auto const& a, auto const& b){ return a.frame < b.frame; });
    for (auto const& i : r.inputs) {
        if (i.frame < 0 || i.frame > maxTick) throw std::runtime_error("Input frame is outside supported range.");
        if (i.button < 1 || i.button > 3) throw std::runtime_error("Input button must be 1, 2 or 3.");
    }
}
void validateConfig(Replay const& r, Config const& c) {
    if (std::abs(r.tps - 240.0) > 1e-7) throw std::runtime_error("This engine measures 240 TPS. Re-record at 240 TPS; changing metadata is not a conversion.");
    if (!r.deaths.empty()) throw std::runtime_error("Replay contains planned deaths. Use a continuous successful replay.");
    if (c.first > c.last || c.last >= r.inputs.size()) throw std::runtime_error("Invalid input range (use the 1-based numbers shown in the UI).");
    if (c.radius < 1 || c.radius > 10000) throw std::runtime_error("Scan radius must be 1..10000 ticks.");
    if (c.endTick < 0 || c.endTick > maxTick || c.frameOffset < -120 || c.frameOffset > 120) throw std::runtime_error("Invalid end tick or frame offset.");
    if (c.repeats < 1 || c.repeats > 10 || c.baselineRepeats < 2 || c.baselineRepeats > 10) throw std::runtime_error("Invalid repeat count.");
    if (!c.includePress && !c.includeRelease) throw std::runtime_error("Select presses and/or releases.");
    if (c.playerFilter < 0 || c.playerFilter > 2) throw std::runtime_error("Invalid player filter.");
    if (c.ticksPerRender < 1 || c.ticksPerRender > 1024) throw std::runtime_error("Invalid scan speed.");
    std::array<bool, 6> held{};
    std::array<Tick, 6> last; last.fill(-1);
    for (auto const& i : r.inputs) {
        auto frame = i.frame + c.frameOffset;
        if (frame < 0 || frame > maxTick) throw std::runtime_error("Frame offset moves an input outside the supported tick range.");
        auto channel = (i.player2 ? 3 : 0) + i.button - 1;
        if (held[channel] == i.down) throw std::runtime_error("Repeated press/release on one button: normalize the source macro first (no events are silently dropped).");
        if (last[channel] == frame) throw std::runtime_error("Press and release on the same tick/button are ambiguous. Use a replay with distinct edge ticks.");
        held[channel] = i.down; last[channel] = frame;
    }
}
std::vector<Row> planRows(Replay const& r, Config const& c) {
    validateConfig(r,c);
    std::vector<Row> rows;
    // O(n) channel links, including sparse P2 and platformer buttons.
    const auto none=r.inputs.size();
    std::vector<std::size_t> previous(none,none),next(none,none);
    std::array<std::size_t,6> seen;seen.fill(none);
    auto channel=[](Input const& i){return (i.player2?3:0)+i.button-1;};
    for(std::size_t k=0;k<none;++k){auto ch=channel(r.inputs[k]);previous[k]=seen[ch];seen[ch]=k;}
    seen.fill(none);
    for(auto k=none;k>0;){--k;auto ch=channel(r.inputs[k]);next[k]=seen[ch];seen[ch]=k;}
    for (auto k = c.first; k <= c.last; ++k) {
        auto const& in = r.inputs[k];
        if ((!c.includePress && in.down) || (!c.includeRelease && !in.down)) continue;
        if (c.playerFilter && c.playerFilter != (in.player2 ? 2 : 1)) continue;
        if (c.mode == ScanMode::HoldPair && !in.down) continue;
        Row row; row.index = k;
        if (c.mode == ScanMode::HoldPair) {
            if(next[k]==none)continue;
            row.pair=next[k];
        }
        Tick minOffset = -(in.frame + c.frameOffset);
        Tick maxOffset = maxTick - (r.inputs[row.pair.value_or(k)].frame + c.frameOffset);
        if(previous[k]!=none)minOffset=std::max(minOffset,r.inputs[previous[k]].frame-in.frame+1);
        auto tail = row.pair.value_or(k);
        if(next[tail]!=none)maxOffset=std::min(maxOffset,r.inputs[next[tail]].frame-r.inputs[tail].frame-1);
        if (c.endTick) {
            if (r.inputs[tail].frame + c.frameOffset >= c.endTick) continue;
            maxOffset = std::min(maxOffset,c.endTick-1-r.inputs[tail].frame-c.frameOffset);
        }
        row.lower = std::max(-c.radius,minOffset); row.upper = std::min(c.radius,maxOffset);
        row.leftSequenceLimit = minOffset >= -c.radius; row.rightSequenceLimit = maxOffset <= c.radius;
        if(row.lower <= 0 && row.upper >= 0) rows.push_back(std::move(row));
    }
    if(rows.empty()) throw std::runtime_error("No inputs match this range, filter and endpoint.");
    return rows;
}
std::vector<Input> makeTrial(Replay const& r, Config const& c, Row const* row, Tick offset, bool remove) {
    auto result = r.inputs;
    for (std::size_t j=0;j<result.size();++j) {
        result[j].frame += c.frameOffset;
        if(row && (j==row->index || (row->pair && j==*row->pair))) result[j].frame += offset;
    }
    if(remove && row) {
        if(row->pair) result.erase(result.begin()+static_cast<std::ptrdiff_t>(*row->pair));
        result.erase(result.begin()+static_cast<std::ptrdiff_t>(row->index));
    }
    std::stable_sort(result.begin(),result.end(),[](auto const&a,auto const&b){return a.frame<b.frame;});
    return result;
}
void constrainEndpoint(Replay const& r, Config const& c, std::vector<Row>& rows, Tick endpoint) {
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](Row const& row){
        return r.inputs[row.pair.value_or(row.index)].frame+c.frameOffset>=endpoint;
    }),rows.end());
    for(auto& row:rows){
        Tick latest=endpoint-1-r.inputs[row.pair.value_or(row.index)].frame-c.frameOffset;
        if(latest<=row.upper){row.upper=latest;row.rightSequenceLimit=true;}
    }
}
void summarize(Row& r) {
    std::sort(r.probes.begin(),r.probes.end(),[](auto const&a,auto const&b){return a.offset<b.offset;});
    r.intervals.clear(); r.targetInterval.reset();
    for(auto const& p:r.probes) if(p.verdict==Verdict::Pass) {
        if(r.intervals.empty() || p.offset != r.intervals.back().last+1) r.intervals.push_back({p.offset,p.offset});
        else r.intervals.back().last=p.offset;
    }
    for(auto const& i:r.intervals) if(i.first<=0 && i.last>=0) r.targetInterval=i;
    r.leftSearchLimit = !r.intervals.empty() && r.intervals.front().first==r.lower && !r.leftSequenceLimit;
    r.rightSearchLimit = !r.intervals.empty() && r.intervals.back().last==r.upper && !r.rightSequenceLimit;
}
Tick countPasses(Row const& r) { Tick n=0; for(auto const&i:r.intervals)n+=i.last-i.first+1;return n; }
std::string intervalText(Row const& r) {
    std::ostringstream out; bool first=true;
    for(auto const&i:r.intervals){ if(!first)out<<"; "; first=false; out<<i.first; if(i.first!=i.last)out<<".."<<i.last; }
    return first?"none":out.str();
}
std::uint64_t hashBytes(void const* ptr,std::size_t n,std::uint64_t h) {
    auto p=static_cast<unsigned char const*>(ptr); for(std::size_t i=0;i<n;++i){ h^=p[i];h*=1099511628211ull; } return h;
}
std::string hexHash(std::uint64_t h) { std::ostringstream s;s<<std::hex<<std::setfill('0')<<std::setw(16)<<h;return s.str(); }
}
