#include <Geode/Geode.hpp>

#include "../bot/bot.hpp"
#include "bot/updater.hpp"
// #include "../recorder/recorder.hpp"

using namespace geode::prelude;

#include <Geode/modify/ShaderLayer.hpp>

struct SLShaderLayer : Modify<SLShaderLayer, ShaderLayer> {
    void performCalculations() {
        if (Bot::get()->isEnabled() &&
            Bot::get()->updater().m_layoutMode->inner() &&
            !LevelEditorLayer::get()) {
            m_state.m_usesShaders = false;
            return;
        }

        ShaderLayer::performCalculations();
    }
};
