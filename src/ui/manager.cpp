#include "manager.hpp"

#include <winuser.h>

#include <Geode/Enums.hpp>
#include <Geode/Geode.hpp>
#include <algorithm>
#include <slc/formats/v3/atom.hpp>
#include <slc/formats/v3/replay.hpp>
#include <tabby.hpp>
#include <variant>
#include <widgets/button.hpp>

#include "Geode/utils/string.hpp"
#include "assist/autoclicker.hpp"
#include "assist/hitboxes.hpp"
#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "checkpoint/fix.hpp"
#include "frame_window/analyzer.hpp"
#include "hook.hpp"
#include "label/label.hpp"
#include "render/dsp.hpp"
#include "render/renderer.hpp"
#include "replay/system.hpp"
#include "settings/settings.hpp"
#include "shared/keys.hpp"
#include "shared/value/value.hpp"
#include "trajectory/trajectory.hpp"
// #include "widgets/hotkey.hpp"
#include "imgui.h"
#include "util/config.hpp"
#include "widgets/checkbox.hpp"
#include "widgets/image.hpp"
#include "widgets/input.hpp"

#ifdef SILICATE_PROTECT
#include "VMProtect/VMProtectSDK.h"
#endif

using namespace geode::prelude;

UIManager::UIManager() : m_font(nullptr), m_medium(nullptr), m_bold(nullptr) {}

void UIManager::toggle() { m_state.toggle(); }

static std::vector<Theme> s_themes = {
    Theme("Silica", "title_new.png",
          R"(#version 130
        #extension GL_ARB_explicit_attrib_location : require
        #extension GL_ARB_explicit_uniform_location : require

        in vec2 v_texCoord;
        out vec4 fragColor;

        layout(location = 0) uniform sampler2D u_texture;
        layout(location = 1) uniform vec2 u_texelSize;
        layout(location = 2) uniform vec2 u_direction;
        layout(location = 3) uniform vec4 u_window;
        layout(location = 4) uniform float u_time;

        void main() {
            fragColor = texture2D(u_texture, v_texCoord);
        }
        )",
          1.0),
    Theme("Polychrome", "title_gay_new.png",
          R"(#version 130
        #extension GL_ARB_explicit_attrib_location : require
        #extension GL_ARB_explicit_uniform_location : require

        in vec2 v_texCoord;
        out vec4 fragColor;

        layout(location = 0) uniform sampler2D u_texture;
        layout(location = 1) uniform vec2 u_texelSize;
        layout(location = 2) uniform vec2 u_direction;
        layout(location = 3) uniform vec4 u_window;
        layout(location = 4) uniform float u_time;

        void main() {
            fragColor = vec4(
                texture2D(u_texture, v_texCoord + vec2(0.01, 0.0)).x,
                texture2D(u_texture, v_texCoord + vec2(0.0, -0.01)).y,
                texture2D(u_texture, v_texCoord + vec2(-0.01, 0.0)).z,
                1.0
            );
            fragColor = clamp(fragColor, 0.15, 1.0);

            vec2 uv = v_texCoord;

            vec4 col =  vec4(
                abs(sin(uv.y + 2.0)) * abs(sin(uv.x * 2.0 + u_time)) * 1.0,
                abs(sin(uv.y + 2.0)) * abs(sin(uv.x * 2.0 + u_time + 0.4)) * 1.0,
                abs(sin(uv.y + 2.0)) * abs(sin(uv.x * 2.0 + u_time + 0.8)) * 1.0,
                1.0
            );

            vec4 col2 =  vec4(
                abs(sin(uv.y)) * abs(sin(uv.x * 2.0 + u_time + 1.0)) * 1.0,
                abs(sin(uv.y)) * abs(sin(uv.x * 2.0 + u_time + 1.5)) * 1.0,
                abs(sin(uv.y)) * abs(sin(uv.x * 2.0 + u_time + 2.0)) * 1.0,
                1.0
            );

            vec4 rainbowCol = col + col2;

            fragColor *= rainbowCol;
        }
        )",
          3.0),
    Theme("Estradiol", "title_trans_new.png",
          R"(#version 130
        #extension GL_ARB_explicit_attrib_location : require
        #extension GL_ARB_explicit_uniform_location : require

        in vec2 v_texCoord;
        out vec4 fragColor;

        layout(location = 0) uniform sampler2D u_texture;
        layout(location = 1) uniform vec2 u_texelSize;
        layout(location = 2) uniform vec2 u_direction;
        layout(location = 3) uniform vec4 u_window;
        layout(location = 4) uniform float u_time;

        void main() {

            fragColor = texture2D(u_texture, v_texCoord);

            vec2 uv = v_texCoord;

            vec4 col =  vec4(
                abs(sin(uv.y + 2.0)) * abs(sin(uv.x * 2.0 + u_time)) * 2.0,
                abs(sin(uv.y + 2.0)) * abs(sin(uv.x * 2.0 + u_time + 0.4)) * 0.0,
                abs(sin(uv.y + 2.0)) * abs(sin(uv.x * 2.0 + u_time)) * 2.0,
                1.0
            );

            vec4 col2 =  vec4(
                abs(sin(uv.y)) * abs(sin(uv.x * 2.0 + u_time + 1.0)) * 0.0,
                abs(sin(uv.y)) * abs(sin(uv.x * 2.0 + u_time + 2.0)) * 2.0,
                abs(sin(uv.y)) * abs(sin(uv.x * 2.0 + u_time + 2.0)) * 2.0,
                1.0
            );

            vec4 rainbowCol = col + col2;

            fragColor = mix(fragColor, rainbowCol, 0.4);
        }
        )",
          1.55),
    Theme("Endothermic", "title_jolly_new.png",
          R"(#version 130
        #extension GL_ARB_explicit_attrib_location : require
        #extension GL_ARB_explicit_uniform_location : require

        #define _SnowflakeAmount 250
        #define _BlizardFactor 0.15

        in vec2 v_texCoord;
        out vec4 fragColor;

        layout(location = 0) uniform sampler2D u_texture;
        layout(location = 1) uniform vec2 u_texelSize;
        layout(location = 2) uniform vec2 u_direction;
        layout(location = 3) uniform vec4 u_window;
        layout(location = 4) uniform float u_time;

        float rnd(float x) {
            return fract(sin(dot(vec2(x+47.49,38.2467/(x+2.3)), vec2(12.9898, 78.233)))* (43758.5453));
        }

        float drawCircle(vec2 center, float radius) {
            vec2 uv = v_texCoord * vec2(1.0, u_texelSize.x / u_texelSize.y);

            return 1.0 - smoothstep(0.0, radius, length(uv - center));
        }

        void main() {
            fragColor = texture2D(u_texture, v_texCoord);

            vec2 uv = v_texCoord;
            fragColor += vec4(0.2, 0.2, 0.6, 1.0);

            for (int i=0; i < _SnowflakeAmount; i++) {

                float j = float(i);
                float speed = 0.3+rnd(cos(j))*(0.7+0.5*cos(j/(float(_SnowflakeAmount)*0.25)));

                vec2 center = vec2((0.25-uv.y)*_BlizardFactor+rnd(j)+0.1*cos(u_time+sin(j)), mod(sin(j)-speed*(u_time*1.5*(0.1+_BlizardFactor)), 0.65));

                float circle = drawCircle(center, 0.001 + speed * 0.012) * ((0.001 + speed * 0.012) * 50.0);

                fragColor += vec4(circle * 0.4, circle * 0.4, circle * 0.5, circle * 0.35);

            }
        }
        )",
          1.0),
    Theme("Cyanide", "title_new.png",
          R"(#version 130
        #extension GL_ARB_explicit_attrib_location : require
        #extension GL_ARB_explicit_uniform_location : require

        in vec2 v_texCoord;
        out vec4 fragColor;

        layout(location = 0) uniform sampler2D u_texture;
        layout(location = 1) uniform vec2 u_texelSize;
        layout(location = 2) uniform vec2 u_direction;
        layout(location = 3) uniform vec4 u_window;
        layout(location = 4) uniform float u_time;

        // Adapted from https://www.shadertoy.com/view/4st3DM
        void main() {
            fragColor = texture2D(u_texture, v_texCoord);
            float gray = 0.299 * fragColor.x + 0.587 * fragColor.y + 0.114 * fragColor.z;
            fragColor = mix(vec4(gray, gray, gray, 1.0), fragColor, 0.3);
            fragColor -= vec4(0.3);

            vec2 uv = v_texCoord;

            float n = u_time;
            float x = uv.x * (sin(uv.y + u_time * 0.5)* 2.0);
            float y = uv.y * (sin(uv.x + u_time * 0.2)* 2.0);

            float xp = uv.x-0.5+sin(x*3.+n-sin(y*7.+n));
            float yp = uv.y-0.5+sin(y*3.+n+sin(x*5.-n));

            float eh = ((sqrt(xp*xp+yp*yp)*5.+n));

            vec3 one = vec3(0.2, 0.7, 0.5) * 0.8;
            vec3 two = vec3(0.9, 0.2, 0.5) * 0.8;

            vec4 phase = vec4(mix(one, two, (sin(u_time) + 1.0) / 2.0), 1.0);

            fragColor += phase;
            fragColor += vec4(
                sin(eh*0.6+(y+n)*5.-n*5.),
                sin(eh*0.6+(y+n)*5.-n*5.),
                sin(eh*0.6+(y+n)*5.-n*5.),
            1.0) * vec4(one, 1.0) * vec4(0.4);

            fragColor += vec4(
                sin(eh*0.5+(y+n*1.1)*5.-n*5.),
                sin(eh*0.5+(y+n*1.1)*5.-n*5.),
                sin(eh*0.5+(y+n*1.1)*5.-n*5.),
            1.0) * vec4(two, 1.0) * vec4(0.4);

            float light = 0.299 * fragColor.x + 0.587 * fragColor.y + 0.114 * fragColor.z;
            vec4 mixed = (fragColor * vec4(mix(two, one, (sin(u_time) + 1.0) / 2.0), 1.0));
            fragColor = mix(fragColor, mixed, clamp(light - 0.6, 0.0, 1.0));

            // fragColor -= vec4(
            //     sin(eh*0.7+(y+n*1.2)*5.-n*5.),
            //     sin(eh*0.7+(y+n*1.2)*5.-n*5.),
            //     sin(eh*0.7+(y+n*1.2)*5.-n*5.),
            // 1.0) * phase * vec4(0.2);
        }
        )",
          2.0),
};

static uint64_t fnv1aHash(const std::vector<uint8_t>& data) {
    uint64_t hash = 14695981039346656037ull;

    for (const uint8_t byte : data) {
        hash ^= byte;
        hash *= 1099511628211;
    }

    return hash;
}

using DpiGetterType = decltype(GetDpiForWindow)*;
static DpiGetterType g_dpiGetter = nullptr;

static float getWindowDpi() {
    if (!g_dpiGetter) return 1.0;

    uint32_t dpi = g_dpiGetter(ImGuiHookCtx::get().m_hWnd);
    float scaling = (float)dpi / 96.0;

    return scaling;
}

// EPIC code
static std::string ffmpegUrl = "https://cdn.silicate.dev/ffmpeg.zip";

void UIManager::setup() {
    m_state.m_animationSpeed->handle([](float& speed) {
        if (speed < 0.1f || speed > 3.0f) {
            speed = 1.0f;
        }

        tabby::TabbyGlobalCfg::get().animationSpeed = speed;
    });

    m_state.m_playAnimations->handle(
        [](bool& play) { tabby::TabbyGlobalCfg::get().playAnimations = play; });

    g_dpiGetter = (DpiGetterType)GetProcAddress(GetModuleHandleA("user32.dll"),
                                                "GetDpiForWindow");

    m_state.m_uiScale->handle([this](float& scale) {
        tabby::TabbyGlobalCfg::get().uiScale = scale * getWindowDpi();
        m_state.m_restartGameInfo = true;
    });

    tabby::TabbyGlobalCfg::get().uiScale =
        m_state.m_uiScale->inner() * geode::utils::getDisplayFactor();

    static ImVector<ImWchar> glyphRanges;

    ImFontGlyphRangesBuilder builder;
    builder.AddChar(0xf192);
    builder.AddChar(0xefba);
    builder.AddChar(0xf03d);
    builder.AddChar(0xf121);
    builder.AddChar(0xf013);
    builder.AddChar(0xf078);
    builder.AddChar(0xf044);
    builder.AddChar(0xf054);
    builder.AddChar(0xf00c);
    builder.AddChar(0xf51b);
    builder.AddChar(0xf004);
    builder.AddChar(0xf0fe);
    builder.BuildRanges(&glyphRanges);

    ImFontConfig mediumFontCfg;
    mediumFontCfg.OversampleH = 3;
    mediumFontCfg.OversampleV = 3;
    // mediumFontCfg.FontDataOwnedByAtlas = true;
    mediumFontCfg.GlyphExtraAdvanceX = -1.0f * 20.0f * 0.02f;

    ImFont* mediumFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        geode::utils::string::pathToString(Mod::get()->getResourcesDir() /
                                           "font_medium.ttf")
            .c_str(),
        20.0f, &mediumFontCfg);
    mediumFontCfg.MergeMode = true;
    mediumFontCfg.GlyphOffset = ImVec2(0.0f, -1.0f);
    ImGui::GetIO().Fonts->AddFontFromFileTTF(
        geode::utils::string::pathToString(Mod::get()->getResourcesDir() /
                                           "font_symbols.ttf")
            .c_str(),
        18.0f, &mediumFontCfg, glyphRanges.Data);

    ImGui::GetIO().Fonts->Build();

    ImFontConfig mainFontCfg;
    mainFontCfg.OversampleH = 3;
    mainFontCfg.OversampleV = 3;
    // mainFontCfg.FontDataOwnedByAtlas = true;
    mainFontCfg.GlyphExtraAdvanceX = -1.0f * 17.0f * 0.03f;

    ImFont* mainFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        geode::utils::string::pathToString(Mod::get()->getResourcesDir() /
                                           "font_main.ttf")
            .c_str(),
        17.0f, &mainFontCfg);
    mainFontCfg.MergeMode = true;
    mainFontCfg.GlyphOffset = ImVec2(0.0f, -2.0f);
    ImGui::GetIO().Fonts->AddFontFromFileTTF(
        geode::utils::string::pathToString(Mod::get()->getResourcesDir() /
                                           "font_symbols.ttf")
            .c_str(),
        14.0f, &mainFontCfg, glyphRanges.Data);

    ImGui::GetIO().Fonts->Build();

    m_font = tabby::Font(mainFont);
    m_medium = tabby::Font(mediumFont);

    m_bold =
        tabby::Font::load(geode::utils::string::pathToString(
                              Mod::get()->getResourcesDir() / "font_bold.ttf"),
                          32.0f);

    m_state.m_rainbow->notifyChange();
    m_state.m_playAnimations->notifyChange();
    m_state.m_animationSpeed->notifyChange();

    for (const auto& label : Bot::get()->labels().m_labels) {
        m_state.m_labelState.options.push_back(label.getFriendlyName().c_str());
    }

    m_state.m_replayNames.clear();
    for (const auto& entry : std::filesystem::directory_iterator(
             Mod::get()->getPersistentDir() / "replays")) {
        if (entry.is_regular_file() && entry.path().extension() == ".slc") {
            m_state.m_replayNames.push_back(entry.path().stem().string());
        }
    }

    m_state.m_presetNames.clear();
    for (const auto& entry : std::filesystem::directory_iterator(
             Mod::get()->getPersistentDir() / "presets")) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            m_state.m_presetNames.push_back(entry.path().stem().string());
        }
    }

    m_state.m_scriptNames.clear();
    for (const auto& entry : std::filesystem::directory_iterator(
             Mod::get()->getPersistentDir() / "scripts")) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            m_state.m_scriptNames.push_back(entry.path().stem().string());
        }
    }

    m_replayAutocomplete.suggestions = m_state.m_replayNames;
    m_state.m_presetAutocomplete.suggestions = m_state.m_presetNames;
    m_state.m_scriptAutocomplete.suggestions = m_state.m_scriptNames;

    m_state.m_bgColorState.colors = SLSettings::get()->layoutBgColor;
    m_state.m_groundColorState.colors = SLSettings::get()->layoutGroundColor;

    for (auto& theme : s_themes) {
        theme.initialize();
    }

    m_theme = &s_themes[SLSettings::get()->theme];
    m_state.m_themeState.selectedIndex = SLSettings::get()->theme;
    m_theme->apply();

    for (const auto& theme : s_themes) {
        m_state.m_themeState.options.push_back(theme.m_name);
    }

    Renderer::get()->loadFFmpeg();
}

void UIManager::reinitThemeGraphics() {
    for (auto& theme : s_themes) {
        theme.initialize();
    }
    if (m_theme) {
        m_theme->apply();
    }
}

static void preDrawBlurEffect(const ImDrawList*, const ImDrawCmd* cmd) {
    auto pos = ImGui::GetDrawData()->DisplayPos;
    auto sz = ImGui::GetDrawData()->DisplaySize;

    ImGuiHookCtx::get().preSampleBlur(
        ImVec4((cmd->ClipRect.x - pos.x) / sz.x,
               1.0 - ((cmd->ClipRect.w - pos.y) / sz.y),
               (cmd->ClipRect.z - pos.x) / sz.x,
               1.0 - ((cmd->ClipRect.y - pos.y) / sz.y)));
}

static void drawBlurEffectFirstPass(const ImDrawList*, const ImDrawCmd*) {
    ImGuiHookCtx::get().sampleBlurFirstPass();
}

static void drawBlurEffectSecondPass(const ImDrawList*, const ImDrawCmd*) {
    ImGuiHookCtx::get().sampleBlurSecondPass();
}

static void drawPostprocess(const ImDrawList*, const ImDrawCmd*) {
    ImGuiHookCtx::get().sampleBlurPostprocess();
}

static void postDrawBlurEffect(const ImDrawList*, const ImDrawCmd*) {
    ImGuiHookCtx::get().postSampleBlur();
}

static void renderBlurBg(float rounding = 24.0f, float borderSize = 2.5f,
                         bool useShader = true, float bgOpacity = 0.15f,
                         bool pp = false) {
    rounding *= tabby::TabbyGlobalCfg::get().uiScale;
    borderSize *= tabby::TabbyGlobalCfg::get().uiScale;

    bgOpacity = std::pow(bgOpacity, Bot::get()->ui().m_theme->m_opacityExp);

    if (!useShader) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetWindowPos(),
            ImGui::GetWindowPos() + ImGui::GetWindowSize(),
            ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, bgOpacity)), rounding,
            ImDrawFlags_RoundCornersAll);

        // window border
        ImGui::GetWindowDrawList()->AddRect(
            ImGui::GetWindowPos(),
            ImGui::GetWindowPos() + ImGui::GetWindowSize(),
            ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 0.2f)), rounding,
            ImDrawFlags_RoundCornersAll, borderSize);

        return;
    }

    ImGuiHookCtx::get().m_renderData.m_size =
        cocos2d::CCSize(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
    ImGuiHookCtx::get().m_renderData.m_pos =
        cocos2d::CCPoint(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    cocos2d::CCSize frameSize = cocos2d::CCSize(ImGuiHookCtx::get().m_width,
                                                ImGuiHookCtx::get().m_height);

    drawList->AddCallback(&preDrawBlurEffect, nullptr);

    ImVec2 origin_pos = ImGui::GetWindowPos();
    ImVec2 dest_pos = ImGui::GetWindowPos() + ImGui::GetWindowSize();

    float origin_uv_x = ImGui::GetWindowPos().x /
                        ImGuiHookCtx::get().m_blurPass.m_width /
                        ImGuiHookCtx::BLUR_DOWNSCALING_FACTOR;
    float origin_uv_y = (frameSize.height - ImGui::GetWindowPos().y) /
                        ImGuiHookCtx::get().m_blurPass.m_height /
                        ImGuiHookCtx::BLUR_DOWNSCALING_FACTOR;

    float dest_uv_x = (ImGui::GetWindowPos().x + ImGui::GetWindowSize().x) /
                      ImGuiHookCtx::get().m_blurPass.m_width /
                      ImGuiHookCtx::BLUR_DOWNSCALING_FACTOR;
    float dest_uv_y = (frameSize.height - ImGui::GetWindowPos().y -
                       ImGui::GetWindowSize().y) /
                      ImGuiHookCtx::get().m_blurPass.m_height /
                      ImGuiHookCtx::BLUR_DOWNSCALING_FACTOR;

    const int SAMPLE_COUNT = 8;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        drawList->AddCallback(&drawBlurEffectFirstPass, nullptr);
        drawList->AddImage(
            ImTextureRef((ImTextureID)ImGuiHookCtx::get().m_blurPass.m_tex),
            {-1.0, -1.0}, {1.0, 1.0});

        if (i == SAMPLE_COUNT - 1 && pp) {
            drawList->AddCallback(&drawPostprocess, nullptr);
        } else {
            drawList->AddCallback(&drawBlurEffectSecondPass, nullptr);
        }

        drawList->AddImage(
            ImTextureRef((ImTextureID)ImGuiHookCtx::get().m_inputTex),
            {-1.0, -1.0}, {1.0, 1.0});
    }

    drawList->AddCallback(&postDrawBlurEffect, nullptr);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    drawList->AddImageRounded(
        ImTextureRef((ImTextureID)ImGuiHookCtx::get().m_blurPass.m_tex),
        origin_pos, dest_pos, {origin_uv_x, origin_uv_y},
        {dest_uv_x, dest_uv_y}, ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 1.0)),
        rounding, ImDrawFlags_RoundCornersAll);

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(),
        ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, bgOpacity)), rounding,
        ImDrawFlags_RoundCornersAll);

    // window border
    ImGui::GetWindowDrawList()->AddRect(
        ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(),
        ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 0.1f)), rounding,
        ImDrawFlags_RoundCornersAll, borderSize);
}

static std::vector<std::string> filterCandidates(
    const std::vector<std::string>& candidates, const std::string& filter) {
    std::vector<std::string> filtered;
    std::string lowerFilter = filter;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                   ::tolower);

    for (const auto& candidate : candidates) {
        std::string lowerCandidate = candidate;
        std::transform(lowerCandidate.begin(), lowerCandidate.end(),
                       lowerCandidate.begin(), ::tolower);
        if (lowerCandidate.find(lowerFilter) == 0) {
            filtered.push_back(candidate);
        }
    }

    return filtered;
}

void UIManager::draw() {
#ifdef SILICATE_PROTECT
    VMProtectBegin("UIDraw");
#endif

    auto bot = Bot::get();
    auto& cfg = tabby::TabbyGlobalCfg::get();

    ImGuiHookCtx::get().m_time += cocos2d::CCDirector::get()->getDeltaTime();
    const auto popupShaderFn = [&]() {
        renderBlurBg(12.0f, 1.5f, m_state.m_useShader->inner(),
                     m_state.m_opacity->inner());
    };

    cfg.uiScale = m_state.m_uiScale->inner() * getWindowDpi();

    if (!m_state.m_visible->inner()) {
        auto view = CCEGLView::get();

        auto pl = PlayLayer::get();
        bool shouldHide = false;
        if (pl) {
            shouldHide = !pl->m_isPaused && !pl->m_hasCompletedLevel &&
                         !GameManager::get()->getGameVariable("0024");
        }

        view->showCursor(!shouldHide);
        return;
    }

    CCEGLView::get()->showCursor(true);

    tabby::ScopedFont s(m_font);
    // clip all that start with m_state.m_replayName (case insensitive) (std)

    tabby::window("Silicate", [this, bot, popupShaderFn]() {
        if (m_state.m_useShader->inner()) {
            renderBlurBg(24.0f, 2.5f, true, m_state.m_opacity->inner(), true);
        } else {
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetWindowPos(),
                ImGui::GetWindowPos() + ImGui::GetWindowSize(),
                ImGui::GetColorU32(
                    ImVec4(0.1f, 0.1f, 0.1f, m_state.m_opacity->inner())),
                24.0f * tabby::TabbyGlobalCfg::get().uiScale,
                ImDrawFlags_RoundCornersAll);
        }

        tabby::section(
            "Navigation",
            [&]() {
                double width = tabby::TabbyGlobalCfg::get().widgetWidth;
                ImGui::Image(static_cast<ImTextureID>(m_theme->m_texture),
                             ImVec2(width, width * 0.3572923f));

                tabby::divider(false);

                float uiScale = tabby::TabbyGlobalCfg::get().uiScale;
                tabby::ScopedFont sm(m_medium);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                    ImVec2(18.0f * uiScale, 9.0f * uiScale));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                                    12.0f * uiScale);

                if (tabby::button_selector(
                        "\uf192    Record##Tab",
                        m_state.m_currentTab == UIState::UITab::Record)
                        .pressed) {
                    m_state.m_currentTab = UIState::UITab::Record;
                }

                if (tabby::button_selector(
                        "\uefba    Assist##Tab",
                        m_state.m_currentTab == UIState::UITab::Assist)
                        .pressed) {
                    m_state.m_currentTab = UIState::UITab::Assist;
                }

                if (tabby::button_selector(
                        "\uf51b    Prediction##Tab",
                        m_state.m_currentTab == UIState::UITab::Prediction)
                        .pressed) {
                    m_state.m_currentTab = UIState::UITab::Prediction;
                }

                if (tabby::button_selector(
                        "\uf01f    Edit##Tab",
                        m_state.m_currentTab == UIState::UITab::Edit)
                        .pressed) {
                    m_state.m_currentTab = UIState::UITab::Edit;
                }

                if (tabby::button_selector(
                        "\uf201    Frame Window##Tab",
                        m_state.m_currentTab == UIState::UITab::FrameWindow)
                        .pressed) {
                    m_state.m_currentTab = UIState::UITab::FrameWindow;
                }

                if (tabby::button_selector(
                        "\uf03d    Render##Tab",
                        m_state.m_currentTab == UIState::UITab::Render)
                        .pressed) {
                    m_state.m_currentTab = UIState::UITab::Render;
                }

                if (m_state.m_showExperimentalFeatures) {
                    if (tabby::button_selector(
                            "\uf0fe    Keybinds##Tab",
                            m_state.m_currentTab == UIState::UITab::Keybinds)
                            .pressed) {
                        m_state.m_currentTab = UIState::UITab::Keybinds;
                    }
                }

                if (tabby::button_selector(
                        "\uf013    Settings##Tab",
                        m_state.m_currentTab == UIState::UITab::Settings)
                        .pressed) {
                    m_state.m_currentTab = UIState::UITab::Settings;
                }

                ImGui::PopStyleVar(2);
            },
            200.0f, true);

        tabby::section("Main", [&]() {
            if (!Bot::get()->isEnabled()) {
                tabby::text("The bot is currently disabled.");
                if (GJBaseGameLayer::get()) {
                    tabby::text(
                        "Please exit the level you're in to enable the bot.");
                } else {
                    if (tabby::button("Enable").pressed) {
                        Bot::get()->m_enabled->inner() = true;
                        Bot::get()->m_enabled->notifyChange();
                    }
                }

                return;
            }

            tabby::tab(m_state.m_currentTab, UIState::UITab::Record, [&]() {
                tabby::text("Record", m_bold);

                tabby::divider(false);
                auto& rs = Bot::get()->replaySystem();

                if (tabby::input_text_autocomplete(
                        "Replay Name", "Replay", rs.m_replayName,
                        m_replayAutocomplete, popupShaderFn)
                        .changed) {
                    m_replayAutocomplete.suggestions = filterCandidates(
                        m_state.m_replayNames, rs.m_replayName);
                }
                if (m_state.m_lastReplayName != rs.m_replayName) {
                    m_state.m_lastReplayName = rs.m_replayName;
                    m_state.m_replayNames.clear();
                    for (const auto& entry :
                         std::filesystem::directory_iterator(
                             Mod::get()->getPersistentDir() / "replays")) {
                        if (entry.is_regular_file() &&
                            entry.path().extension() == ".slc") {
                            m_state.m_replayNames.push_back(
                                entry.path().stem().string());
                        }
                    }
                }

                tabby::fraction(2.0, [&]() {
                    if (tabby::button("Load").pressed) {
                        auto path = rs.getCurrentPath();
                        rs.load(path);
                    }

                    tabby::same_line();

                    if (tabby::button("Save").pressed) {
                        auto path = rs.getCurrentPath();
                        rs.backupExisting(path);
                        rs.save(path);

                        m_state.m_replayNames.clear();
                        for (const auto& entry :
                             std::filesystem::directory_iterator(
                                 Mod::get()->getPersistentDir() / "replays")) {
                            if (entry.is_regular_file() &&
                                entry.path().extension() == ".slc") {
                                m_state.m_replayNames.push_back(
                                    entry.path().stem().string());
                            }
                        }
                    }
                });

                if (tabby::radio(bot->m_mode, Bot::Mode::Recording,
                                 "Record##Mode")
                        .changed) {
                    // prune all inputs after current frame if in a level
                    if (PlayLayer::get()) {
                        if (rs.getInputIndex() < rs.m_actionAtom.length()) {
                            rs.createBackup();
                            rs.m_actionAtom.clipActions(
                                bot->updater().getFrame());
                        }
                    }
                }

                tabby::spacer(16.0);
                tabby::radio(bot->m_mode, Bot::Mode::Playing, "Play##Mode");

                tabby::divider();

                if (tabby::drag("TPS", Bot::get()->updater().m_tps->inner(),
                                0.0, std::numeric_limits<double>::max(), 1.0f,
                                "{:g}")
                        .changed) {
                    Bot::get()->updater().m_tps->notifyChange();
                }

                auto pl = PlayLayer::get();
                if (pl && bot->isRecording() &&
                    m_state.m_showExperimentalFeatures) {
                    if (tabby::button("Add TPS Change").pressed) {
                        (void)bot->replaySystem().m_actionAtom.addAction(
                            bot->updater().getFrame() - 1,
                            bot->updater().getTps());
                        bot->updater().estimatedStepCount = 0;
                    }
                }

                if (tabby::drag("Speed",
                                Bot::get()->updater().m_speedhack->inner(), 0.0,
                                std::numeric_limits<double>::max(), 0.01f,
                                "{:.2G}x")
                        .changed) {
                    Bot::get()->updater().m_speedhack->notifyChange();
                }

                tabby::divider();

                tabby::checkbox("Frame Advance",
                                Bot::get()->updater().m_paused->inner());

                // tabby::overlay([]() {
                //     tabby::checkbox("Frame Advance",
                //                     Bot::get()->updater().m_paused->inner());
                // }, [this, popupShaderFn]() {
                //     tabby::text("keybinds for frame advance");
                //
                //     tabby::dropdown("hi", m_state.m_lockDeltaState,
                //     m_state.m_lockDeltaState.selectedIndex, popupShaderFn,
                //     false);
                // }, popupShaderFn);

                tabby::checkbox("Intentional Death",
                                Bot::get()->updater().m_canDie->inner());

                if (m_state.m_showExperimentalFeatures) {
                    tabby::checkbox(
                        "Frame Extrapolation",
                        Bot::get()->updater().m_extrapolateFrames->inner());
                }

                tabby::divider();

                tabby::checkbox("Seed Override",
                                Bot::get()->replaySystem().m_overrideSeed);

                if (Bot::get()->replaySystem().m_overrideSeed) {
                    tabby::drag("Seed",
                                Bot::get()->replaySystem().m_overriddenSeed,
                                0ull, std::numeric_limits<uint64_t>::max());
                }
            });

            tabby::tab(m_state.m_currentTab, UIState::UITab::Assist, [&]() {
                tabby::text("Assist", m_bold);

                tabby::divider(false);

                tabby::text("Hitboxes", m_medium);
                tabby::checkbox("Toggle##Hitboxes",
                                Bot::get()->hitboxes().m_enabled->inner());

                tabby::checkbox("Show Trail##Hitboxes",
                                Bot::get()->hitboxes().m_trailEnabled->inner());

                if (Bot::get()->hitboxes().m_trailEnabled->inner())
                {
                    tabby::checkbox("Player Hitbox Above Trail##Hitboxes",
                                    Bot::get()->hitboxes().m_playerAbove->inner());
                    tabby::checkbox("Rotated Hitbox Above Trail##Hitboxes",
                                    Bot::get()->hitboxes().m_rotatedAbove->inner());
                }

                tabby::drag("Width##Hitboxes",
                            Bot::get()->hitboxes().m_width->inner(), 0.0, 1.0,
                            0.02f, "{:.2f}");

                tabby::dropdown("Hitbox##Selector", m_state.m_hitboxState,
                                m_state.m_hitboxState.selectedIndex,
                                popupShaderFn);

                {
                    auto& h = m_state.m_hitboxCategories[m_state.m_hitboxState
                                                             .selectedIndex];

                    auto& category = SLSettings::get()->hitboxes.categories[h];

                    tabby::checkbox("Enabled##SpecificHitbox",
                                    category.enabled);

                    m_state.m_hitboxColorState.colors = category.colors;

                    tabby::color("Color##SpecificHitbox",
                                 m_state.m_hitboxColorState, popupShaderFn);

                    category.colors = m_state.m_hitboxColorState.colors;

                    tabby::drag("Fill Opacity##SpecificHitbox",
                                category.fillOpacity, 0.0, 1.0, 0.01, "{:.2f}");
                };

                tabby::divider();

                tabby::text("Layout", m_medium);

                if (tabby::checkbox("Enabled##LayoutMode",
                                    Bot::get()->updater().m_layoutMode->inner())
                        .pressed) {
                    Bot::get()->updater().m_layoutMode->notifyChange();
                }

                tabby::checkbox("Use Regular Background##LayoutMode",
                                Bot::get()->updater().m_useRegularBg->inner());

                tabby::color("Background Color##LayoutMode",
                             m_state.m_bgColorState, popupShaderFn);
                tabby::color("Ground Color##LayoutMode",
                             m_state.m_groundColorState, popupShaderFn);

                SLSettings::get()->layoutBgColor =
                    m_state.m_bgColorState.colors;
                SLSettings::get()->layoutGroundColor =
                    m_state.m_groundColorState.colors;

                tabby::divider();

                tabby::text("Noclip", m_medium);

                // god i hate working on this. Rust release me
                tabby::checkbox("Enabled##Noclip",
                                Bot::get()->updater().m_noclip->inner());

                tabby::dropdown("Player##Noclip", m_state.m_noclipState,
                                *reinterpret_cast<int*>(
                                    &Bot::get()->updater().m_noclipType),
                                popupShaderFn);

                tabby::divider();

                tabby::text("Trajectory", m_medium);

                tabby::checkbox(
                    "Enabled##Trajectory",
                    Bot::get()->trajectory().m_state.m_enabled->inner());
                tabby::drag("Width##Trajectory",
                            Bot::get()->trajectory().m_state.m_width->inner(),
                            0.0, 1.0, 0.01f, "{:.2f}");
                tabby::drag("Length##Trajectory",
                            Bot::get()->trajectory().m_state.m_length->inner(),
                            0.0, 5.0, 0.01f, "{:.2f}s");

                tabby::dropdown(
                    "Trajectory##Selector", m_state.m_trajectoryState,
                    m_state.m_trajectoryState.selectedIndex, popupShaderFn);
                {
                    auto& t = m_state.m_categories[m_state.m_trajectoryState
                                                       .selectedIndex];

                    auto& category =
                        SLSettings::get()->trajectory.categories[t];

                    tabby::checkbox("Enabled##SpecificTrajectory",
                                    category.enabled);

                    m_state.m_trajectoryColorState.colors = category.colors;

                    tabby::color("Color##SpecificTrajectory",
                                 m_state.m_trajectoryColorState, popupShaderFn);

                    category.colors = m_state.m_trajectoryColorState.colors;
                };

                tabby::divider();

                tabby::text("Backstepping", m_medium);

                tabby::fraction(
                    2.0,
                    [&]() {
                        tabby::checkbox(
                            "Backwards Stepping",
                            Bot::get()->updater().m_backwardsStepping->inner());

                        tabby::spacer(16.0);

                        if (tabby::drag("Steps",
                                        Bot::get()
                                            ->practiceFix()
                                            .m_maxStoredFrames->inner(),
                                        1u, 2400u)
                                .changed) {
                            Bot::get()
                                ->practiceFix()
                                .m_maxStoredFrames->notifyChange();
                        }
                    },
                    16.0);

                // tabby::checkbox("Run Full Updates",
                // Bot::get()->updater().m_fullGamePrediction->inner());

                tabby::divider();

                tabby::text("Autoclicker", m_medium);

                tabby::checkbox("Enabled##Autoclicker",
                                Bot::get()->autoclicker().m_enabled->inner());
                tabby::drag("Interval##Autoclicker",
                            Bot::get()->autoclicker().m_frequency, 0,
                            std::numeric_limits<int>::max(), 1.0f,
                            "{} Frame(s)");
                tabby::checkbox("Swift Clicks",
                                Bot::get()->autoclicker().m_performSwifts);
                tabby::dropdown("Player##Autoclicker",
                                m_state.m_autoclickerState,
                                *reinterpret_cast<int*>(
                                    &Bot::get()->autoclicker().m_player),
                                popupShaderFn);

                tabby::divider();

                tabby::text("Other", m_medium);

                tabby::checkbox("Mirror Inputs",
                                Bot::get()->replaySystem().m_mirrorInputs);
                tabby::checkbox("Mirror Inverted",
                                Bot::get()->replaySystem().m_mirrorInverted);

                tabby::checkbox("Maintain Gravity",
                                Bot::get()->replaySystem().m_maintainGravity);

                tabby::checkbox("No Mirror Portals",
                                Bot::get()->updater().m_noMirror->inner());
            });

            tabby::tab(m_state.m_currentTab, UIState::UITab::Prediction, [&]() {
                tabby::text("Prediction", m_bold);

                tabby::divider(false);

                tabby::text("Automation", m_medium);

                tabby::checkbox("Prevent Death",
                                Bot::get()->updater().m_preventDeath->inner());
                tabby::checkbox(
                    "Use Trajectory##PD",
                    Bot::get()->updater().m_fullGamePrediction->inner());
                // tabby::checkbox("Auto Flip On Death",
                // Bot::get()->updater().m_autoFlipOnDeath->inner());

                tabby::divider();

                tabby::text("Simulation", m_medium);

                if (tabby::button("Find Best Frame").pressed) {
                    Bot::get()->updater().findBestFrameCandidate();
                }

                tabby::drag(
                    "Threshold##Prediction",
                    Bot::get()->updater().m_acceptablePrediction->inner(), 0.0f,
                    1.0f, 0.01f, "{:.2f}");

                // tabby::divider();

                // tabby::text("Pathfinder", m_medium);

                // auto& pf = Bot::get()->pathfinder();

                // if (pf.isRunning()) {
                //     if (tabby::button("Stop##Pathfinder").pressed) {
                //         pf.stop();
                //     }

                //     tabby::checkbox("Preview##Pathfinder",
                //     pf.m_renderPreview);

                //     auto pl = PlayLayer::get();
                //     tabby::text(fmt::format("Progress: {} (Attempt {})",
                //     Bot::get()->updater().getFrame(), pl->m_attempts));
                // } else {
                //     if (tabby::button("Start##Pathfinder").pressed) {
                //         pf.start();
                //     }
                // }
            });

            tabby::tab(m_state.m_currentTab, UIState::UITab::Edit, [&]() {
                tabby::text("Edit", m_bold);

                tabby::divider(false);

                auto& replay = Bot::get()->replaySystem().m_actionAtom;
                auto& inputs = replay.m_actions;
                int inputIndex = Bot::get()->replaySystem().getInputIndex();

                if (inputs.empty()) {
                    tabby::text("No replay loaded.");
                }

                for (int i = 0; i < (int)inputs.size(); i++) {
                    auto& input = inputs[i];
                    if (i == inputIndex) {
                        if (m_state.m_editIndex != inputIndex) {
                            m_state.m_editIndex = inputIndex;
                            ImGui::SetScrollHereY();
                        }

                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              {1.0, 0.5, 0.5, 1.0});
                    }

                    if (tabby::drag(std::string("Frame##") + std::to_string(i),
                                    input.m_frame, 0ull,
                                    std::numeric_limits<uint64_t>::max())
                            .changed) {
                        Bot::get()->updater().m_tps->notifyChange();
                        input.recalculateDelta(i == 0 ? 0
                                                      : inputs[i - 1].m_frame);
                        if ((int)inputs.size() > i) {
                            inputs[i + 1].recalculateDelta(inputs[i].m_frame);
                        }
                    }

                    tabby::fraction(
                        2.0,
                        [&]() {
                            if (tabby::checkbox(std::string("Holding##") +
                                                    std::to_string(i),
                                                input.m_holding)
                                    .pressed) {
                                Bot::get()->updater().m_tps->notifyChange();
                            }

                            tabby::spacer(16.0);

                            if (tabby::checkbox(std::string("Player 2##") +
                                                    std::to_string(i),
                                                input.m_player2)
                                    .pressed) {
                                Bot::get()->updater().m_tps->notifyChange();
                            }

                            if (tabby::button(std::string("Add Below##") +
                                              std::to_string(i))
                                    .pressed) {
                                if (i + 1 == (int)inputs.size()) {
                                    inputs.push_back(slc::v3::Action(
                                        inputs[i].m_frame, 0,
                                        slc::v3::Action::ActionType::Jump,
                                        !inputs[i].m_holding,
                                        inputs[i].m_player2));
                                } else {
                                    inputs.insert(
                                        inputs.begin() + i + 1,
                                        slc::v3::Action(
                                            inputs[i].m_frame, 0,
                                            slc::v3::Action::ActionType::Jump,
                                            !inputs[i].m_holding,
                                            inputs[i].m_player2));
                                }
                            }

                            tabby::spacer(16.0);

                            if (tabby::button(std::string("Remove##") +
                                              std::to_string(i))
                                    .pressed) {
                                inputs.erase(inputs.begin() + i);
                                if ((int)inputs.size() > i) {
                                    inputs[i].recalculateDelta(
                                        i == 0 ? 0 : inputs[i - 1].m_frame);
                                }

                                if ((int)inputs.size() > i + 1) {
                                    inputs[i + 1].recalculateDelta(
                                        inputs[i].m_frame);
                                }
                            }
                        },
                        16.0);

                    tabby::divider();

                    if (i == inputIndex) {
                        ImGui::PopStyleColor();
                    }
                }
            });

            tabby::tab(m_state.m_currentTab, UIState::UITab::FrameWindow,
                       [&]() {
                auto& fw = bot->frameWindow();
                auto& cfg = fw.m_config;

                tabby::text("Frame Window Analyzer", m_bold);
                tabby::divider(false);
                tabby::text(
                    "NaN-compatible core: one press/release is shifted while "
                    "Silicate replays the real level. Sequence context never "
                    "replaces the canonical FW value.");

                tabby::divider();
                tabby::text("Analysis range", m_medium);
                tabby::fraction(
                    2.0,
                    [&]() {
                        tabby::drag("First input", cfg.firstInput, 1u,
                                    1000000u, 1.0f);
                        tabby::spacer(16.0);
                        tabby::drag("Last input (0 = all)", cfg.lastInput, 0u,
                                    1000000u, 1.0f);

                        tabby::drag("Search radius", cfg.searchRadius, 1u,
                                    240u, 1.0f, "{} ticks");
                        tabby::spacer(16.0);
                        tabby::drag("Stop after failures",
                                    cfg.stopAfterFailures, 1u, 32u, 1.0f);
                    },
                    16.0);

                tabby::divider();
                tabby::text("Context / spam", m_medium);
                tabby::checkbox("Sequence context", cfg.sequenceContext);
                if (cfg.sequenceContext) {
                    tabby::fraction(
                        2.0,
                        [&]() {
                            tabby::drag("Dense gap", cfg.denseGap, 1u, 120u,
                                        1.0f, "{} ticks");
                            tabby::spacer(16.0);
                            tabby::drag("Max sequence pairs",
                                        cfg.maxSequencePairs, 1u, 10000u,
                                        1.0f);
                        },
                        16.0);
                }

                tabby::divider();
                tabby::text("Runner", m_medium);
                tabby::fraction(
                    2.0,
                    [&]() {
                        tabby::drag("Analysis speed", cfg.analysisSpeed, 1.0,
                                    1000.0, 1.0f, "{:.1f}x");
                        tabby::spacer(16.0);
                        tabby::drag("Trial timeout",
                                    cfg.trialTimeoutSeconds, 1.0, 120.0, 1.0f,
                                    "{:.1f}s");
                    },
                    16.0);

                tabby::checkbox("NaNDL export uses frame numbers",
                                cfg.exportFrameNumbers);
                tabby::drag("NaNDL respawn time", cfg.respawnTime, 0.0,
                            120.0, 0.1f, "{:.1f}s");

                tabby::divider();
                tabby::text(fmt::format("Status: {}", fw.status()));
                if (fw.isActive()) {
                    tabby::text(fmt::format(
                        "Phase: {} | Input: {} | Trials: {}",
                        FrameWindowAnalyzer::phaseName(fw.phase()),
                        fw.currentInputNumber(), fw.completedTrials()));
                    if (tabby::button("Cancel analysis").pressed) {
                        fw.cancel();
                    }
                } else {
                    tabby::fraction(
                        2.0,
                        [&]() {
                            if (tabby::button("Analyze Frame Windows").pressed) {
                                fw.start();
                            }
                            tabby::same_line();
                            if (tabby::button("Clear results").pressed) {
                                fw.clearResults();
                            }
                        },
                        16.0);
                }

                tabby::divider();
                tabby::text("Export / diagnostics", m_medium);
                tabby::fraction(
                    2.0,
                    [&]() {
                        if (tabby::button("Export NaNDL JSON").pressed) {
                            fw.exportNaNDL();
                        }
                        tabby::spacer(16.0);
                        if (tabby::button("Export Debug JSON").pressed) {
                            fw.exportFullDiagnostics();
                        }

                        if (tabby::button("Import NaNDL JSON").pressed) {
                            fw.importNaNDL(fw.defaultNaNDLImportPath());
                        }
                        tabby::spacer(16.0);
                        if (tabby::button("Open diagnostics folder").pressed) {
                            geode::utils::file::openFolder(
                                fw.diagnosticsDir());
                        }
                    },
                    16.0);
                tabby::text(fmt::format(
                    "Import path: {}",
                    fw.defaultNaNDLImportPath().string()));

                if (!fw.results().empty()) {
                    tabby::divider();
                    tabby::text(fmt::format("Results ({})", fw.results().size()),
                                m_medium);
                    ImGui::BeginChild("##FrameWindowResults", ImVec2(0, 320),
                                      true);
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(fw.results().size()));
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart;
                             i < clipper.DisplayEnd; ++i) {
                            const auto& r = fw.results()[i];
                            const auto line = r.analyzed
                                ? fmt::format(
                                      "#{:04} | F {:>7} | {} | P{} | {} {} | FW {} "
                                      "[{:+},{:+}]{}{}{}",
                                      r.inputNumber, r.originalFrame, r.gameMode,
                                      r.player2 ? 2 : 1,
                                      FrameWindowAnalyzer::buttonName(r.button),
                                      r.holding ? "Press" : "Release", r.frameWindow,
                                      r.earliestOffset, r.latestOffset,
                                      r.coupled ? " | COUPLED" : "",
                                      r.discontinuous ? " | DISCONTINUOUS" : "",
                                      (r.earlyTruncated || r.lateTruncated)
                                          ? " | RADIUS LIMIT"
                                          : "")
                                : fmt::format(
                                      "#{:04} | F {:>7} | {} | P{} | {} {} | PENDING",
                                      r.inputNumber, r.originalFrame, r.gameMode,
                                      r.player2 ? 2 : 1,
                                      FrameWindowAnalyzer::buttonName(r.button),
                                      r.holding ? "Press" : "Release");
                            ImGui::TextUnformatted(line.c_str());
                        }
                    }
                    clipper.End();
                    ImGui::EndChild();
                }
            });

            tabby::tab(m_state.m_currentTab, UIState::UITab::Render, [&]() {
                tabby::text("Render", m_bold);

                tabby::divider(false);

                auto renderer = Renderer::get();

                if (!renderer->isFFmpegLoaded()) {
                    tabby::text("FFmpeg not loaded.");

                    if (m_ffmpegDownloadProgress < 0.0 &&
                        tabby::button("Download").pressed) {
                        m_ffmpegDownloadProgress = 0.0;
                        geode::log::info("Downloading FFmpeg...");
                        auto req = web::WebRequest();

                        req.onProgress([&](const web::WebProgress& prog) {
                            if (prog.downloadTotal() == 0) {
                                return;
                            }

                            m_ffmpegDownloadProgress =
                                static_cast<double>(prog.downloaded()) /
                                static_cast<double>(prog.downloadTotal());
                        });

                        m_webListener.spawn(
                            req.get(ffmpegUrl), [&](web::WebResponse resp) {
                                const auto data = resp.data();

                                geode::log::info("Verifying checksum...");
                                uint64_t hash = fnv1aHash(data);
                                constexpr uint64_t EXPECTED =
                                    0x44618c661fa11607ull;
                                if (hash != EXPECTED) {
                                    geode::log::error(
                                        "Invalid checksum! Aborting FFmpeg "
                                        "loader");
                                    m_ffmpegDownloadProgress = -1.0;
                                    return;
                                }

                                geode::log::info(
                                    "Checksum valid! Unzipping...");
                                auto unzipResult =
                                    geode::utils::file::Unzip::create(data);
                                if (unzipResult.isErr()) {
                                    return;
                                }

                                auto unzip = std::move(unzipResult.unwrap());
                                auto ffmpegDir =
                                    Mod::get()->getTempDir() / "ffmpeg";
                                if (unzip.extractAllTo(ffmpegDir).isErr()) {
                                    return;
                                }

                                namespace fs = std::filesystem;

                                auto libDir = Mod::get()->getPersistentDir() /
                                              "libraries";
                                geode::log::info(
                                    "Copying dlls from temp dir `{}`...",
                                    ffmpegDir);
                                for (const auto& entry :
                                     fs::directory_iterator(ffmpegDir)) {
                                    if (entry.path().extension() == ".dll") {
                                        fs::copy_file(
                                            entry,
                                            libDir / entry.path().filename(),
                                            fs::copy_options::
                                                overwrite_existing);
                                    }
                                }

                                fs::remove_all(ffmpegDir);

                                Renderer::get()->loadFFmpeg();
                                m_ffmpegDownloadProgress = -1.0;
                            });
                    }

                    if (m_ffmpegDownloadProgress >= 0.95) {
                        tabby::text(
                            "Loading FFmpeg! Please do not close your game...");
                    } else if (m_ffmpegDownloadProgress >= 0.0) {
                        tabby::text(
                            fmt::format("Downloading FFmpeg... {:.1f}%",
                                        m_ffmpegDownloadProgress * 100.0));
                    }

                    return;
                }

                if (renderer->m_autoVideoName->inner()) {
                    tabby::input_text(
                        "Video Template", "Template",
                        Renderer::get()->m_videoNameTemplate->inner());
                } else {
                    tabby::input_text("Video Name", "Video",
                                      Renderer::get()->m_settings.m_outputPath);
                }

                tabby::checkbox("Auto Video Name",
                                renderer->m_autoVideoName->inner());

                if (renderer->isRecording()) {
                    if (tabby::button("Stop").pressed) {
                        renderer->signalStop();
                    }
                } else {
                    if (!PlayLayer::get() && !renderer->m_shouldStart) {
                        if (tabby::button("Start").pressed) {
                            renderer->queueStart();
                        }
                    } else if (renderer->m_shouldStart) {
                        tabby::text("Waiting to enter level...");
                    } else if (PlayLayer::get()) {
                        if (tabby::button("Start Here").pressed) {
                            (void)renderer->start();
                        }
                    }
                }

                auto ar = AudioRecorder::get();
                tabby::checkbox("Audio Preview", ar->m_audioPreview->inner());
                tabby::checkbox("Auto Exit on Finish",
                                renderer->m_autoExitLevel->inner());

                tabby::divider();

                tabby::text("Presets", m_medium);

                tabby::input_text_autocomplete(
                    "Preset Name", "Preset", m_state.m_presetName,
                    m_state.m_presetAutocomplete, popupShaderFn);

                tabby::fraction(2.0, [&]() {
                    if (tabby::button("Load##Preset").pressed) {
                        auto path = Mod::get()->getPersistentDir() / "presets" /
                                    (m_state.m_presetName + ".json");
                        if (std::filesystem::exists(path)) {
                            renderer->loadSettings(path);
                        } else {
                            geode::log::error("Preset file does not exist: {}",
                                              path.string());
                        }
                    }

                    tabby::same_line();

                    if (tabby::button("Save##Preset").pressed) {
                        auto path = Mod::get()->getPersistentDir() / "presets" /
                                    (m_state.m_presetName + ".json");
                        renderer->saveSettings(path);
                    }
                });

                tabby::divider();

                tabby::text("Video Settings", m_medium);

                tabby::fraction(
                    2.0,
                    [&]() {
                        tabby::drag("Width", renderer->m_settings.m_width, 1,
                                    10000, 1.0f);
                        tabby::spacer(16.0);
                        tabby::drag("Height", renderer->m_settings.m_height, 1,
                                    10000, 1.0f);

                        tabby::drag("FPS", renderer->m_settings.m_fps, 1, 1000,
                                    1.0f);
                        tabby::spacer(16.0);
                        if (tabby::drag("Bitrate", m_state.m_bitrate, 0.0,
                                        10000.0, 1.0f, "{:g}Mbps")
                                .changed) {
                            renderer->m_settings.m_bitrate =
                                std::round(m_state.m_bitrate * 1'000'000.0);
                        }
                    },
                    16.0);

                tabby::input_text("Codec", "Codec",
                                  renderer->m_settings.m_codec);
                tabby::input_text("Extension", "Extension",
                                  renderer->m_settings.m_extension);
                if (tabby::drag("After End Time",
                            renderer->m_settings.m_afterEndTime, 0.0f, 10000.0f,
                            1.0f, "{:.2f}s").changed) {
                                renderer->m_settings.m_fadeOutTime = std::min(
                                renderer->m_settings.m_fadeOutTime, (double)renderer->m_settings.m_afterEndTime);
                            }
                tabby::checkbox("SSB Fix",
                            Bot::get()->updater().m_ssbFix->inner());

                tabby::drag("Music Volume", renderer->m_settings.m_musicVolume,
                            0.0, 1.0, 0.01, "{:.2f}");
                tabby::drag("SFX Volume", renderer->m_settings.m_sfxVolume, 0.0,
                            1.0, 0.01, "{:.2f}");

                tabby::drag("Fade In Time", renderer->m_settings.m_fadeInTime,
                            0.0, 60.0, 0.1, "{:.1f}s");
                if (tabby::drag("Fade Out Time", renderer->m_settings.m_fadeOutTime,
                            0.0, 60.0, 0.1, "{:.1f}s").changed) {
                                renderer->m_settings.m_afterEndTime = std::max(
                                (float)renderer->m_settings.m_fadeOutTime, renderer->m_settings.m_afterEndTime);
                            }
                            
                if (m_state.m_showExperimentalFeatures) {
                    tabby::drag("Visual FPS", renderer->m_settings.m_visualFps, 1, 100000,
                                1.0f);
                }
                tabby::checkbox("Record 1st Attempt Pause",
                                renderer->m_settings.m_firstAttemptPause);

                tabby::checkbox(
                    "Show Labels While Rendering",
                    bot->updater().m_showLabelsWhileRendering->inner());

                tabby::input_text("FFmpeg Args", "-preset slow ...",
                                  renderer->m_settings.m_renderArgs);
            });

            // tabby::tab(m_state.m_currentTab, UIState::UITab::Scripts, [&]() {
            //     tabby::text("Scripts", m_bold);

            //     tabby::divider(false);

            //     tabby::text("Script Manager", m_medium);

            //     tabby::input_text_autocomplete(
            //         "Script Name", "Script", m_state.m_scriptName,
            //         m_state.m_scriptAutocomplete, [&]() {
            //             renderBlurBg(12.0f, 1.5f,
            //             m_state.m_useShader->inner(),
            //                          m_state.m_opacity->inner());
            //         });

            //     if (tabby::button("Load").pressed) {
            //         auto path = Mod::get()->getPersistentDir() / "scripts" /
            //                     (m_state.m_scriptName + ".lua");
            //         if (std::filesystem::exists(path)) {
            //             Bot::get()->scripts().loadScript(path);
            //         } else {
            //             geode::log::error("Script file does not exist: {}",
            //                               path.string());
            //         }
            //     }

            //     tabby::divider();

            //     auto& scripts = Bot::get()->scripts();
            //     for (int i = 0; i < scripts.m_scripts.size(); i++) {
            //         auto& script = scripts.m_scripts[i];
            //         tabby::text(script.m_name);
            //         tabby::checkbox("Enabled##Script" + std::to_string(i),
            //                         script.m_enabled);

            //         tabby::divider();
            //     }
            // });

            tabby::tab(m_state.m_currentTab, UIState::UITab::Keybinds, [&]() {
                auto* b = SLBindingManager::get();

                auto& kbs = b->getKeybinds();

                tabby::text("Keybinds", m_bold);

                tabby::divider(false);

                m_state.m_keybindEditGeneration++;

                int row = 0;
                for (auto& kb : kbs | std::views::values | std::views::join) {
                    if (!kb) continue;

                    int id = row++;

                    auto& edit = m_state.m_keybindEdits[kb.get()];
                    edit.seen = m_state.m_keybindEditGeneration;

                    // don't fight the user while they're typing
                    if (m_state.m_editedKeybindTag != kb.get()) {
                        edit.tag = kb->getTag();
                    }

                    // tints the field; the text as of last frame, since the
                    // edit happens while the widget draws
                    bool tagValid = b->hasValue(edit.tag);
                    tabby::WidgetState tagState;
                    bool tagCommitted = false;

                    if (!tagValid) {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              ImVec4(1.0, 0.5, 0.5, 1.0));
                    }

                    tagState = tabby::input_text(
                        fmt::format("Tag##tagkb{}", id), "Tag", edit.tag);

                    // has to be asked before anything else is drawn
                    tagCommitted = ImGui::IsItemDeactivatedAfterEdit();

                    if (!tagValid) {
                        ImGui::PopStyleColor();
                    }

                    tabby::fraction(
                        2.0,
                        [&]() {
                            tabby::text("Bind");

                            tabby::same_line();

                            ImGui::SetCursorPosX(
                                tabby::TabbyGlobalCfg::get().widgetWidth +
                                16.0);

                            bool isEditing = b->isEditing(kb.get());
                            std::string buttonText =
                                fmt::format("{}##kb{}",
                                            isEditing ? "Press a key..."
                                                      : kb->getKeyLabel(),
                                            id);

                            if (tabby::button(buttonText).pressed) {
                                if (isEditing) {
                                    b->stopEdit();
                                } else {
                                    b->startEdit(kb.get());
                                }
                            }
                        },
                        16.0);

                    if (tagState.held) {
                        m_state.m_editedKeybindTag = kb.get();
                    } else if (m_state.m_editedKeybindTag == kb.get()) {
                        m_state.m_editedKeybindTag = nullptr;
                    }

                    if (tagCommitted && b->hasValue(edit.tag)) {
                        m_state.m_pendingRetag = kb.get();
                        m_state.m_pendingRetagTag = edit.tag;
                    }

                    // don't fight the user while they're typing
                    if (m_state.m_editedKeybindValue != kb.get()) {
                        edit.value = kb->toString();
                    }

                    bool valid = kb->canParse(edit.value);
                    if (!valid) {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              ImVec4(1.0, 0.5, 0.5, 1.0));
                    }

                    auto valueState =
                        tabby::input_text(fmt::format("Value##valuekb{}", id),
                                          "Value", edit.value);

                    if (!valid) {
                        ImGui::PopStyleColor();
                    }

                    if (valueState.held) {
                        m_state.m_editedKeybindValue = kb.get();
                    } else if (m_state.m_editedKeybindValue == kb.get()) {
                        m_state.m_editedKeybindValue = nullptr;
                    }

                    if (valueState.changed) {
                        kb->fromString(edit.value);
                    }

                    tabby::divider();
                }

                if (m_state.m_pendingRetag != nullptr) {
                    if (b->retagKeybind(m_state.m_pendingRetag,
                                        m_state.m_pendingRetagTag)) {
                        m_state.m_keybindEdits.erase(m_state.m_pendingRetag);

                        if (m_state.m_editedKeybindTag ==
                            m_state.m_pendingRetag) {
                            m_state.m_editedKeybindTag = nullptr;
                        }

                        if (m_state.m_editedKeybindValue ==
                            m_state.m_pendingRetag) {
                            m_state.m_editedKeybindValue = nullptr;
                        }
                    }

                    m_state.m_pendingRetag = nullptr;
                    m_state.m_pendingRetagTag.clear();
                }

                std::erase_if(m_state.m_keybindEdits, [this](const auto& item) {
                    if (item.second.seen == m_state.m_keybindEditGeneration) {
                        return false;
                    }

                    if (m_state.m_editedKeybindTag == item.first) {
                        m_state.m_editedKeybindTag = nullptr;
                    }

                    if (m_state.m_editedKeybindValue == item.first) {
                        m_state.m_editedKeybindValue = nullptr;
                    }

                    return true;
                });

                if (tabby::button("Add Keybind").pressed) {
                    RawKeybind k{
                        .m_key = 0,
                        .m_modifiers = 0,
                        .m_valueTag = "updater.frame_advance",
                        .m_type = KeybindType::Toggle,
                        .m_active = true,
                        .m_value = "1",
                    };

                    if (auto kb = b->makeKeybind(k)) {
                        b->registerKeybind(kb);
                    }
                }
            });

            tabby::tab(m_state.m_currentTab, UIState::UITab::Settings, [&]() {
                tabby::text("Settings", m_bold);

                tabby::divider(false);

                tabby::text("Interface", m_medium);

                if (tabby::button("Open Silicate Folder").pressed) {
                    geode::utils::file::openFolder(
                        Mod::get()->getPersistentDir());
                }

                tabby::checkbox("Use Glass Shader",
                                m_state.m_useShader->inner());

                if (tabby::dropdown("Theme", m_state.m_themeState,
                                    m_state.m_themeState.selectedIndex,
                                    popupShaderFn)
                        .changed) {
                    SLSettings::get()->theme =
                        m_state.m_themeState.selectedIndex;
                    m_theme = &s_themes[m_state.m_themeState.selectedIndex];
                    m_theme->apply();
                }

                if (tabby::checkbox("Play Animations",
                                    m_state.m_playAnimations->inner())
                        .pressed) {
                    m_state.m_playAnimations->notifyChange();
                }

                if (tabby::drag("Animation Speed",
                                m_state.m_animationSpeed->inner(), 0.1f, 3.0f,
                                0.01f, "{:.2f}")
                        .changed) {
                    m_state.m_animationSpeed->notifyChange();
                }

                if (tabby::drag("UI Scale", m_state.m_uiScale->inner(), 0.1f,
                                10.0f, 0.01f, "{:.2f}x")
                        .changed) {
                    m_state.m_uiScale->notifyChange();
                }

                // if (m_state.m_restartGameInfo) {
                //     ImGui::PushStyleColor(ImGuiCol_Text,
                //                           ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                //     tabby::text(
                //         "It's recommended to restart the game after "
                //         "changing "
                //         "UI scale.");
                //     ImGui::PopStyleColor();
                // }

                if (tabby::drag("UI Opacity", m_state.m_opacity->inner(), 0.1f,
                                1.0f, 0.01f, "{:.2f}")
                        .changed) {
                    m_state.m_opacity->notifyChange();
                }

                tabby::divider();

                tabby::text("Backups", m_medium);

                tabby::checkbox(
                    "Auto-Save At Level End",
                    Bot::get()->replaySystem().m_autosaveAtLevelEnd->inner());

                auto& rs = Bot::get()->replaySystem();
                if (tabby::checkbox("Auto-Backup",
                                    Bot::get()
                                        ->replaySystem()
                                        .m_autosaveAtInterval->inner())
                        .pressed) {
                    rs.m_autosaveAtInterval->notifyChange();
                }

                if (rs.m_autosaveAtInterval->inner()) {
                    if (tabby::drag("Auto-Backup Interval",
                                    rs.m_autosaveInterval->inner(), 1.0, 3600.0,
                                    1.0, "{:.0f}s")
                            .changed) {
                        rs.m_autosaveInterval->notifyChange();
                    }
                }

                tabby::divider();

                tabby::text("Labels", m_medium);

                tabby::checkbox("Display Labels",
                                Bot::get()->labels().m_globalEnabled);

                tabby::dropdown("Label##Selector", m_state.m_labelState,
                                m_state.m_labelState.selectedIndex,
                                popupShaderFn);

                auto& label = Bot::get()
                                  ->labels()
                                  .m_labels[m_state.m_labelState.selectedIndex]
                                  .m_config;

                if (tabby::checkbox("Enabled##Label", label.m_enabled)
                        .pressed) {
                    bot->labels().m_requiresRefresh = true;
                }

                if (tabby::drag("Opacity##Label", label.m_opacity, 0.0f, 1.0f,
                                0.01f, "{:.2f}x")
                        .changed) {
                    bot->labels().m_requiresRefresh = true;
                }

                if (tabby::drag("Size##Label", label.m_scale, 0.0f, 100.0f,
                                1.0f, "{:.2f}x")
                        .changed) {
                    bot->labels().m_requiresRefresh = true;
                }

                if (tabby::dropdown("Font##Label", m_state.m_labelFontsState,
                                    *reinterpret_cast<int*>(&label.m_font),
                                    popupShaderFn)
                        .changed) {
                    bot->labels().m_requiresRefresh = true;
                }

                if (tabby::dropdown(
                        "Position##Label", m_state.m_labelPositionsState,
                        *reinterpret_cast<int*>(&label.m_anchor), popupShaderFn)
                        .changed) {
                    bot->labels().m_requiresRefresh = true;
                }

                tabby::divider();

                tabby::text("Playback", m_medium);

                tabby::checkbox("Block Inputs During Playback",
                                bot->replaySystem().m_ignoreInputs->inner());
                tabby::checkbox("Audio Speedhack",
                                bot->updater().m_speedhackAudio->inner());

                tabby::divider();

                tabby::text("Updater", m_medium);

                tabby::checkbox("Lock Delta",
                                bot->updater().m_lockDelta->inner());

                tabby::dropdown("Lock Delta Mode", m_state.m_lockDeltaState,
                                bot->updater().m_lockDeltaMode->inner(),
                                popupShaderFn);

                tabby::checkbox("Real Time",
                                bot->updater().m_realTime->inner());

                if (!bot->updater().m_realTime->inner()) {
                    tabby::checkbox("Dynamic UPR",
                                    bot->updater().m_dynamicUpr->inner());

                    if (bot->updater().m_dynamicUpr->inner()) {
                        tabby::drag("Target FPS",
                                    bot->updater().m_fpsTarget->inner(), 1.0,
                                    480.0, 1.0f, "{:.0f} FPS");
                    } else {
                        tabby::drag("Max UPR", bot->updater().m_maxUPR->inner(),
                                    1u, 1000000u, 1.0f);

                        tabby::checkbox(
                            "Use Visual Updates",
                            bot->updater().m_useVisualUpdates->inner());
                    }
                }

                tabby::divider();

                tabby::text("Miscellaneous", m_medium);

                tabby::checkbox(
                    "Use Alternate Input Hook",
                    bot->replaySystem().m_useAlternateHook->inner());

                if (tabby::button("Disable Bot").pressed) {
                    Bot::get()->m_enabled->inner() = false;
                    Bot::get()->m_enabled->notifyChange();
                    FMOD::ChannelGroup* master;
                    FMODAudioEngine::get()->m_system->getMasterChannelGroup(&master);
                    master->setPitch(1.0);
                }

                tabby::checkbox("Experimental Features",
                                m_state.m_showExperimentalFeatures);
            });
        });

        // window border
        ImGui::GetWindowDrawList()->AddRect(
            ImGui::GetWindowPos(),
            ImGui::GetWindowPos() + ImGui::GetWindowSize(),
            ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 0.1f)),
            24.0f * tabby::TabbyGlobalCfg::get().uiScale,
            ImDrawFlags_RoundCornersAll,
            2.5 * tabby::TabbyGlobalCfg::get().uiScale);
    });

#ifdef SILICATE_PROTECT
    VMProtectEnd();
#endif
}
