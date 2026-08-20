#include <Geode/Geode.hpp>

#include "bot/bot.hpp"
#include "bot/updater.hpp"

using namespace geode::prelude;

#include <Geode/modify/CCScheduler.hpp>

struct SLCCScheduler : Modify<SLCCScheduler, CCScheduler> {
    void update(float dt) override {
        const auto bot = Bot::get();
        if (bot->updater().m_onlyRefresh || !bot->isEnabled()) {
            CCScheduler::update(dt);  // only update at the cocos level
            return;
        }

        bot->updater().runUpdates(
            [this](float dt) { this->CCScheduler::update(dt); }, dt, false);
    }
};
