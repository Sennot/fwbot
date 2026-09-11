#include "Model.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace fwl {
namespace {
using json=nlohmann::json;
// Independent, bounds-checked reader for the actual GDR2 writer's wire format.
// Strings are length-prefixed; floats are big endian IEEE754 (the upstream
// README's cstring/varint description does not match its binary writer).
class Reader {
    std::vector<std::uint8_t> const& b;
    std::size_t pos=0;
public:
    explicit Reader(std::vector<std::uint8_t> const& bytes):b(bytes){}
    std::size_t remaining()const{return b.size()-pos;}
    std::uint8_t byte(){if(!remaining())throw std::runtime_error("Truncated GDR2.");return b[pos++];}
    std::uint64_t var(){
        std::uint64_t v=0;
        for(unsigned n=0;n<10;++n){auto c=byte();if(n==9 && (c&0xfe))throw std::runtime_error("GDR2 varint overflow.");v|=std::uint64_t(c&127)<<(7*n);if(!(c&128))return v;}
        throw std::runtime_error("Invalid GDR2 varint.");
    }
    void skip(std::uint64_t n){if(n>remaining())throw std::runtime_error("Truncated GDR2 extension.");pos+=static_cast<std::size_t>(n);}
    void block(){skip(var());}
    std::string str(){auto n=var();if(n>remaining()||n>1024*1024)throw std::runtime_error("Invalid GDR2 string length.");std::string s(reinterpret_cast<char const*>(b.data()+pos),static_cast<std::size_t>(n));skip(n);return s;}
    bool boolean(){auto v=var();if(v>1)throw std::runtime_error("Invalid GDR2 boolean.");return v!=0;}
    template<class T> T floating(){
        std::array<std::uint8_t,sizeof(T)> a{};for(auto&v:a)v=byte();
        if constexpr(std::endian::native==std::endian::little)std::reverse(a.begin(),a.end());
        T out;std::memcpy(&out,a.data(),sizeof(T));return out;
    }
};
Tick checkedTick(std::uint64_t n){if(n>std::uint64_t(maxTick))throw std::runtime_error("Frame exceeds supported limit.");return static_cast<Tick>(n);}
Tick jsonTick(json const& j){
    if(!j.is_number_integer())throw std::runtime_error("Input frame must be an integer.");
    if(j.is_number_unsigned())return checkedTick(j.get<std::uint64_t>());
    auto x=j.get<Tick>();if(x<0||x>maxTick)throw std::runtime_error("Invalid input frame.");return x;
}
Replay parseV2(std::vector<std::uint8_t> const& bytes){
    Reader rd(bytes);rd.skip(3);
    if(rd.var()!=2)throw std::runtime_error("Unsupported GDR binary version (expected 2).");
    Replay r;r.format="gdr2";
    auto tag=rd.str();r.author=rd.str();rd.str();r.duration=rd.floating<float>();
    rd.var();r.tps=rd.floating<double>();r.seed=rd.var();rd.var();
    r.ldm=rd.boolean();r.platformer=rd.boolean();r.bot=rd.str();rd.var();
    auto level=rd.var();if(level>std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("Invalid level ID.");
    r.levelID=static_cast<std::uint32_t>(level);r.levelName=rd.str();
    auto ext=rd.var();r.extensionsIgnored=ext!=0||!tag.empty();rd.skip(ext);
    auto deaths=rd.var();if(deaths>maxInputs||deaths>rd.remaining())throw std::runtime_error("Invalid death count.");
    Tick previous=0;
    for(std::uint64_t i=0;i<deaths;++i){auto delta=checkedTick(rd.var());if(delta>maxTick-previous)throw std::runtime_error("Death frame overflow.");previous+=delta;r.deaths.push_back(previous);}
    auto count=rd.var(), p1=rd.var();
    if(count>maxInputs||p1>count||count>rd.remaining())throw std::runtime_error("Invalid GDR2 input counts.");
    r.inputs.reserve(static_cast<std::size_t>(count));previous=0;
    for(std::uint64_t i=0;i<count;++i){
        if(i==p1)previous=0;
        auto packed=rd.var();auto delta=checkedTick(packed>>(r.platformer?3:1));
        if(delta>maxTick-previous)throw std::runtime_error("Input delta overflow.");
        previous+=delta;
        r.inputs.push_back({previous,r.platformer?int((packed>>1)&3):1,i>=p1,(packed&1)!=0});
        if(!tag.empty())rd.block();
    }
    if(rd.remaining())throw std::runtime_error("Trailing bytes or incorrect input count in GDR2.");
    return r;
}
Replay parseV1(std::vector<std::uint8_t> const& bytes){
    json j;
    try{j=json::from_msgpack(bytes);}catch(json::exception const&){j=json::parse(bytes);}
    if(!j.is_object()||!j.contains("inputs")||!j["inputs"].is_array())throw std::runtime_error("Not a GDR replay: missing inputs array.");
    if(j["inputs"].size()>maxInputs)throw std::runtime_error("Too many inputs.");
    Replay r;r.format="gdr1";r.author=j.value("author",std::string{});
    r.duration=j.value("duration",0.0);r.tps=j.value("framerate",240.0);
    if(j.contains("seed")){
        if(!j["seed"].is_number_integer())throw std::runtime_error("Invalid seed.");
        r.seed=j["seed"].is_number_unsigned()?j["seed"].get<std::uint64_t>():static_cast<std::uint64_t>(j["seed"].get<std::int64_t>());
    }
    r.platformer=j.value("platformer",false);r.ldm=j.value("ldm",false);
    if(j.contains("bot")&&j["bot"].is_object())r.bot=j["bot"].value("name",std::string{});
    if(j.contains("level")&&j["level"].is_object()){
        auto id=j["level"].value("id",std::int64_t(0));
        if(id<0||std::uint64_t(id)>std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("Invalid level ID.");
        r.levelID=static_cast<std::uint32_t>(id);r.levelName=j["level"].value("name",std::string{});
    }
    r.extensionsIgnored=j.contains("frameFixes")||j.contains("frame_fixes");
    for(auto const&i:j["inputs"]){
        if(!i.is_object())throw std::runtime_error("Invalid input object.");
        Input in;in.frame=jsonTick(i.at("frame"));
        auto const& btn=i.contains("btn")?i.at("btn"):i.at("button");
        if(!btn.is_number_integer())throw std::runtime_error("Invalid input button.");
        if(btn<1 || btn>3)throw std::runtime_error("Input button must be 1, 2 or 3.");
        in.button=btn.get<int>();
        in.player2=i.contains("2p")?i.at("2p").get<bool>():i.value("player2",false);
        in.down=i.at("down").get<bool>();
        r.extensionsIgnored |= i.size()>4;
        if(in.button!=1)r.platformer=true;
        r.inputs.push_back(in);
    }
    if(j.contains("deaths")){for(auto const& d:j.at("deaths"))r.deaths.push_back(jsonTick(d));}
    return r;
}
}
Replay parseReplay(std::vector<std::uint8_t> const& bytes,std::string source){
    if(bytes.empty()||bytes.size()>32*1024*1024)throw std::runtime_error("Replay must be nonempty and at most 32 MiB.");
    auto r=bytes.size()>=3&&bytes[0]=='G'&&bytes[1]=='D'&&bytes[2]=='R'?parseV2(bytes):parseV1(bytes);
    r.source=std::move(source);validateReplay(r);return r;
}
Replay readReplay(std::filesystem::path const& p){
    std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open replay file.");
    auto n=f.tellg();if(n<=0||n>32*1024*1024)throw std::runtime_error("Replay must be nonempty and at most 32 MiB.");
    std::vector<std::uint8_t> b(static_cast<std::size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);
    if(!f)throw std::runtime_error("Cannot read replay file.");
    auto name=p.filename().u8string();return parseReplay(b,std::string(name.begin(),name.end()));
}
}
