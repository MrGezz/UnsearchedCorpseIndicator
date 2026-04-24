#include "PCH.h"
#include "State.h"

#include <chrono>

namespace UCI {
    std::unordered_map<RE::FormID, float> g_deathTimes;
    std::unordered_map<RE::FormID, float> g_playerKilled;
    std::unordered_set<RE::FormID> g_searched;
    bool g_iconsEnabled = true;

    float GetRealTime() {
        using clock = std::chrono::steady_clock;
        static auto start = clock::now();
        return std::chrono::duration<float>(clock::now() - start).count();
    }

    void ResetRuntimeState() {
        g_deathTimes.clear();
        g_playerKilled.clear();
        g_searched.clear();
        g_iconsEnabled = true;
    }
}
