#include <string>
#include <vector>

#include "imgui.h"
#include "tab.hpp"

class SLUIWindow {
    std::string m_name;
    bool m_visible = false;

    std::vector<SLUITab> m_tabs;
    size_t m_currentTab = 0;

    SLUIWindow(std::string name) : m_name(name) {}
};