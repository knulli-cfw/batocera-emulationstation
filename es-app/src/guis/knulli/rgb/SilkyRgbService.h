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
  std::string colorPrimary;
  std::string colorSecondary;
};

struct ColorInfo
{
  std::string id;
  std::string name;
};

class SilkyRgbService
{
public:
        static void start();
        static void stop();
        static void reloadConfig();
        static std::vector<std::string> requiredSettings();
        static std::vector<ModeInfo> getAvailableModes();
        static std::vector<PaletteInfo> getAvailablePalettes();
        static std::vector<ColorInfo> getAvailableColors();
        static void applyValue(std::string key, std::string value);
        static void updateScreenBrightness();
private:
        static bool isNumeric(const std::string& s);
};
