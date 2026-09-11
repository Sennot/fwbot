#include "Engine.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/cocos.hpp>
#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <charconv>
#include <functional>
#include <sstream>

using namespace geode::prelude;
namespace fwl {
namespace {
void alert(std::string const& s){FLAlertLayer::create("Frame Window Lab",s,"OK")->show();}
std::optional<std::filesystem::path> pickReplay(){
    std::array<wchar_t,32768> name{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);
    ofn.hwndOwner=GetActiveWindow();ofn.lpstrFile=name.data();ofn.nMaxFile=static_cast<DWORD>(name.size());
    ofn.lpstrFilter=L"Geometry Dash replays (*.gdr;*.gdr2)\0*.gdr;*.gdr2\0All files\0*.*\0\0";
    ofn.lpstrTitle=L"Import replay into Frame Window Lab";
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
    if(GetOpenFileNameW(&ofn))return std::filesystem::path(name.data());
    if(CommDlgExtendedError())throw std::runtime_error("Windows file picker failed.");
    return std::nullopt;
}
Tick integer(TextInput* input,char const* name){
    std::string s=input->getString();Tick n=0;auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),n);
    if(error!=std::errc{}||end!=s.data()+s.size())throw std::runtime_error(std::string("Enter an integer for ")+name+".");return n;
}
}
class LabPopup:public Popup {
    Ref<PauseLayer> pause;
    Config draft;
    CCNode* body=nullptr;
    CCMenu* menu=nullptr;
    CCLabelBMFont* statusLabel=nullptr;
    TextInput *first=nullptr,*last=nullptr,*radius=nullptr,*endpoint=nullptr,*frameOffset=nullptr,*previewOffset=nullptr;
    int view=0,page=0;
    std::size_t selected=0;
    CCLabelBMFont* text(std::string const&s,float x,float y,float scale=.4f,float width=460){
        auto l=CCLabelBMFont::create(s.c_str(),"bigFont.fnt");l->setPosition({x,y});l->limitLabelWidth(width,scale,.15f);body->addChild(l);return l;
    }
    void button(std::string const&s,float x,float y,std::function<void()> fn,float width=95){
        auto spr=ButtonSprite::create(s.c_str());spr->setScale(std::min(.5f,width/spr->getContentWidth()));
        auto b=CCMenuItemExt::createSpriteExtra(spr,[fn=std::move(fn)](CCMenuItemSpriteExtra* item){Ref<CCMenuItemSpriteExtra> keep(item);auto call=fn;try{call();}catch(std::exception const&e){alert(e.what());}});
        b->setPosition({x,y});menu->addChild(b);
    }
    TextInput* field(char const*name,Tick value,float x,float y,float width=100){
        auto in=TextInput::create(width,name);in->setScale(.75f);in->setPosition({x,y});in->setLabel(name);
        in->setCommonFilter(CommonFilter::Int);in->setMaxCharCount(9);in->setString(std::to_string(value));body->addChild(in);return in;
    }
    void clear(){
        if(body)body->removeFromParentAndCleanup(true);
        body=CCNode::create();body->setContentSize({510,300});body->setPosition({0,0});m_mainLayer->addChild(body);
        menu=CCMenu::create();menu->setPosition({0,0});body->addChild(menu,20);statusLabel=nullptr;
        first=last=radius=endpoint=frameOffset=previewOffset=nullptr;
    }
    void readDraft(){
        if(!first)return;
        auto f=integer(first,"first input"),l=integer(last,"last input");
        if(f<1||l<1)throw std::runtime_error("Input numbers start at 1.");
        draft.first=static_cast<std::size_t>(f-1);draft.last=static_cast<std::size_t>(l-1);
        draft.radius=integer(radius,"radius");draft.endTick=integer(endpoint,"end tick");draft.frameOffset=integer(frameOffset,"frame offset");
    }
    void resumeGame(){
        auto p=pause;onClose(nullptr);if(p)p->onResume(nullptr);
    }
    void home(){
        view=0;clear();auto&e=Engine::get();
        button("Import GDR/GDR2",100,253,[this]{
            if(Engine::get().protectedRun)throw std::runtime_error("Stop analysis before importing.");
            if(auto path=pickReplay()){Engine::get().load(*path);draft=Engine::get().config;home();}
        },155);
        button("Help",295,253,[this]{help();},70);
        button("Files",420,253,[]{file::openFolder(Mod::get()->getSaveDir());},75);
        statusLabel=text(e.progress(),255,223,.32f,475);
        first=field("First input",draft.first+1,53,184,95);
        last=field("Last input",draft.last+1,153,184,95);
        radius=field("Radius +/-",draft.radius,253,184,95);
        endpoint=field("End tick (0=finish)",draft.endTick,353,184,108);
        frameOffset=field("Frame offset",draft.frameOffset,453,184,95);
        for(auto*f:{first,last,radius,endpoint,frameOffset})f->setEnabled(!e.protectedRun);
        auto edge=draft.includePress?(draft.includeRelease?"Press + release":"Press only"):"Release only";
        button(edge,90,140,[this]{
            if(Engine::get().protectedRun)return;readDraft();
            if(draft.includePress&&draft.includeRelease)draft.includeRelease=false;
            else if(draft.includePress){draft.includePress=false;draft.includeRelease=true;}
            else draft.includePress=draft.includeRelease=true;home();
        },140);
        button(draft.playerFilter==0?"Both players":draft.playerFilter==1?"Player 1":"Player 2",260,140,[this]{
            if(Engine::get().protectedRun)return;readDraft();draft.playerFilter=(draft.playerFilter+1)%3;home();
        },130);
        button(draft.mode==ScanMode::Edge?"Single edge":"Hold pair",425,140,[this]{
            if(Engine::get().protectedRun)return;readDraft();draft.mode=draft.mode==ScanMode::Edge?ScanMode::HoldPair:ScanMode::Edge;
            if(draft.mode==ScanMode::HoldPair)draft.includePress=true;home();
        },120);
        text("All modes: cube / ship / ball / UFO / wave / robot / spider / swing",255,108,.30f,475);
        text("240 TPS | full-start replay | fixed other inputs | no position corrections",255,89,.28f,475);
        button("Options",75,58,[this]{if(Engine::get().protectedRun)return;readDraft();options();},100);
        button("Results",195,58,[this]{if(!Engine::get().protectedRun)readDraft();results();},100);
        button(e.active?"Resume":"Start scan",330,58,[this]{
            auto&e=Engine::get();if(e.active){resumeGame();return;}readDraft();e.allowLevelMismatch=Mod::get()->getSavedValue<bool>("allow-copy-id",false);
            e.start(PlayLayer::get(),draft);resumeGame();
        },130);
        button("Stop",450,58,[this]{Engine::get().stop();home();},75);
        text("Esc pauses a running scan. Completed rows are saved automatically.",255,25,.28f,475);
    }
    void options(){
        view=1;clear();text("Analysis options",255,250,.55f);
        auto reps=field("Repeats per offset",draft.repeats,130,205,155);
        auto checks=field("Baseline repeats",draft.baselineRepeats,365,205,155);
        auto speed=field("Steps per render (max 1024)",draft.ticksPerRender,255,145,220);
        text("Higher speed runs more simulation steps between rendered frames.",255,105,.30f,465);
        text("A 10 ms budget keeps the interface responsive. Physics step stays fixed.",255,86,.28f,465);
        bool copy=Mod::get()->getSavedValue<bool>("allow-copy-id",false);
        button(copy?"Allow copy ID: ON":"Allow copy ID: OFF",255,58,[this,reps,checks,speed,copy]{
            draft.repeats=static_cast<int>(integer(reps,"repeats"));draft.baselineRepeats=static_cast<int>(integer(checks,"baseline repeats"));
            draft.ticksPerRender=static_cast<int>(integer(speed,"speed"));Mod::get()->setSavedValue("allow-copy-id",!copy);options();
        },200);
        button("Apply",430,25,[this,reps,checks,speed]{
            auto r=integer(reps,"repeats"),b=integer(checks,"baseline repeats"),s=integer(speed,"speed");
            if(r<1||r>10||b<2||b>10||s<1||s>1024)throw std::runtime_error("Repeats: 1..10; baseline: 2..10; speed: 1..1024.");
            draft.repeats=int(r);draft.baselineRepeats=int(b);draft.ticksPerRender=int(s);home();
        },80);
        button("Back",75,25,[this]{home();},80);
    }
    void results(){
        view=2;clear();auto&e=Engine::get();
        int pages=std::max(1,int((e.rows.size()+6)/7));page=std::clamp(page,0,pages-1);
        text(fmt::format("Results | page {}/{} | click a row",page+1,pages),255,250,.40f);
        text("Input       Tick      Edge / player        Window       Status",255,225,.30f);
        for(int n=0;n<7;++n){std::size_t k=static_cast<std::size_t>(page*7+n);if(k>=e.rows.size())break;
            auto const&r=e.rows[k];auto const&i=e.replay.inputs[r.index];
            auto count=r.targetInterval?std::to_string(r.targetInterval->last-r.targetInterval->first+1):"-";
            auto state=r.done?(r.leftSearchLimit||r.rightSearchLimit?"open bound":"done"):"partial";
            for(auto const&p:r.probes)if(p.verdict==Verdict::Unstable||p.verdict==Verdict::Timeout)state="uncertain";
            std::string s=fmt::format("#{}   {}   {} P{}   {} ticks   {}",r.index+1,i.frame+e.config.frameOffset,i.down?"press":"release",i.player2?2:1,count,state);
            button(s,255,198-n*22,[this,k]{selected=k;detail();},455);
        }
        if(e.rows.empty())text("No results yet. Start an analysis first.",255,150,.4f);
        button("<",60,28,[this]{--page;results();},40);
        button("Back",155,28,[this]{home();},90);
        button("Export",300,28,[]{Engine::get().saveReport();file::openFolder(Mod::get()->getSaveDir());},100);
        button(">",450,28,[this]{++page;results();},40);
    }
    void detail(){
        view=3;clear();auto&e=Engine::get();if(selected>=e.rows.size()){results();return;}
        auto const&r=e.rows[selected];auto const&i=e.replay.inputs[r.index];
        text(fmt::format("Input #{} | {} | P{} | {}",r.index+1,i.down?"press":"release",i.player2?2:1,r.gameMode),255,249,.42f);
        text(fmt::format("Original tick {} | scanned offsets {}..{}",i.frame+e.config.frameOffset,r.lower,r.upper),255,221,.34f);
        auto ints=intervalText(r);if(ints.size()>160)ints=ints.substr(0,157)+"...";
        text("Passing offsets: "+ints,255,195,.32f,470);
        auto w=r.targetInterval?std::to_string(r.targetInterval->last-r.targetInterval->first+1):"none";
        text(fmt::format("Window containing original input: {} | all passing ticks: {}",w,countPasses(r)),255,171,.31f,470);
        auto removal=r.removal?verdictName(*r.removal):"not tested";
        text(std::string("Removing this ")+(r.pair?"hold pair":"edge")+": "+removal,255,147,.32f,470);
        std::string bounds="Bounds: ";
        bounds+=r.leftSearchLimit?"left not found":r.leftSequenceLimit?"left sequence/start limit":r.done?"left scan complete":"left incomplete";
        bounds+=" | ";bounds+=r.rightSearchLimit?"right not found":r.rightSequenceLimit?"right sequence/end limit":r.done?"right scan complete":"right incomplete";
        text(bounds,255,123,.28f,470);
        previewOffset=field("Preview offset",0,95,77,130);
        button("Early",225,77,[this]{auto const&r=Engine::get().rows[selected];previewOffset->setString(std::to_string(r.targetInterval?r.targetInterval->first:0));},75);
        button("Late",330,77,[this]{auto const&r=Engine::get().rows[selected];previewOffset->setString(std::to_string(r.targetInterval?r.targetInterval->last:0));},75);
        button("Play",440,77,[this]{
            auto&e=Engine::get();auto off=integer(previewOffset,"preview offset");e.preview(PlayLayer::get(),selected,off);resumeGame();
        },75);
        button("Back",85,28,[this]{results();},90);
        button("Copy details",270,28,[this]{
            auto&e=Engine::get();auto const&r=e.rows[selected];
            std::ostringstream s;s<<"Input #"<<r.index+1<<"\nOffsets: "<<intervalText(r)<<"\n";
            for(auto const&p:r.probes)s<<p.offset<<": "<<verdictName(p.verdict)<<" @ "<<p.stoppedAt<<"\n";
            clipboard::write(s.str());
        },140);
        button("Export",430,28,[]{Engine::get().saveReport();file::openFolder(Mod::get()->getSaveDir());},90);
    }
    void help(){
        view=4;clear();
        std::vector<std::string> lines={
            "1. Open the matching level in normal mode, without StartPos.",
            "2. Import .gdr (MessagePack/JSON) or binary .gdr2.",
            "3. Select input numbers, scan radius and endpoint.",
            "4. End tick 0 means actual level completion, not the last input.",
            "5. Start scan. Esc pauses; Resume continues the same job.",
            "6. Results show separate passing intervals and open bounds.",
            "Single edge moves one press/release; Hold pair moves both.",
            "Other inputs remain fixed. Windows depend on the chosen macro.",
            "CBF, external bots/corrections/speedhack must be disabled.",
            "A baseline failure means no valid measurement. Try frame offset.",
            "A pass after removing an edge only applies to this tested section.",
            "Full details and Russian instructions are included in README_RU.md."
        };
        for(std::size_t k=0;k<lines.size();++k)text(lines[k],255,250-float(k)*17,.29f,475);
        button("Back",255,25,[this]{home();},100);
    }
    bool init(Ref<PauseLayer> const&p){
        if(!Popup::init(510,300,"GJ_square02.png"))return false;
        pause=p;draft=Engine::get().config;setTitle("Frame Window Lab", "goldFont.fnt",.65f,17);
        auto win=CCDirector::get()->getWinSize();m_mainLayer->setScale(std::min({1.f,(win.width-14)/510.f,(win.height-12)/300.f}));
        home();scheduleUpdate();return true;
    }
    void update(float)override{if(statusLabel){statusLabel->setString(Engine::get().progress().c_str());statusLabel->limitLabelWidth(475,.32f,.15f);}}
public:
    static LabPopup* create(Ref<PauseLayer>const&p){auto pop=new LabPopup;if(pop->init(p)){pop->autorelease();return pop;}delete pop;return nullptr;}
};
void showLab(Ref<PauseLayer>const&p){if(auto pop=LabPopup::create(p))pop->show();}
}
