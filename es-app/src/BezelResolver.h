#pragma once

#include <string>
#include <vector>

struct BezelInfos {
    std::string png;
    std::string info;
    std::string layout;
    std::string mamezip;
    bool specific_to_game;
    bool valid; // Replacement for std::optional
};

struct Resolution {
    int width;
    int height;
};

class BezelResolver {
public:
    static BezelInfos getBezelInfos(const std::string& romPath, 
                                    const std::string& bezelName, 
                                    const std::string& systemName, 
                                    const std::string& emulator);

private:
    static const std::string KNULLI_SHARE_DIR;
    static const std::string USERDATA;
    static const std::string DATAINIT_DIR;
    static const std::string DEFAULTS_DIR;
    static const std::string USER_DECORATIONS;
    static const std::string SYSTEM_DECORATIONS;

    static Resolution getCurrentResolution();
    static std::string toAspectRatio(int width, int height);
    static std::string getAltDecoration(const std::string& systemName, const std::string& romPath, const std::string& emulator);
    static std::string toLower(std::string s);
    static std::string getStem(const std::string& path);
    static bool fileExists(const std::string& path);
    static std::string join(const std::string& p1, const std::string& p2);
};
