#include "autoclicker.hpp"

#include "bot/bot.hpp"
#include "bot/updater.hpp"

void Autoclicker::update(PlayLayer* pl) {
    if (!pl) {
        m_p1Clicked = false;
        m_p2Clicked = false;
        return;
    }

    if (!m_enabled->inner()) {
        if (m_p1Clicked) {
            pl->queueButton(1, false, false, 0.0);
            m_p1Clicked = false;
        }
        if (m_p2Clicked) {
            pl->queueButton(1, false, true, 0.0);
            m_p2Clicked = false;
        }
        return;
    }

    auto bot = Bot::get();
    auto frame = bot->updater().getFrame();

    if (frame == m_lastFrame) {
        return;
    }

    if (!bot->isRecording()) {
        return;
    }

    if ((frame >= m_lastFrame + m_frequency) || m_lastFrame == UINT64_MAX) {
        m_lastFrame = frame;
        if (performPlayer1()) {
            if (!m_p1Clicked) {
                pl->queueButton(1, true, false, 0.0);
                m_p1Clicked = true;
            } else if (m_p1Clicked) {
                m_p1Clicked = false;
                pl->queueButton(1, false, false, 0.0);
            }

            if (m_performSwifts) {
                m_p1Clicked = false;
                pl->queueButton(1, false, false, 0.0);
            }
        }

        if (performPlayer2()) {
            if (!m_p2Clicked) {
                pl->queueButton(1, true, true, 0.0);
                m_p2Clicked = true;
            } else if (m_p2Clicked) {
                m_p2Clicked = false;
                pl->queueButton(1, false, true, 0.0);
            }

            if (m_performSwifts) {
                m_p2Clicked = false;
                pl->queueButton(1, false, true, 0.0);
            }
        }
    }
}
