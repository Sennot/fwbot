#ifndef BOT_HPP
#define BOT_HPP

#include <array>
#include <memory>

#include "settings/settings.hpp"
#include "shared/value/value.hpp"

// #define SL_DEV_MODE

#ifdef SL_DEV_MODE
#define SL_LOG_DEV(...)                \
    {                                  \
        geode::log::info(__VA_ARGS__); \
    }
#else
#define SL_LOG_DEV(...) \
    {                   \
    }
#endif

class BotUpdater;
class BotScheduler;
class UIManager;
class ReplaySystem;
class PracticeFix;
class TrajectoryManager;
class Autoclicker;
class Hitboxes;
class LabelManager;
class CPSCounter;
class FrameWindowAnalyzer;

class Bot {
   public:
    enum Mode {
        Recording,
        Playing,
    };

   public:
    static Bot* get() {
        static Bot instance;

        return &instance;
    }

    Bot(Bot const&) = delete;
    void operator=(Bot const&) = delete;

    void initialize();
    inline bool initialized() { return m_hasInitialized; };

    bool isEnabled();

    bool isRecording() { return m_mode == Recording; }
    bool isPlaying() { return m_mode == Playing; }
    void setMode(Mode mode) { m_mode = mode; }

    BotUpdater& updater();
    BotScheduler& scheduler();
    UIManager& ui();
    ReplaySystem& replaySystem();
    PracticeFix& practiceFix();
    TrajectoryManager& trajectory();
    Autoclicker& autoclicker();
    Hitboxes& hitboxes();
    LabelManager& labels();
    CPSCounter& cps();
    FrameWindowAnalyzer& frameWindow();

    Mode m_mode = Recording;
    SLValuePtr<bool> m_enabled = SLValue<bool>::create(
        "_________________bot.enabled", &SLSettings::get()->botEnabled);

   private:
    bool m_hasInitialized = false;

    Bot();
    ~Bot();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif  // BOT_HPP
