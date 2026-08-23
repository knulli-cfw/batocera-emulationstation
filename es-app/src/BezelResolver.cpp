#include "BezelResolver.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <sys/stat.h>
#include <cstdio>
#include <memory>
#include <array>

// Initialize constants
const std::string BezelResolver::KNULLI_SHARE_DIR   = "/usr/share/knulli";
const std::string BezelResolver::USERDATA           = "/userdata";
const std::string BezelResolver::DATAINIT_DIR       = "/usr/share/knulli/datainit";
const std::string BezelResolver::DEFAULTS_DIR       = "/usr/share/knulli/configgen";
const std::string BezelResolver::USER_DECORATIONS   = "/userdata/decorations";
const std::string BezelResolver::SYSTEM_DECORATIONS = "/usr/share/knulli/datainit/decorations";

bool BezelResolver::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string BezelResolver::join(const std::string& p1, const std::string& p2) {
    if (p1.empty()) return p2;
    if (p2.empty()) return p1;
    if (p1.back() == '/') return p1 + p2;
    return p1 + "/" + p2;
}

std::string BezelResolver::getStem(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
    size_t lastDot = filename.find_last_of(".");
    if (lastDot == std::string::npos) return filename;
    return filename.substr(0, lastDot);
}

std::string BezelResolver::toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

std::string BezelResolver::toAspectRatio(int width, int height) {
    auto gcd = [](int a, int b) {
        while (b) { a %= b; std::swap(a, b); }
        return a;
    };
    int common = gcd(width, height);
    return std::to_string(width / common) + "_" + std::to_string(height / common);
}

Resolution BezelResolver::getCurrentResolution() {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen("knulli-resolution currentResolution", "r");
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe) != NULL) {
            result += buffer.data();
        }
        pclose(pipe);
    }
    size_t xPos = result.find('x');
    if (xPos != std::string::npos) {
        return { std::stoi(result.substr(0, xPos)), std::stoi(result.substr(xPos + 1)) };
    }
    return { 1920, 1080 };
}

std::string BezelResolver::getAltDecoration(const std::string& systemName, const std::string& romPath, const std::string& emulator) {
    if (emulator != "mame" && emulator != "retroarch") return "standalone";
    
    std::vector<std::string> special = {"lynx", "wswan", "wswanc", "mame", "fbneo", "naomi", "atomiswave", "nds", "3ds", "vectrex"};
    if (std::find(special.begin(), special.end(), systemName) == special.end()) return "0";

    std::string csvPath = join(DEFAULTS_DIR, "data/special/" + systemName + ".csv");
    if (!fileExists(csvPath)) return "0";

    std::string romCompare = toLower(getStem(romPath));
    std::ifstream file(csvPath);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        std::string romInFile, val;
        if (std::getline(ss, romInFile, ';') && std::getline(ss, val, ';')) {
            if (toLower(romInFile) == romCompare) return val;
        }
    }
    return "0";
}

BezelInfos BezelResolver::getBezelInfos(const std::string& romPath, const std::string& bezelName, const std::string& systemName, const std::string& emulator) {
    BezelInfos res;
    res.valid = false;

    Resolution screen = getCurrentResolution();
    std::string aspect = toAspectRatio(screen.width, screen.height);
    std::string alt = getAltDecoration(systemName, romPath, emulator);
    std::string romBase = getStem(romPath);

    struct SearchTarget { std::string path; bool specific; };
    std::vector<SearchTarget> targets;
    std::vector<std::string> roots = { join(USER_DECORATIONS, bezelName), join(SYSTEM_DECORATIONS, bezelName) };

    // 1. Game Specific
    for (auto& r : roots) {
        targets.push_back({ join(r, "games/" + systemName + "/" + romBase + "-" + aspect), true });
        targets.push_back({ join(r, "games/" + systemName + "/" + romBase), true });
        targets.push_back({ join(r, "games/" + romBase + "-" + aspect), true });
        targets.push_back({ join(r, "games/" + romBase), true });
    }

    // 2. Alt Decorations
    if (alt != "0") {
        for (auto& r : roots) {
            targets.push_back({ join(r, "systems/" + systemName + "-" + alt + "-" + aspect), false });
            targets.push_back({ join(r, "systems/" + systemName + "-" + alt), false });
            targets.push_back({ join(r, "default-" + alt + "-" + aspect), true });
            targets.push_back({ join(r, "default-" + alt), true });
        }
    }

    // 3. System Defaults
    if (alt != "90" && alt != "270") {
        for (auto& r : roots) {
            targets.push_back({ join(r, "systems/" + systemName + "-" + aspect), false });
            targets.push_back({ join(r, "systems/" + systemName), false });
            targets.push_back({ join(r, "default-" + aspect), true });
            targets.push_back({ join(r, "default"), true });
        }
    }

    for (const auto& t : targets) {
        if (fileExists(t.path + ".png")) {
            res.png = t.path + ".png";
            res.info = t.path + ".info";
            res.layout = t.path + ".lay";
            res.mamezip = t.path + ".zip";
            res.specific_to_game = t.specific;
            res.valid = true;
            return res;
        }
    }
    return res;
}