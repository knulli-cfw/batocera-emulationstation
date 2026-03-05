#include "BezelResolver.h"
#include <fstream>
#include <sstream>
#include <numeric>
#include <cstdio>
#include <memory>
#include <array>

// Helper: GCD for aspect ratio calculation
static int get_gcd(int a, int b) {
    while (b) {
        a %= b;
        std::swap(a, b);
    }
    return a;
}

std::string BezelResolver::toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

std::string BezelResolver::toAspectRatio(int width, int height) {
    int common = get_gcd(width, height);
    return std::to_string(width / common) + "_" + std::to_string(height / common);
}

Resolution BezelResolver::getCurrentResolution() {
    std::array<char, 128> buffer;
    std::string result;
    // Capturing knulli-resolution output
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("knulli-resolution currentResolution", "r"), pclose);
    
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
    }

    size_t xPos = result.find('x');
    if (xPos != std::string::npos) {
        try {
            return { std::stoi(result.substr(0, xPos)), std::stoi(result.substr(xPos + 1)) };
        } catch (...) {}
    }
    return { 1920, 1080 }; // Fallback
}

std::string BezelResolver::getAltDecoration(const std::string& systemName, const std::string& romPath, const std::string& emulator) {
    if (emulator != "mame" && emulator != "retroarch") return "standalone";

    const std::vector<std::string> specialSystems = { 
        "lynx", "wswan", "wswanc", "mame", "fbneo", "naomi", "atomiswave", "nds", "3ds", "vectrex" 
    };

    if (std::find(specialSystems.begin(), specialSystems.end(), systemName) == specialSystems.end()) {
        return "0";
    }

    fs::path specialFile = DEFAULTS_DIR / "data" / "special" / (systemName + ".csv");
    if (!fs::exists(specialFile)) return "0";

    std::string romCompare = toLower(fs::path(romPath).stem().string());
    std::ifstream file(specialFile);
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); 
        std::stringstream ss(line);
        std::string romInFile, rotationValue;

        if (std::getline(ss, romInFile, ';') && std::getline(ss, rotationValue, ';')) {
            if (toLower(romInFile) == romCompare) return rotationValue;
        }
    }

    return "0";
}

std::optional<BezelInfos> BezelResolver::getBezelInfos(const std::string& romPath, 
                                                      const std::string& bezelName, 
                                                      const std::string& systemName, 
                                                      const std::string& emulator) {
    Resolution res = getCurrentResolution();
    std::string aspect = toAspectRatio(res.width, res.height);
    std::string altDeco = getAltDecoration(systemName, romPath, emulator);
    std::string romBase = fs::path(romPath).stem().string();

    struct Target { std::string pathStr; bool isSpecific; };
    std::vector<Target> search_paths;

    // 1. Rom specific paths (Priority 1-8 in Python)
    std::vector<fs::path> roots = { USER_DECORATIONS / bezelName, SYSTEM_DECORATIONS / bezelName };
    
    for (auto& root : roots) {
        search_paths.push_back({ (root / "games" / systemName / (romBase + "-" + aspect)).string(), true });
        search_paths.push_back({ (root / "games" / systemName / romBase).string(), true });
    }
    for (auto& root : roots) {
        search_paths.push_back({ (root / "games" / (romBase + "-" + aspect)).string(), true });
        search_paths.push_back({ (root / "games" / romBase).string(), true });
    }

    // 2. Alt-Decoration paths
    if (altDeco != "0") {
        for (auto& root : roots) {
            search_paths.push_back({ (root / "systems" / (systemName + "-" + altDeco + "-" + aspect)).string(), false });
            search_paths.push_back({ (root / "systems" / (systemName + "-" + altDeco)).string(), false });
            search_paths.push_back({ (root / "default-" + altDeco + "-" + aspect).string(), true });
            search_paths.push_back({ (root / "default-" + altDeco).string(), true });
        }
    }

    // 3. System and Default paths
    if (altDeco != "90" && altDeco != "270") {
        for (auto& root : roots) {
            search_paths.push_back({ (root / "systems" / (systemName + "-" + aspect)).string(), false });
            search_paths.push_back({ (root / "systems" / systemName).string(), false });
            search_paths.push_back({ (root / "default-" + aspect).string(), true });
            search_paths.push_back({ (root / "default").string(), true });
        }
    }

    // Execution: Find first existing .png
    for (const auto& target : search_paths) {
        fs::path pngFile = target.pathStr + ".png";
        if (fs::exists(pngFile)) {
            return BezelInfos{
                pngFile,
                target.pathStr + ".info",
                target.pathStr + ".lay",
                target.pathStr + ".zip",
                target.isSpecific
            };
        }
    }

    return std::nullopt;
}