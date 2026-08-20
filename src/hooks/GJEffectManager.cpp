#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/GJEffectManager.hpp>

#include "bot/bot.hpp"
#include "bot/updater.hpp"

struct SLGJEffectManager : Modify<SLGJEffectManager, GJEffectManager> {
    void updateEffects(float dt) {
        if (Bot::get()->isEnabled() &&
            Bot::get()->updater().m_layoutMode->inner() &&
            !LevelEditorLayer::get()) {
            m_pulseEffectMap.clear();
            m_pulseEffectVector.clear();
            m_opacityEffectMap.clear();
        }

        GJEffectManager::updateEffects(dt);
    }
};
