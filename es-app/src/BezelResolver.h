#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

struct BezelInfos {
    fs::path png;
    fs::path info;
    fs::path layout;
    fs::path mamezip;
    bool specific_to_game;
};

struct Resolution {
    int width;
    int height;
};

class BezelResolver {
public:
    // Main entry point
    static std::optional<BezelInfos> getBezelInfos(const std::string& romPath, 
                                                   const std::string& bezelName, 
                                                   const std::string& systemName, 
                                                   const std::string& emulator);

private:
    // Knulli Hard-wired Paths
    static inline const fs::path KNULLI_SHARE_DIR   = "/usr/share/knulli";
    static inline const fs::path USERDATA           = "/userdata";
    static inline const fs::path DATAINIT_DIR       = KNULLI_SHARE_DIR / "datainit";
    static inline const fs::path DEFAULTS_DIR       = KNULLI_SHARE_DIR / "configgen";
    
    // Decoration specific paths
    static inline const fs::path USER_DECORATIONS   = USERDATA / "decorations";
    static inline const fs::path SYSTEM_DECORATIONS = DATAINIT_DIR / "decorations";

    // Internal Helpers
    static Resolution getCurrentResolution();
    static std::string toAspectRatio(int width, int height);
    static std::string getAltDecoration(const std::string& systemName, const std::string& romPath, const std::string& emulator);
    static std::string toLower(std::string s);
};