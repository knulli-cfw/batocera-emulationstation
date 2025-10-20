#include "guis/knulli/syscalls/DisplaySettings.h"
#include "guis/knulli/syscalls/SysCalls.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>

const std::string DISPLAY_SETTINGS_COMMAND_NAME = "/usr/bin/knulli-display-settings";
const std::string DISPLAY_SETTINGS_ARGUMENT_GET_CAPABILITIES = "--get-capabilities";
const std::string SEPARATOR = " ";

bool DisplaySettings::hasDisplaySettings()
{
	return Utils::FileSystem::exists(DISPLAY_SETTINGS_COMMAND_NAME);
}

std::vector<std::string> DisplaySettings::getCapabilities()
{
	 return SysCalls::executeAndCatchOutputCsv(DISPLAY_SETTINGS_COMMAND_NAME
		+ SEPARATOR + DISPLAY_SETTINGS_ARGUMENT_GET_CAPABILITIES);
}

void DisplaySettings::set(std::string capability, int value)
{
	std::string command = DISPLAY_SETTINGS_COMMAND_NAME
		+ SEPARATOR + capability
		+ SEPARATOR + std::to_string(value);

	SysCalls::execute(command);
}
	