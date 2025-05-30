#include "guis/knulli/FactorySettings.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>
#include <fstream>

const std::string FACTORY_RESET_NAME = "/usr/bin/knulli-factory-reset";
const std::string SEPARATOR = " ";
const std::string FORCE = "-f";

int FactorySettings::applyFactoryReset()
{
	if (hasFactoryReset()) {
		int result = system((FACTORY_RESET_NAME + SEPARATOR + FORCE).c_str());
		return WEXITSTATUS(result);
	}
	// Installer is missing
	return 2;
}


bool FactorySettings::hasFactoryReset() {
	return Utils::FileSystem::exists(FACTORY_RESET_NAME);
}
