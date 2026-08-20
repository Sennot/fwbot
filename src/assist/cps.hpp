#pragma once

#include <queue>
#include <slc/slc.hpp>

class CPSCounter {
   private:
    std::queue<double> m_timestampsP1;
    std::queue<double> m_timestampsP2;

    int m_maxCPSP1;
    int m_maxCPSP2;

   public:
    int queryCPS(int player = 1);
    int queryMaxCPS(int player = 1);

    void clearActions();
    void pushAction(bool player2);

    void update();
};
