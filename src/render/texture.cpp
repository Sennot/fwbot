#include "texture.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>

// #include "colorspace/nv12.hpp"
#include "colorspace/yuv420p.hpp"
// #include "colorspace/rgb0.hpp"
// #include "colorspace/rgb24.hpp"

#ifdef SILICATE_PROTECT
#include "VMProtect/VMProtectSDK.h"
#endif

using namespace cocos2d;

void RenderTexture::init(std::unique_ptr<Colorspace> colorspace) {
#ifdef SILICATE_PROTECT
    VMProtectBegin("RenderTexture::init");
#endif

    m_colorspace = std::move(colorspace);
    m_colorspace->m_alignedWidth = m_alignedWidth;
    m_colorspace->m_alignedHeight = m_alignedHeight;

    m_passes = this->m_colorspace->getPasses();

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_old_fbo);

    for (auto& pass : m_passes) {
        pass.initialize();
    }

    m_bufferSize = m_colorspace->getBufferSize();

    constexpr size_t kEncoderReadPadding = 2 * AV_INPUT_BUFFER_PADDING_SIZE;

    glGenBuffers(RING_SIZE, m_pbo);
    for (int i = 0; i < RING_SIZE; ++i) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, m_bufferSize + kEncoderReadPadding,
                     nullptr, GL_STREAM_READ);
        m_slotMapped[i] = false;
        m_slotData[i] = nullptr;
        m_fence[i] = nullptr;
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    m_issued = 0;
    m_mapped = 0;

    float vertices[] = {
        // Pos      // UVs
        -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom-left
        1.0f,  -1.0f, 1.0f, 0.0f,  // Bottom-right
        1.0f,  1.0f,  1.0f, 1.0f,  // Top-right
        -1.0f, 1.0f,  0.0f, 1.0f   // Top-left
    };

    glGenVertexArrays(1, &m_quadVAO);
    glBindVertexArray(m_quadVAO);

    glGenBuffers(1, &m_quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_old_fbo);

#ifdef SILICATE_PROTECT
    VMProtectEnd();
#endif
}

void RenderTexture::issue(float fadeThreshold) {
    const int w = m_issued % RING_SIZE;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[w]);

    if (m_slotMapped[w]) {
        // dont let the cia take over
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        m_slotMapped[w] = false;
        m_slotData[w] = nullptr;
    }
    if (m_fence[w]) {
        glDeleteSync(m_fence[w]);
        m_fence[w] = nullptr;
    }

    for (size_t i = 0; i < m_passes.size(); ++i) {
        auto& pass = m_passes[i];
        glBindFramebuffer(GL_FRAMEBUFFER, pass.m_fbo);
        glViewport(0, 0, pass.m_width, pass.m_height);

        if (i == 0) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            CCDirector::get()->m_pRunningScene->visit();
        } else {
            glDisable(GL_BLEND);
        }

        if (pass.m_vertexShader && pass.m_fragmentShader) {
            glUseProgram(pass.m_program);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_passes[pass.m_sourceTex].m_tex);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, pass.m_tex);

            glUniform1i(pass.m_uTextureLoc, 1);
            glUniform2f(pass.m_uTexelSizeLoc, 1.0f / pass.m_width,
                        1.0f / pass.m_height);
            if (pass.m_uFadeLoc != -1) {
                glUniform1f(pass.m_uFadeLoc, fadeThreshold);
            }

            glBindVertexArray(m_quadVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
        }

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        pass.m_readPixels(0, 0);
    }

    m_fence[w] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    m_issued++;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_old_fbo);

    glUseProgram(0);
    glEnable(GL_BLEND);
}

bool RenderTexture::tryHarvest(uint8_t** outData, bool block) {
    if (m_mapped >= m_issued) {
        return false;  // nothing outstanding
    }

    const int p = m_mapped % RING_SIZE;
    GLsync fence = m_fence[p];
    if (!fence) {
        return false;
    }

    const GLbitfield flags = block ? GL_SYNC_FLUSH_COMMANDS_BIT : 0;
    const GLuint64 timeout = block ? 1'000'000'000ull : 0;  // 1s cap
    GLenum status = glClientWaitSync(fence, flags, timeout);
    if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED) {
        return false;  // readback still in flight
    }

    glDeleteSync(fence);
    m_fence[p] = nullptr;

    // the fence is signaled, so the data is already in the PBO - this map won't
    // block the GL thread
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[p]);
    auto* pixelData =
        static_cast<uint8_t*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    if (!pixelData) {
        geode::log::error("Failed to map buffer: {}", glGetError());
        m_mapped++;
        return false;
    }

    m_slotData[p] = pixelData;
    m_slotMapped[p] = true;
    *outData = pixelData;
    m_mapped++;
    return true;
}

void RenderTexture::displayPreview() {
    int blend;
    glGetIntegerv(GL_BLEND, &blend);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_passes[0].m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_old_fbo);

    const float scale =
        std::min(static_cast<float>(m_windowWidth) / m_alignedWidth,
                 static_cast<float>(m_windowHeight) / m_alignedHeight);
    const int width = static_cast<int>(m_alignedWidth * scale);
    const int height = static_cast<int>(m_alignedHeight * scale);
    const int x = (static_cast<int>(m_windowWidth) - width) / 2;
    const int y = (static_cast<int>(m_windowHeight) - height) / 2;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, m_alignedWidth, m_alignedHeight, x, y, x + width,
                      y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    if (blend) {
        glEnable(GL_BLEND);
    }

    if (scissor) {
        glEnable(GL_SCISSOR_TEST);
    }
}

void RenderTexture::destroy() {
    for (auto& pass : m_passes) {
        pass.destroy();
    }

    for (int i = 0; i < RING_SIZE; ++i) {
        if (m_slotMapped[i]) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[i]);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            m_slotMapped[i] = false;
            m_slotData[i] = nullptr;
        }
        if (m_fence[i]) {
            glDeleteSync(m_fence[i]);
            m_fence[i] = nullptr;
        }
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    glDeleteTextures(2, m_tex);
    glDeleteFramebuffers(2, m_fbo);
    glDeleteBuffers(RING_SIZE, m_pbo);
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
    glDeleteProgram(m_program);

    m_issued = 0;
    m_mapped = 0;

    glBindFramebuffer(GL_FRAMEBUFFER, m_old_fbo);
}
