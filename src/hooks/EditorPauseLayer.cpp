#include <Geode/Geode.hpp>
#include <Geode/binding/EditorPauseLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>

#include "assist/hitboxes.hpp"
#include "bot/bot.hpp"
#include "trajectory/trajectory.hpp"

using namespace geode::prelude;

struct SLEditorPauseLayer : Modify<SLEditorPauseLayer, EditorPauseLayer> {
    void onSaveAndPlay(cocos2d::CCObject* sender) {
        Bot::get()->hitboxes().destroy();
        Bot::get()->trajectory().uninit();

        EditorPauseLayer::onSaveAndPlay(sender);
    }
};
