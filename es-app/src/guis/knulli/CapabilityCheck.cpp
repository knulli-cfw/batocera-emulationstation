#include "guis/knulli/CapabilityCheck.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>
#include <fstream>
#include <string>

const std::string CAPABILITY_CHECK_COMMAND_NAME = "/usr/bin/knulli-board-capability";
const std::string SEPARATOR = " ";

bool CapabilityCheck::hasCapability(const std::string capability)
{
	int result = system((CAPABILITY_CHECK_COMMAND_NAME
		+ SEPARATOR + capability
		).c_str());
	return WEXITSTATUS(result) == 0;
}