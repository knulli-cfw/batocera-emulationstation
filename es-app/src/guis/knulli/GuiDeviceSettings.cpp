#include "guis/knulli/GuiDeviceSettings.h"
#include "guis/knulli/ExtendedGuiSettings.h"

#include "guis/knulli/FactorySettings.h"
#include "guis/knulli/GuiDisplaySettings.h"
#include "guis/knulli/GuiPowerManagementSettings.h"
#include "guis/knulli/rgb/LegacyGuiRgbSettings.h"
#include "guis/knulli/rgb/SilkyGuiRgbSettings.h"
#include "guis/knulli/rgb/SilkyRgbService.h"
#include "guis/knulli/Pico8Installer.h"
#include "guis/knulli/ThreadedSyncthing.h"
#include "guis/knulli/syscalls/DisplaySettings.h"
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
#include "BoardCheck.h"
#include "CapabilityCheck.h"
#include "UsbService.h"
#include "utils/SyncthingUtil.h"

const std::vector<std::string> BOARDS_WITH_TOGGLE_SWITCH = {"trimui-brick", "trimui-smart-pro"};

constexpr const char* DEFAULT_USB_MODE = "off";
constexpr const char* DEFAULT_SWITCH_MODE = "mute";

GuiDeviceSettings::GuiDeviceSettings(Window* window) : ExtendedGuiSettings(window, _("DEVICE SETTINGS").c_str())
{
	addGroup(_("POWER SAVING AND BATTERY LIFE"));
	addEntry(_("POWER MANAGEMENT"), true, [this] { openPowerManagementSettings(); });
	if(CapabilityCheck::hasCapability(CapabilityCheck::RGB_CAPABILITY) || BoardCheck::isBoard(BOARDS_WITH_TOGGLE_SWITCH) || DisplaySettings::hasDisplaySettings()) {
		addGroup(_("DEVICE CUSTOMIZATION"));
		// Only add Display Settings if display settings are supported on this device.
		if(DisplaySettings::hasDisplaySettings()) {
			addEntry(_("DISPLAY SETTINGS"), true, [this] { openDisplaySettings(); });
		}
		if(CapabilityCheck::hasCapability(CapabilityCheck::RGB_CAPABILITY)) {
			addEntry(_("RGB LED SETTINGS"), true, [this] { openRgbLedSettings(); });
		}
		if(BoardCheck::isBoard(BOARDS_WITH_TOGGLE_SWITCH)) {
			optionsToggleSwitchMode = createToggleSwitchModeOptionList();
	
			addSaveFunc([this] {
				// Set the toggle Switch mode in batocera.conf
				SystemConf::getInstance()->set("system.toggleswitch.mode", optionsToggleSwitchMode->getSelected());
				SystemConf::getInstance()->saveSystemConf();
			});
	
		}
	}

	if(SyncthingUtil::isEnabled()) {
		addGroup(_("SYNCTHING"));

		addWithDescription(_("Synchronize now"), _("Attempt to synchronize your devices with Syncthing."), nullptr, [this]
		{
			if (ThreadedSyncthing::isRunning())
				mWindow->pushGui(new GuiMsgBox(mWindow, _("SYNCHRONIZATION IS ALREADY RUNNING.")));
			else
			{
				mWindow->pushGui(new GuiMsgBox(mWindow, _("SYNCHRONIZE NOW?"), _("YES"), [this]
					{
						ThreadedSyncthing::start(mWindow);
					},
					_("NO"), nullptr));
			}
		});

		switchSyncthingScanOnGameExit = createSwitch(_("AUTO-SCAN ON GAME EXIT"), "syncthing.autoscan", _("Scan for changed files after exiting a game."), true, false, true);
		addSaveFunc([this] {
			// Set the telemetry settings in batocera.conf
			SystemConf::getInstance()->set("syncthing.autoscan", switchSyncthingScanOnGameExit->getState() ? "1" : "0");
			SystemConf::getInstance()->saveSystemConf();
		});

	}

	if(Pico8Installer::hasInstaller()) {
		addGroup(_("NATIVE PICO-8"));
		addEntry(_("INSTALL PICO-8"), true, [this] { installPico8(); });
	}
	// Only add USB MODE options if USB service is available on this device.
	if (UsbService::hasService() && (CapabilityCheck::hasCapability(CapabilityCheck::ADB_CAPABILITY) || CapabilityCheck::hasCapability(CapabilityCheck::MTP_CAPABILITY))) {
		addGroup(_("USB MODE"));
		optionsUsbMode = createUsbModeOptionList();

		addSaveFunc([this] {		
			// Set the USB mode in batocera.conf
			SystemConf::getInstance()->set("system.usbmode", optionsUsbMode->getSelected());
			SystemConf::getInstance()->saveSystemConf();

			if (optionsUsbMode->getSelected() == "off") {
				// Deactivate the USB Service
				UsbService::stop();
			} else {
				// Reactivate the USB Service
				UsbService::restart();	
			}
		});
	}

	addGroup(_("TELEMETRY"));
	switchTelemetryStatistics = createSwitch(_("ENABLE STATISTICS"), "system.telemetry", _("Help the Knulli project keep track of which devices and Knulli versions are currently in use. Enable telemetry and report your device model and your current Knulli version to our statistics server after boot. No other data will be transmitted!"), true, false, true);
	addSaveFunc([this] {
		// Set the telemetry settings in batocera.conf
		SystemConf::getInstance()->set("system.telemetry", switchTelemetryStatistics->getState() ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});

	if (FactorySettings::hasFactoryReset()) {
		addGroup(_("FACTORY SETTINGS"));
		addWithDescription(_("FACTORY RESET"), _("This will reset all your Knulli settings to factory defaults. Other user data (e.g., games, BIOS files, saves, etc.) will be left untouched."), nullptr, [this]
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE YOU WANT TO RESET TO FACTORY SETTINGS? ALL YOUR SETTINGS WILL BE UNDONE!\n\nDO NOT PANIC: THE SCREEN WILL TURN OFF AND THE DEVICE WILL REBOOT AUTOMATICALLY AFTER A COUPLE OF SECONDS."), _("YES"), [this]
				{
					FactorySettings::applyFactoryReset();
				},
				_("NO"), nullptr));
		}, "", false, true);
	}

}

void GuiDeviceSettings::openPowerManagementSettings()
{
	mWindow->pushGui(new GuiPowerManagementSettings(mWindow));
}

void GuiDeviceSettings::openDisplaySettings()
{
	mWindow->pushGui(new GuiDisplaySettings(mWindow));
}

void GuiDeviceSettings::openRgbLedSettings()
{
	if (SilkyRgbService::isInstalled()) {
		mWindow->pushGui(new SilkyGuiRgbSettings(mWindow));
	} else {
		mWindow->pushGui(new LegacyGuiRgbSettings(mWindow));
	}
}

void GuiDeviceSettings::installPico8()
{
	int result = Pico8Installer::install();
	if(result == 0) {
		mWindow->pushGui(new GuiMsgBox(mWindow, _("Native Pico-8 was successfully installed."), _("OK"), nullptr));
	} else if(result == 1) {
		mWindow->pushGui(new GuiMsgBox(mWindow, _("Unable to install: An unknown error occurred. If the error persists, try installing Pico-8 manually."), _("OK"), nullptr));
	} else if(result == 2) {
		mWindow->pushGui(new GuiMsgBox(mWindow, _("Unable to install: Pico-8 installer files missing. Please download the Raspberry Pi version of Pico-8 and place the ZIP file in the roms/pico8 folder and try again."), _("OK"), nullptr));
	}
}

// Creates a new USB mode option list
std::shared_ptr<OptionListComponent<std::string>> GuiDeviceSettings::createUsbModeOptionList()
{
    auto optionsUsbMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("USB MODE"), false);

    std::string selectedUsbMode = SystemConf::getInstance()->get("system.usbmode");
    if (selectedUsbMode.empty())
        selectedUsbMode = DEFAULT_USB_MODE;

	optionsUsbMode->add(_("OFF"), "off", selectedUsbMode == "off");
	if (CapabilityCheck::hasCapability(CapabilityCheck::ADB_CAPABILITY)) {
		optionsUsbMode->add(_("ADB"), "adb", selectedUsbMode == "adb");
	}
	if (CapabilityCheck::hasCapability(CapabilityCheck::MTP_CAPABILITY)) {
		optionsUsbMode->add(_("MTP"), "mtp", selectedUsbMode == "mtp");
	}

    addWithDescription(_("USB MODE"), _("Set the USB mode to access your device."), optionsUsbMode);
    return optionsUsbMode;
}

// Creates a new toggle switch mode option list.
std::shared_ptr<OptionListComponent<std::string>> GuiDeviceSettings::createToggleSwitchModeOptionList()
{
    auto optionsToggleSwitchMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("TOGGLE SWITCH MODE"), false);

    std::string selectedToggleSwitchMode = SystemConf::getInstance()->get("system.toggleswitch.mode");
    if (selectedToggleSwitchMode.empty()) {
		selectedToggleSwitchMode = DEFAULT_SWITCH_MODE;
	}

	optionsToggleSwitchMode->add(_("MUTE/UNMUTE"), "mute", selectedToggleSwitchMode == "mute");
	optionsToggleSwitchMode->add(_("RGB ON/OFF"), "rgboff", selectedToggleSwitchMode == "rgboff");
	optionsToggleSwitchMode->add(_("AIRPLANE MODE ON/OFF"), "airplane", selectedToggleSwitchMode == "airplane");

    addWithDescription(_("TOGGLE SWITCH MODE"), _("Decide what to use the switch of your device for."), optionsToggleSwitchMode);
    return optionsToggleSwitchMode;
}
