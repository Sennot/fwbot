#pragma once

#include <cocos2d.h>

#include <memory>

#include "colorspace/colorspace.hpp"
#include "pass.hpp"

class RenderTexture {
   public:
    static constexpr int RING_SIZE = 8;

    void init(std::unique_ptr<Colorspace> colorspace);
    void destroy();

    void issue(float fadeThreshold);
    bool tryHarvest(uint8_t** outData, bool block);

    int64_t issuedCount() const { return m_issued; }
    int64_t mappedCount() const { return m_mapped; }

    void displayPreview();

   public:
    std::unique_ptr<Colorspace> m_colorspace;
    std::vector<RenderPass> m_passes;

    uint32_t m_alignedWidth, m_alignedHeight;
    // real window size, only used to fit the preview blit into it
    uint32_t m_windowWidth, m_windowHeight;

    uint32_t m_tex[2];
    uint32_t m_quadVAO, m_quadVBO;

    int m_old_fbo, m_old_rbo;
    uint32_t m_fbo[2];

    uint32_t m_pbo[RING_SIZE];
    GLsync m_fence[RING_SIZE] = {};
    uint8_t* m_slotData[RING_SIZE] =
        {};  // mapped CPU pointer, null if unmapped
    bool m_slotMapped[RING_SIZE] = {};
    size_t m_bufferSize = 0;

    int64_t m_issued = 0;
    int64_t m_mapped = 0;

    uint32_t m_program;
};
