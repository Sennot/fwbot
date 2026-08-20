#include <Geode/Geode.hpp>

#include "render/renderer.hpp"

using namespace geode::prelude;

#include <Geode/modify/CCEGLView.hpp>

struct SLCCEGLView : Modify<SLCCEGLView, CCEGLView> {
    void setFrameSize(float width, float height) {
        CCEGLView::setFrameSize(width, height);
        Renderer::get()->reacquireView();
    }
};
