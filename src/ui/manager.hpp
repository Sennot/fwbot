#ifndef UI_MANAGER_HPP
#define UI_MANAGER_HPP

#define IMGUI_DEFINE_MATH_OPERATORS

#include <tabby.hpp>

#include "state.hpp"
#include "theme.hpp"

class UIManager {
   public:
    UIState m_state;

    // ImFont* m_font;
    // ImFont* m_bold;

    tabby::Font m_font;
    tabby::Font m_medium;
    tabby::Font m_bold;
    Theme* m_theme;
    tabby::AutocompleteState m_replayAutocomplete;

    geode::async::TaskHolder<geode::utils::web::WebResponse> m_webListener;
    double m_ffmpegDownloadProgress = -1.0;

    GLuint m_texture = 0;

    UIManager();

    void setup();
    void reinitThemeGraphics();
    void draw();
    void toggle();
};

#endif  // UI_MANAGER_HPP
