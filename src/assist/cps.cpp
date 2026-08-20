#include "cps.hpp"

#include "bot/bot.hpp"
#include "bot/updater.hpp"

int CPSCounter::queryCPS(int player) {
    if (player == 1) {
        return m_timestampsP1.size();
    } else if (player == 2) {
        return m_timestampsP2.size();
    }

    return -1;
}

int CPSCounter::queryMaxCPS(int player) {
    if (player == 1) {
        return m_maxCPSP1;
    } else if (player == 2) {
        return m_maxCPSP2;
    }

    return -1;
}

void CPSCounter::clearActions() {
    m_maxCPSP1 = 0;
    m_maxCPSP2 = 0;

    while (!m_timestampsP1.empty()) m_timestampsP1.pop();
    while (!m_timestampsP2.empty()) m_timestampsP2.pop();
}

void CPSCounter::pushAction(bool player2) {
    auto* gjbgl = GJBaseGameLayer::get();
    if (!gjbgl) return;

    double time = gjbgl->m_gameState.m_levelTime;

    if (player2) {
        m_timestampsP2.push(time);
    } else {
        m_timestampsP1.push(time);
    }

    m_maxCPSP1 = std::max(m_maxCPSP1, static_cast<int>(m_timestampsP1.size()));
    m_maxCPSP2 = std::max(m_maxCPSP2, static_cast<int>(m_timestampsP2.size()));
}

void CPSCounter::update() {
    if (m_timestampsP1.empty() && m_timestampsP2.empty()) return;

    auto* gjbgl = GJBaseGameLayer::get();
    if (!gjbgl) return;

    double time = gjbgl->m_gameState.m_levelTime;

    while (!m_timestampsP1.empty() && time - m_timestampsP1.front() > 1.0) {
        m_timestampsP1.pop();
    }

    while (!m_timestampsP2.empty() && time - m_timestampsP2.front() > 1.0) {
        m_timestampsP2.pop();
    }
}
