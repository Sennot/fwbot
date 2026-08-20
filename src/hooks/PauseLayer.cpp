#include <Geode/Geode.hpp>
#include <safetyhook.hpp>
#include <ui/manager.hpp>

#include "assist/hitboxes.hpp"
#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "checkpoint/fix.hpp"
#include "render/renderer.hpp"
#include "replay/system.hpp"
#include "trajectory/trajectory.hpp"
#include "util/midhook.hpp"

using namespace geode::prelude;

#include <Geode/modify/PauseLayer.hpp>

struct SLPauseLayer : Modify<SLPauseLayer, PauseLayer> {
    void goEdit() {
        if (Renderer::get()->isRecording()) {
            Renderer::get()->signalStop();
        }

        Renderer::get()->restoreView();

        auto bot = Bot::get();

        bot->updater().setPaused(false);

        bot->trajectory().uninit();
        bot->hitboxes().destroy();

        PauseLayer::goEdit();
        bot->practiceFix().removeAll();
        bot->replaySystem().onExit();
    }

    void customSetup() override {
        PauseLayer::customSetup();

        CCSprite* sprite = CCSprite::create("silicate-button.png"_spr);
        sprite->setScale(0.35f);

        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(SLPauseLayer::onSilicateOpen));

        CCNode* menu = this->getChildByID("right-button-menu");
        menu->addChild(btn);
        menu->updateLayout();
    }

   public:
    void onSilicateOpen(CCObject*) { Bot::get()->ui().toggle(); }
};

static void releaseButtonsMidhook(SafetyHookContext& ctx) {
    if (Bot::get()->isPlaying()) {
        ctx.rip += 5;
    }
}

$execute {
    // In PlayLayer::pauseGame
    util::midhook(geode::base::get() + 0x3BA239, "pauseReleaseButtons",
                  releaseButtonsMidhook);
    util::midhook(geode::base::get() + 0x3BA27C, "pauseReleaseButtons",
                  releaseButtonsMidhook);
    // In PlayLayer::resume
    util::midhook(geode::base::get() + 0x3BA879, "resumeReleaseButtons",
                  releaseButtonsMidhook);
}
