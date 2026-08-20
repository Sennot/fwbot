#include "theme.hpp"

#include <Geode/Geode.hpp>

#include "bot/bot.hpp"
#include "hook.hpp"
#include "manager.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

const char* THEME_BASE_VERT = R"(#version 130
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texCoord;

out vec2 v_texCoord;

void main() {
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";

void Theme::initialize() {
    int width, height, channels;
    uint8_t* data = stbi_load(
        (geode::Mod::get()->getResourcesDir() / m_iconPath).string().c_str(),
        &width, &height, &channels, 0);
    if (!data) {
        return;
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (channels == 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);
    }
    stbi_image_free(data);

    m_postprocessPass =
        RenderPass{.m_width = (unsigned int)ImGuiHookCtx::get().m_width,
                   .m_height = (unsigned int)ImGuiHookCtx::get().m_height,
                   .m_vertexShader = THEME_BASE_VERT,
                   .m_fragmentShader = m_postprocessShader.c_str(),
                   .m_readPixels = [](float, float) {}};
    m_postprocessPass.initialize();
}

void Theme::resize(uint32_t width, uint32_t height) {
    m_postprocessPass.m_width = width;
    m_postprocessPass.m_height = height;
    m_postprocessPass.resize();
}

void Theme::apply() { Bot::get()->ui().m_texture = m_texture; }
