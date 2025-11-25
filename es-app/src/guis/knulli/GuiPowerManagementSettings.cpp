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
#include "Scripting.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"
#include "CapabilityCheck.h"

GuiPowerManagementSettings::GuiPowerManagementSettings(Window* window) : GuiSettings(window, _("POWER MANAGEMENT").c_str())
{

	addGroup(_("BATTERY SAVING"));

	// Battery save mode
	auto optionsBatterySaveMode = std::make_shared<OptionListComponent<std::string> >(mWindow, _("MODE"), false);

	std::string selectedBatteryMode = SystemConf::getInstance()->get("system.batterysaver.mode");
	if (selectedBatteryMode.empty())
		selectedBatteryMode = "dim";

	optionsBatterySaveMode->add(_("DIM"),            "dim", selectedBatteryMode == "dim");
	optionsBatterySaveMode->add(_("DISPLAY OFF"),    "dispoff", selectedBatteryMode == "dispoff");
	optionsBatterySaveMode->add(_("SUSPEND"),        "suspend", selectedBatteryMode == "suspend");
	optionsBatterySaveMode->add(_("SHUTDOWN"),       "shutdown", selectedBatteryMode == "shutdown");
	optionsBatterySaveMode->add(_("NONE"),           "none", selectedBatteryMode == "none");

	addWithLabel(_("MODE"), optionsBatterySaveMode);

	// Idlewatcher timer (1-60 minutes in 1 minute steps)
	auto sliderIdleWatcherTimer = std::make_shared<SliderComponent>(mWindow, 1.f, 60.f, 1.f, "m");

	float selectedIdleWatcherTimeSeconds = 300.f;
	std::string configuredIdleWatcherTime = SystemConf::getInstance()->get("system.idlewatcher.timer");
	if (!configuredIdleWatcherTime.empty()) {
		selectedIdleWatcherTimeSeconds = Utils::String::toFloat(configuredIdleWatcherTime);
	}

	int selectedIdleWatcherTimeMinutes = (int)Math::round(selectedIdleWatcherTimeSeconds/60.f);

	sliderIdleWatcherTimer->setValue((float)(selectedIdleWatcherTimeMinutes));
	addWithDescription(_("IDLE TIME"),_("Battery save mode is activated after idle time has passed without any input."), sliderIdleWatcherTimer);

	// Battery save extended mode
	auto optionsBatterySaveExtendedMode = std::make_shared<OptionListComponent<std::string> >(mWindow, _("EXTENDED MODE"), false);

	std::string selectedBatteryExtendedMode = SystemConf::getInstance()->get("system.batterysaver.extendedmode");
	if (selectedBatteryExtendedMode.empty())
		selectedBatteryExtendedMode = "suspend";

	optionsBatterySaveExtendedMode->add(_("SUSPEND"),        "suspend", selectedBatteryExtendedMode == "suspend");
	optionsBatterySaveExtendedMode->add(_("SHUTDOWN"),       "shutdown", selectedBatteryExtendedMode == "shutdown");
	optionsBatterySaveExtendedMode->add(_("NONE"),           "none", selectedBatteryExtendedMode == "none");

	addWithLabel(_("EXTENDED MODE"), optionsBatterySaveExtendedMode);

	// Idlewatcher extended timer (1-60 minutes in 1 minute steps)
	auto sliderIdleWatcherExtendedTime = std::make_shared<SliderComponent>(mWindow, 1.f, 60.f, 1.f, "m");

	float selectedIdleWatcherExtendedTimeSeconds = 900.f;
	std::string configuredIdleWatcherExtendedTime = SystemConf::getInstance()->get("system.idlewatcher.extendedtimer");
	if (!configuredIdleWatcherExtendedTime.empty()) {
		selectedIdleWatcherExtendedTimeSeconds = Utils::String::toFloat(configuredIdleWatcherExtendedTime);
	}

	int selectedIdleWatcherExtendedTimeMinutes = (int)Math::round(selectedIdleWatcherExtendedTimeSeconds/60.f);

	sliderIdleWatcherExtendedTime->setValue((float)(selectedIdleWatcherExtendedTimeMinutes));
	addWithDescription(_("EXTENDED IDLE TIME"),_("Secondary timer which starts after initial idle time has passed without any input."), sliderIdleWatcherExtendedTime);

	// Aggressive battery save mode toggle
	auto aggressiveBatterySaveMode = std::make_shared<SwitchComponent>(mWindow);

	aggressiveBatterySaveMode->setState(SystemConf::getInstance()->getBool("system.batterysaver.aggressive"));
	addWithDescription(_("AGGRESSIVE BATTERY SAVER"),_("Optimizes battery life with extra power-saving measures during system idle."), aggressiveBatterySaveMode);

	// Power LED toggle switch
	auto powerLedSwitch = std::make_shared<SwitchComponent>(mWindow);
	if (CapabilityCheck::hasCapability(CapabilityCheck::PWRLED_CAPABILITY)) {
		bool powerLedEnabled = SystemConf::getInstance()->getBool("system.power.led");
		powerLedSwitch->setState(powerLedEnabled);

		addWithLabel(_("POWER LED"), powerLedSwitch);
	}

	// Lid close mode
	auto optionsLidCloseMode = std::make_shared<OptionListComponent<std::string> >(mWindow, _("LID CLOSE MODE"), false);
	// TODO: do not even instantiate if lid is not supported
	if (CapabilityCheck::hasCapability(CapabilityCheck::LID_CAPABILITY)) {
		std::string selectedLidCloseMode = SystemConf::getInstance()->get("system.lid");
		if (selectedLidCloseMode.empty())
			selectedLidCloseMode = "suspend";

		optionsLidCloseMode->add(_("NONE"),           "none", selectedLidCloseMode == "none");
		optionsLidCloseMode->add(_("DISPLAY OFF"),    "dispoff", selectedLidCloseMode == "dispoff");
		optionsLidCloseMode->add(_("SUSPEND"),        "suspend", selectedLidCloseMode == "suspend");
		optionsLidCloseMode->add(_("SHUTDOWN"),       "shutdown", selectedLidCloseMode == "shutdown");

		addWithLabel(_("LID CLOSE MODE"), optionsLidCloseMode);
	}

	addSaveFunc([this,
	             optionsBatterySaveMode,
	             sliderIdleWatcherTimer,
	             optionsBatterySaveExtendedMode,
	             sliderIdleWatcherExtendedTime,
	             aggressiveBatterySaveMode,
	             powerLedSwitch,
	             optionsLidCloseMode]
	{
		int newIdleWatcherTimeSeconds = (int)Math::round(sliderIdleWatcherTimer->getValue()*60.f);
		int newIdleWatcherExtendedTimeSeconds = (int)Math::round(sliderIdleWatcherExtendedTime->getValue()*60.f);
		SystemConf::getInstance()->set("system.idlewatcher.timer", std::to_string(newIdleWatcherTimeSeconds));
		SystemConf::getInstance()->set("system.batterysaver.mode", optionsBatterySaveMode->getSelected());
		SystemConf::getInstance()->set("system.idlewatcher.extendedtimer", std::to_string(newIdleWatcherExtendedTimeSeconds));
		SystemConf::getInstance()->set("system.batterysaver.extendedmode", optionsBatterySaveExtendedMode->getSelected());
		SystemConf::getInstance()->setBool("system.batterysaver.aggressive", aggressiveBatterySaveMode->getState());
		if (CapabilityCheck::hasCapability(CapabilityCheck::PWRLED_CAPABILITY)) {
			SystemConf::getInstance()->setBool("system.power.led", powerLedSwitch->getState());
		}
		if (CapabilityCheck::hasCapability(CapabilityCheck::LID_CAPABILITY)) {
			SystemConf::getInstance()->set("system.lid", optionsLidCloseMode->getSelected());
		}
		SystemConf::getInstance()->saveSystemConf();
		Scripting::fireEvent("powermanagement-changed");
		setVariable("exitreboot", true);
	});

}
