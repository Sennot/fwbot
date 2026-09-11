#include "Engine.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

class $modify(FWLScheduler, CCScheduler) {
    void update(float dt) {
        auto&e=fwl::Engine::get();
        try { e.runScheduler(dt,[this](float step){CCScheduler::update(step);}); }
        catch(std::exception const&err){e.driving=false;e.injecting=false;e.resetting=false;e.stop(std::string("Analysis error: ")+err.what());}
    }
};
class $modify(FWLBase, GJBaseGameLayer) {
    void processCommands(float dt,bool isHalfTick,bool isLastTick) {
        auto&e=fwl::Engine::get();
        if(e.beforeCommands(this,isHalfTick))GJBaseGameLayer::processCommands(dt,isHalfTick,isLastTick);
    }
    void handleButton(bool down,int button,bool isPlayer1) {
        auto&e=fwl::Engine::get();
        if(e.protectedRun &&
           static_cast<GJBaseGameLayer*>(this)==static_cast<GJBaseGameLayer*>(e.layer) &&
           !e.injecting && !e.resetting)return;
        GJBaseGameLayer::handleButton(down,button,isPlayer1);
    }
};
class $modify(FWLPlay, PlayLayer) {
    void destroyPlayer(PlayerObject*player,GameObject*object) {
        auto&e=fwl::Engine::get();
        if(e.protectedRun&&e.layer==this){
            if(object!=m_anticheatSpike)e.died(this);
            return; // quarantine the failed trial; no death animation or retry timer
        }
        PlayLayer::destroyPlayer(player,object);
    }
    void levelComplete() {
        auto&e=fwl::Engine::get();
        if(e.protectedRun&&e.layer==this){e.completed(this);return;}
        PlayLayer::levelComplete();
    }
    void resetLevel(){fwl::Engine::get().unexpectedReset(this);PlayLayer::resetLevel();}
    void onQuit(){fwl::Engine::get().detach(this);PlayLayer::onQuit();}
    void onExit(){fwl::Engine::get().detach(this);PlayLayer::onExit();}
};
class $modify(FWLStats, GameStatsManager) {
    void incrementStat(char const*key,int amount){
        if(fwl::Engine::get().protectedRun)return;
        GameStatsManager::incrementStat(key,amount);
    }
};
class $modify(FWLLevel,GJGameLevel) {
    void savePercentage(int percent,bool practice,int clicks,int attempts,bool valid){
        auto&e=fwl::Engine::get();
        if(e.protectedRun&&e.layer&&e.layer->m_level==this)return;
        GJGameLevel::savePercentage(percent,practice,clicks,attempts,valid);
    }
};
class $modify(FWLPause,PauseLayer) {
    void customSetup(){
        PauseLayer::customSetup();
        auto size=CCDirector::get()->getWinSize();
        auto menu=CCMenu::create();menu->setPosition({0,0});menu->setID("frame-window-lab-menu");
        auto sprite=ButtonSprite::create("FW Lab");sprite->setScale(.55f);
        auto button=CCMenuItemSpriteExtra::create(sprite,this,menu_selector(FWLPause::onLab));
        button->setPosition({size.width/2,25});button->setID("frame-window-lab-button");
        menu->addChild(button);addChild(menu,100);
    }
    void onLab(CCObject*){fwl::showLab(Ref<PauseLayer>(this));}
    void onPracticeMode(CCObject*sender){
        if(fwl::Engine::get().protectedRun){FLAlertLayer::create("Frame Window Lab","Stop analysis before changing practice mode.","OK")->show();return;}
        PauseLayer::onPracticeMode(sender);
    }
    void onNormalMode(CCObject*sender){
        if(fwl::Engine::get().protectedRun)return;
        PauseLayer::onNormalMode(sender);
    }
};
