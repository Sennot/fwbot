#include <Geode/Geode.hpp>
#include <Geode/binding/ForceBlockGameObject.hpp>
#include <Geode/binding/VideoOptionsLayer.hpp>

#include "../bot/bot.hpp"
#include "Geode/cocos/platform/win32/CCEGLView.h"
#include "ui/hook.hpp"
// #include "../recorder/recorder.hpp"

using namespace geode::prelude;

#include <Geode/modify/ForceBlockGameObject.hpp>
#include <Geode/modify/VideoOptionsLayer.hpp>

struct SLVideoOptionsLayer : Modify<SLVideoOptionsLayer, VideoOptionsLayer> {
    void onApply(cocos2d::CCObject* sender) {
        VideoOptionsLayer::onApply(sender);

        auto size = CCDirector::get()->getWinSizeInPixels();

        ImGuiHookCtx::get().handleResize(size.width, size.height);
    }
};
