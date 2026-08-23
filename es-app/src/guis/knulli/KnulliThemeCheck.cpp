#include "guis/knulli/KnulliThemeCheck.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>
#include <fstream>
#include <vector>
#include <string>

static const std::string COMPATIBLE_THEMES_FILE = "/usr/share/emulationstation/knulli-compatible-themes.txt";

std::vector<std::string> KnulliThemeCheck::getCompatibleThemes()
{
	if (Utils::FileSystem::exists(COMPATIBLE_THEMES_FILE)) {
		std::ifstream file(COMPATIBLE_THEMES_FILE);
		std::string themesCsv;
		std::getline(file, themesCsv);
		return themesCsv.empty() ? std::vector<std::string>() : Utils::String::split(themesCsv, ',');
	}
	return std::vector<std::string>();
}

bool KnulliThemeCheck::isCompatible(const std::string themeName)
{
	std::vector<std::string> compatibleThemes = KnulliThemeCheck::getCompatibleThemes();

	for (const auto& compatibleTheme : compatibleThemes) {
		if (themeName == compatibleTheme) {
			return true;
		}
	}
    return false;
}