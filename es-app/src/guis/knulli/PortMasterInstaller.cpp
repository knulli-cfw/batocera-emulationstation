#include "guis/knulli/PortMasterInstaller.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>

const std::string PORTMASTER_INSTALLER_PATH = "/usr/share/knulli/datainit/roms/ports/Install.PortMaster.sh";
const std::string PORTMASTER_INSTALLATION_PATH = "/userdata/system/.local/share/PortMaster";

int PortMasterInstaller::install()
{
	if (hasInstaller()) {
		int result = system(PORTMASTER_INSTALLER_PATH.c_str());
		return WEXITSTATUS(result);
	}
	// Installer is missing
	return 2;
}


bool PortMasterInstaller::isInstalled() {

    auto dirContent = Utils::FileSystem::getDirContent(PORTMASTER_INSTALLATION_PATH);
	if (dirContent.size() > 0) {
		return true;
	}
    return false;

}

bool PortMasterInstaller::hasInstaller() {

	if (Utils::FileSystem::exists(PORTMASTER_INSTALLER_PATH)) {
		return true;
	}
    return false;

}