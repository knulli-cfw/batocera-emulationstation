#include "guis/knulli/GuiTools.h"
#include "guis/knulli/GuiPowerManagementSettings.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"

GuiTools::GuiTools(Window* window) : GuiSettings(window, _("TOOLS").c_str())
{
	addEntry(_("POWER MANAGEMENT"), true, [this] { openPowerManagementSettings(); });
}


void GuiTools::openPowerManagementSettings()
{
	mWindow->pushGui(new GuiPowerManagementSettings(mWindow));
}
