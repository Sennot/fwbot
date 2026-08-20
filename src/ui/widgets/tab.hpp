// #include "imgui.h"
#include <functional>
#include <string>

class SLUITab {
    std::string m_name;
    std::function<void()> m_bodyFn;

    SLUITab(std::string name) : m_name(name) {}

    SLUITab& body(std::function<void()> body) {
        m_bodyFn = body;
        return *this;
    }

    void draw() { m_bodyFn(); }
};
