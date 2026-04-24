#pragma once

#include <unordered_map>
#include <unordered_set>

namespace UCI {
    extern std::unordered_map<RE::FormID, float> g_deathTimes;
    extern std::unordered_map<RE::FormID, float> g_playerKilled;
    extern std::unordered_set<RE::FormID> g_searched;
    extern bool g_iconsEnabled;

    float GetRealTime();
    void ResetRuntimeState();
}
