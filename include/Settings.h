#pragma once

#include <cstdint>

namespace UCI::Settings {
    extern float delay;
    extern bool allCorpses;
    extern std::uint32_t toggleKey;

    void Load();
}
