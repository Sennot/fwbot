#include <Geode/Geode.hpp>

#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "checkpoint/fix.hpp"
#include "trajectory/trajectory.hpp"

using namespace geode::prelude;

#include <Geode/modify/PlayerObject.hpp>

#include "physics/collisions.hpp"
#include "physics/gjbasegamelayer.hpp"
#include "physics/object.hpp"
#include "physics/player.hpp"

struct SLPlayerObject : Modify<SLPlayerObject, PlayerObject> {
    void update(float dt) {
        auto bot = Bot::get();
        if (!bot->trajectory().isFakePlayer(this)) {
            bot->updater().m_lastPlayerX = bot->updater().m_currentPlayerX;
        }

        PlayerObject::update(dt);

        if (!bot->trajectory().isFakePlayer(this)) {
            bot->trajectory().setDelta(dt);
            bot->updater().m_currentPlayerX = this->getPositionX();
        }
    }

    void playSpawnEffect() {
        if (Bot::get()->practiceFix().m_loadCheckpoint) {
            return;
        }

        PlayerObject::playSpawnEffect();
    }

    void playDeathEffect() {
        auto& updater = Bot::get()->updater();
        if (updater.m_preventDeath->inner() || updater.m_predicting) {
            return;
        }

        PlayerObject::playDeathEffect();
    }

    void handleButton(bool down, int button, bool player1) {
        auto bot = Bot::get();
        if (button == 1) bot->trajectory().handleButton(player1, down);
    }

    void playSpiderDashEffect(cocos2d::CCPoint p0, cocos2d::CCPoint p1) {
        auto bot = Bot::get();
        if (!bot->trajectory().drawing()) {
            PlayerObject::playSpiderDashEffect(p0, p1);
        }
    }

    bool levelFlipping() {
        if (LevelEditorLayer::get()) {
            return false;
        }

        return PlayerObject::levelFlipping();
    }

    void incrementJumps() {
        auto bot = Bot::get();
        if (!bot->trajectory().drawing()) {
            PlayerObject::incrementJumps();
        }
    }

    void ringJump(RingObject* ring, bool unk) {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(this)) {
            phys::ringJump(this, ring);
        } else {
            PlayerObject::ringJump(ring, unk);
        }
    }

    void bumpPlayer(float force, int objectType, bool playEffect,
                    GameObject* object) {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(this)) {
            phys::bumpPlayer(this, force, objectType, playEffect, object);
        } else {
            PlayerObject::bumpPlayer(force, objectType, playEffect, object);
        }
    }

    void propellPlayer(float force, bool dontPlayEffect, int objectType) {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(this)) {
            phys::propellPlayer(this, force, dontPlayEffect, objectType);
        } else {
            PlayerObject::propellPlayer(force, dontPlayEffect, objectType);
        }
    }

    void startDashing(DashRingObject* obj) {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(this)) {
            phys::startDashing(this, obj);
        } else {
            PlayerObject::startDashing(obj);
        }
    }

    void removePendingCheckpoint() {
        return;  // don't, we don't use pending checkpoints anywhere
    }

    void tryPlaceCheckpoint() {
        if (!GameManager::get()->getGameVariable("0027")) {
            return;
        }

        const double checkpointTimeout = this->m_quickCheckpointMode ? 0.2 : 1;
        if ((this->m_gameLayer->m_gameState.m_totalTime -
             this->m_lastCheckpointTime) > checkpointTimeout) {
            this->m_gameLayer->m_uiLayer->onCheck(nullptr);
            this->m_shouldTryPlacingCheckpoint = false;
            this->m_lastCheckpointTime = this->m_totalTime;
        }
    }

    void releaseAllButtons() {
        if (!Bot::get()->isEnabled()) {
            return PlayerObject::releaseAllButtons();
        }

        auto bot = Bot::get();
        auto gjbgl = GJBaseGameLayer::get();
        if ((this == gjbgl->m_player2 && !gjbgl->m_gameState.m_isDualMode) ||
            bot->updater().m_canDie->inner() || bot->isPlaying()) {
            PlayerObject::releaseAllButtons();
        }
    }

#ifdef GEODE_IS_WINDOWS
    void stopDashing() {
        auto bot = Bot::get();
        if (bot->trajectory().isFakePlayer(this)) {
            phys::stopDashing(this);
        } else {
            PlayerObject::stopDashing();
        }
    }
#endif
};
