#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fwl {
using Tick = std::int64_t;
constexpr Tick maxTick = 10'000'000;
constexpr std::size_t maxInputs = 100'000;
struct Input {
    Tick frame = 0;
    int button = 1;
    bool player2 = false;
    bool down = false;
};
struct Replay {
    std::string source, format, author, bot, levelName;
    std::uint32_t levelID = 0;
    std::uint64_t seed = 0;
    double tps = 240, duration = 0;
    bool platformer = false, ldm = false, extensionsIgnored = false;
    std::vector<Input> inputs;
    std::vector<Tick> deaths;
};
struct Interval { Tick first = 0, last = 0; };
enum class Verdict { Pass, Fail, Unstable, Desync, Timeout, NotMeasured };
inline char const* verdictName(Verdict v) {
    switch(v) {
        case Verdict::Pass: return "pass";
        case Verdict::Fail: return "fail";
        case Verdict::Unstable: return "unstable";
        case Verdict::Desync: return "desync";
        case Verdict::Timeout: return "timeout";
        case Verdict::NotMeasured: return "not_measured";
    }
    return "unknown";
}
enum class ScanMode { Edge, HoldPair };
struct Config {
    std::size_t first = 0, last = 0;
    Tick radius = 12, endTick = 0;
    Tick localEndTick = 0; // 0: next edge of the same player/button; otherwise explicit boundary
    Tick frameOffset = 0;
    int repeats = 2, baselineRepeats = 3;
    int ticksPerRender = 64;
    bool includePress = true, includeRelease = true;
    int playerFilter = 0; // 0 both, 1 P1, 2 P2
    ScanMode mode = ScanMode::Edge;
};
struct Probe {
    Tick offset = 0;
    Verdict verdict = Verdict::Fail;
    Tick stoppedAt = 0;
    Verdict localVerdict = Verdict::NotMeasured;
};
struct Row {
    std::size_t index = 0;
    std::optional<std::size_t> pair;
    Tick lower = 0, upper = 0;
    bool leftSequenceLimit = false, rightSequenceLimit = false;
    bool leftSearchLimit = false, rightSearchLimit = false;
    bool done = false;
    std::string gameMode;
    std::vector<Probe> probes;
    std::vector<Interval> intervals; // offsets, inclusive, never bridge holes
    std::optional<Interval> targetInterval;
    std::optional<Verdict> removal;
    Tick localEnd = 0, localUpper = 0;
    std::vector<Interval> localIntervals;
    std::optional<Interval> localTargetInterval;
    bool localLeftSearchLimit = false, localRightSearchLimit = false;
};
Replay parseReplay(std::vector<std::uint8_t> const& bytes, std::string source = {});
Replay readReplay(std::filesystem::path const& path);
void validateReplay(Replay& replay);
void validateConfig(Replay const&, Config const&);
std::vector<Row> planRows(Replay const&, Config const&);
std::vector<Input> makeTrial(Replay const&, Config const&, Row const*, Tick offset, bool remove = false);
void constrainEndpoint(Replay const&, Config const&, std::vector<Row>&, Tick endpoint);
void configureLocalEndpoints(Replay const&, Config const&, std::vector<Row>&, Tick goalEnd);
Verdict classifyLocal(Verdict goalResult, Tick stoppedAt, Tick localEnd, Tick affectedTail);
void summarize(Row&);
Tick countPasses(Row const&);
std::string intervalText(Row const&);
std::string reportJSON(Replay const&, Config const&, std::vector<Row> const&, std::string const& status,
                       std::string const& levelFingerprint, Tick actualEnd);
std::string reportCSV(Replay const&, Config const&, std::vector<Row> const&);
std::uint64_t hashBytes(void const*, std::size_t, std::uint64_t hash = 14695981039346656037ull);
std::string hexHash(std::uint64_t);
}
