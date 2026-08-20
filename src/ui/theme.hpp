#pragma once

#include <string>

#include "render/pass.hpp"

class Theme {
   private:
    std::string m_iconPath;
    std::string m_postprocessShader;

   public:
    RenderPass m_postprocessPass;
    uint32_t m_texture = 0;
    double m_opacityExp;

    std::string m_name;

    void initialize();
    void apply();
    void resize(uint32_t width, uint32_t height);

    Theme(std::string name, std::string iconPath, std::string shader,
          double opacityExp)
        : m_iconPath(iconPath),
          m_postprocessShader(shader),
          m_opacityExp(opacityExp),
          m_name(name) {}
};
