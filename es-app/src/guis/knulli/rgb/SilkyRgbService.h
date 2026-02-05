#pragma once
#include <string>
#include <vector>

struct ModeInfo
{
  std::string id;
  std::string name;
};

struct PaletteInfo
{
  std::string id;
  std::string name;
};

class SilkyRgbService
{
public:
        static void start();
        static void stop();
        static bool isInstalled();
        static void reloadConfig();
        static std::vector<std::string> requiredSettings();
        static std::vector<ModeInfo> getAvailableModes();
        static std::vector<PaletteInfo> getAvailablePalettes();
        static void applyValue(std::string key, std::string value);
        static void updateScreenBrightness();
private:
        static bool isNumeric(const std::string& s);
};
