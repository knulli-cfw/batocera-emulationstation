#include "guis/knulli/PortMasterInstaller.h"
#include "guis/knulli/syscalls/ThreadedRunner.h"
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

const std::string PORTMASTER_INSTALLER_PATH = "/usr/share/portmaster/Install.PortMaster.sh";
const std::string PORTMASTER_INSTALLATION_PATH = "/userdata/system/.local/share/PortMaster";

void PortMasterInstaller::install(Window* window)
{
	if (hasInstaller()) {
		ThreadedRunner::start(window, _("INSTALLING PORTMASTER"), _("Installing PortMaster..."), PORTMASTER_INSTALLER_PATH.c_str());
	}
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