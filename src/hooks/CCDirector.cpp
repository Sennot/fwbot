#include <Geode/Geode.hpp>

#include "bot/bot.hpp"
#include "bot/scheduler.hpp"
#include "bot/updater.hpp"
#include "render/renderer.hpp"

using namespace geode::prelude;

#include <Geode/modify/CCDirector.hpp>

struct SLCCDirector : Modify<SLCCDirector, CCDirector> {
    void drawScene() {
        Renderer::get()->finishStop();

        if (!Bot::get()->isEnabled()) {
            return CCDirector::drawScene();
        }

        Bot::get()->scheduler().update(this->getDeltaTime());

        auto renderer = Renderer::get();
        Bot::get()->updater().updateAudioSpeedhack();

        auto playLayer = PlayLayer::get();
        if (!playLayer) {
            renderer->restoreView();
            return CCDirector::drawScene();
        }

        if (renderer->isRecording() && !playLayer->m_isPaused &&
            (playLayer->m_started ||
             renderer->m_settings.m_firstAttemptPause)) {
            float dt = Renderer::get()->getDt();

            renderer->acquireView();

            if (!renderer->tryBeginFrame()) {
                renderer->displayPreview();
                this->m_pobOpenGLView->swapBuffers();

                Bot::get()->updater().runFrozenTick();
                return;
            }

            // nuh uh
            if (!m_bPaused) {
                m_pScheduler->update(dt);
            }

            SL_LOG_DEV("Scheduler updated, setting next scene...");

            if (m_pNextScene) {
                this->setNextScene();
            }

            renderer->update(playLayer);

            renderer->displayPreview();
            this->m_pobOpenGLView->swapBuffers();

            Bot::get()->updater().runFrozenTick();

            return;
        }
        //
        // auto& pf = Bot::get()->pathfinder();
        // if (pf.isRunning()) {
        //     pf.tick();
        //
        //     Bot::get()->updater().runFrozenTick();
        //
        //     if (pf.m_renderPreview) {
        //         CCDirector::drawScene();
        //     } else {
        //         this->m_pobOpenGLView->swapBuffers();
        //     }
        //
        //     Bot::get()->updater().runFrozenTick();
        //     return;
        // }
        //
        renderer->restoreView();

        CCDirector::drawScene();
        Bot::get()->updater().runFrozenTick();
    }

    // CCSize getWinSize() {
    //     if (auto renderer = Renderer::get(); renderer->isRecording()) {
    //         float aspectRatio = (float)renderer->m_settings.m_width /
    //                             (float)renderer->m_settings.m_height;
    //
    //         cocos2d::CCSize size = CCDirector::getWinSize();
    //         float realAspectRatio = size.width / size.height;
    //
    //         if (realAspectRatio < aspectRatio) {
    //             size.height = size.width / aspectRatio;
    //         } else if (realAspectRatio > aspectRatio) {
    //             size.width = size.height * aspectRatio;
    //         }
    //
    //         return size;
    //     }
    //
    //     return CCDirector::getWinSize();
    // }
};
