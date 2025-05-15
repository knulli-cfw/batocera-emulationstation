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

const std::vector<std::string> KNULLI_COMPATIBLE_THEMES = {"Art-Book-Next", "Canvas", "Knulli", "Techdweeb", "Minimal"};

bool KnulliThemeCheck::isCompatible(const std::string themeName)
{
	for (const auto& compatibleTheme : KNULLI_COMPATIBLE_THEMES) {
		if (themeName == compatibleTheme) {
			return true;
		}
	}
    return false;
}