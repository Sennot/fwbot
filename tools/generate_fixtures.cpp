// Run against upstream GDReplayFormat gdr2 commit
// 2a0ca3cff9b1d16863037a464da5fe27c28a5823. Generates test data, not gameplay macros.
#include <gdr/gdr.hpp>
#include <iostream>
struct ExtendedInput:gdr::Input<"fixture.physics"> {
    using gdr::Input<"fixture.physics">::Input;
    void saveExtension(binary_writer&w)const override { w << 123.5f << std::uint64_t(900000); }
};
struct ExtendedReplay:gdr::Replay<ExtendedReplay,ExtendedInput> {
    ExtendedReplay():Replay("FixtureBot",1){}
    void saveExtension(binary_writer&w)const override { w << std::string("ignored extension"); }
};
int main(int argc,char**argv){
    if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);
    gdr::Replay<> a;a.author="fixture author";a.botInfo={"FixtureBot",1};a.levelInfo=gdr::Level("Fixture level",12345);
    a.duration=2.5f;a.framerate=240;a.gameVersion=22081;a.seed=123;
    a.inputs={{100,1,false,true},{105,1,true,true},{110,1,false,false},{120,1,true,false}};
    auto r=a.exportData(std::filesystem::path(argv[1])/"dual.gdr2");if(r.isErr()){std::cerr<<r.unwrapErr();return 1;}
    ExtendedReplay b;b.author="extensions";b.duration=3.75f;b.framerate=240;b.platformer=true;
    b.inputs={{0,2,false,true},{32,1,false,true},{40,1,true,true},{64,2,false,false},{65,1,false,false},{70,1,true,false}};
    auto s=b.exportData(std::filesystem::path(argv[1])/"platformer-extensions.gdr2");if(s.isErr()){std::cerr<<s.unwrapErr();return 1;}
}
