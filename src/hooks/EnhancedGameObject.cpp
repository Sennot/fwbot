#include <Geode/Geode.hpp>
#include <bot/updater.hpp>
#include <safetyhook.hpp>

#include "bot/bot.hpp"
#include "physics/collisions.hpp"
#include "trajectory/trajectory.hpp"

using namespace geode::prelude;

#include <Geode/modify/EnhancedGameObject.hpp>

struct SLEnhancedGameObject : Modify<SLEnhancedGameObject, EnhancedGameObject> {
    void activatedByPlayer(PlayerObject* player) {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(player)) {
            phys::activateForTrajectory((EffectGameObject*)this, player);
        } else {
            EnhancedGameObject::activatedByPlayer(player);

            // const GJBaseGameLayer* gjbgl = GJBaseGameLayer::get();
            // const bool activated = hasBeenActivatedByPlayer(gjbgl->m_player1)
            // &&
            //     (!gjbgl->m_gameState.m_isDualMode ||
            //     hasBeenActivatedByPlayer(gjbgl->m_player2));
            // if (activated && bot->updater().m_layoutMode->inner()) {
            //     m_opacityMod = 0.5;
            //     m_opacityMod2 = 0.5;
            //     m_hasNoEffects = true;
            // }
        }
    }
};
