#include "PCH.h"
#include "Settings.h"

#include <filesystem>
#include <fstream>

namespace UCI::Settings {
    float delay{ 0.0f };
    bool allCorpses{ true };
    std::uint32_t toggleKey{ 38 };

    void Load() {
        const auto path = std::filesystem::path{ "Data/SKSE/Plugins/UnsearchedCorpsesIndicator.ini" };
        if (!std::filesystem::exists(path)) return;

        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            auto key = line.substr(0, eq);
            auto val = line.substr(eq + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);

            if (key == "fDelay") delay = std::max(0.0f, std::stof(val));
            else if (key == "bAllCorpses") allCorpses = (val == "1" || val == "true");
            else if (key == "iToggleKey") toggleKey = static_cast<std::uint32_t>(std::stoul(val));
        }
    }
}
