#include <Geode/Geode.hpp>
#include <chrono>

struct ScopedTimer {
    const char* m_name;
    std::chrono::high_resolution_clock::time_point m_start;

    ScopedTimer(const char* name)
        : m_name(name), m_start(std::chrono::high_resolution_clock::now()) {}
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto ms =
            std::chrono::duration<double, std::milli>(end - m_start).count();

        if (ms > 0.005) {
            geode::log::info("[PROF] {}: {}ms", m_name, ms);
        }
    }
};

#define SCOPED_TIMER(name) \
    {                      \
    }