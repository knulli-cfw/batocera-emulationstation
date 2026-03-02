#include "guis/knulli/rgb/SilkyGuiRgbSettings.h"
#include "guis/GuiMsgBox.h"
#include "guis/knulli/ExtendedGuiSettings.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "Scripting.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include <memory>
#include <string>
#include "SilkyRgbService.h"
#include "guis/knulli/BoardCheck.h"

#include "Log.h"

constexpr const char* DEFAULT_LED_MODE = "shimmer";
constexpr const char* DEFAULT_LED_PALETTE = "Knulli";
constexpr const char* CUSTOM_LED_PALETTE = "Custom";
constexpr const char* DEFAULT_BATTERY_MODE = "continuous";
constexpr float DEFAULT_LOW_BATTERY_THRESHOLD = 20;
constexpr float DEFAULT_BRIGHTNESS = 100;

constexpr const char* MENU_EVENT_NAME = "rgb-changed";

// Constructor creates a new SilkyGuiRgbSettings menu.
SilkyGuiRgbSettings::SilkyGuiRgbSettings(Window* window) : ExtendedGuiSettings(window, "RGB LED SETTINGS")
{
    switchEnableRgb = createSwitchRgbEnabled();
    switchEnableRgb->setOnChangedCallback([this]() {
        if (switchEnableRgb->getState()) {
            SystemConf::getInstance()->set("led.enabled", "1");
            SystemConf::getInstance()->saveSystemConf();
            SilkyRgbService::start();
        } else {
            SilkyRgbService::stop();
        }
    });

    if (switchEnableRgb->getState()) {
        requiredSettings = SilkyRgbService::requiredSettings();
    }

    if (requiredSettings.empty()) {
        LOG(LogWarning) << "No required RGB settings available from SilkyRgbService, RGB settings menu will be empty.";
    } else {
        addGroup(_("LED MODE AND COLOR"));

        // LED Mode Options
        optionListMode = createModeOptionList();
        optionListMode->setSelectedChangedCallback([this](std::string value) { SilkyRgbService::applyValue("mode", value); });
    
        optionListPalette = createPaletteOptionList("palette", "COLOR PALETTE", "Select the color palette.");
        optionListPalette->setSelectedChangedCallback([this](std::string value) {
            std::shared_ptr<PaletteInfo> paletteInfo = getPaletteInfoById(value);
            if (optionListColorPrimary != nullptr && paletteInfo != nullptr) {
                optionListColorPrimary->setSelected(paletteInfo->colorPrimary);
                SilkyRgbService::applyValue("color.primary", paletteInfo->colorPrimary);
            }
            if (optionListColorSecondary != nullptr && paletteInfo != nullptr) {
                optionListColorSecondary->setSelected(paletteInfo->colorSecondary);
                SilkyRgbService::applyValue("color.secondary", paletteInfo->colorSecondary);
            }
        });

        optionListColorPrimary = createColorOptionList("color.primary", "PRIMARY COLOR", "Select the primary color.");
        optionListColorPrimary->setSelectedChangedCallback([this](std::string value) {
            if (optionListPalette != nullptr) {
                optionListPalette->setSelected(CUSTOM_LED_PALETTE);
            }
            SilkyRgbService::applyValue("color.primary", value);
        });

        optionListColorSecondary = createColorOptionList("color.secondary", "SECONDARY COLOR", "Select the secondary color (for dual-color modes).");
        optionListColorSecondary->setSelectedChangedCallback([this](std::string value) {
            if (optionListPalette != nullptr) {
                optionListPalette->setSelected(CUSTOM_LED_PALETTE);
            }
            SilkyRgbService::applyValue("color.secondary", value);
        });
        
        // Swap colors switch
        switchPaletteInvert = createSwitch(_("INVERT COLORS"), "led.palette.invert", _("Inverts palette colors for all RGB LEDs."), false, false, hasRequiredSetting("palette.invert"));
        switchPaletteInvert->setOnChangedCallback([this]() { SilkyRgbService::applyValue("palette.invert", switchPaletteInvert->getState() ? "1" : "0"); });
    
        // Swap colors switch
        switchPaletteInvertSecondary = createSwitch(_("INVERT COLORS (SECONDARY ZONE)"), "led.palette.invert.secondary", _("Inverts palette colors for secondary RGB LED zone."), false, false, hasRequiredSetting("palette.invert.secondary"));
        switchPaletteInvertSecondary->setOnChangedCallback([this]() { SilkyRgbService::applyValue("palette.invert.secondary", switchPaletteInvertSecondary->getState() ? "1" : "0"); });
    
        // Palette modification options
        optionListPaletteMod = createPaletteModOptionList();
        optionListPaletteMod->setSelectedChangedCallback([this](std::string value) { SilkyRgbService::applyValue("palette.mod", value); }); 
    
        // LED Brightness Slider
        sliderLedBrightness = createSlider(_("BRIGHTNESS"), 0.f, 100.f, 10.f, "%", "", hasRequiredSetting("brightness"));
        setConfigValueForSlider(sliderLedBrightness, DEFAULT_BRIGHTNESS, "led.brightness");
        sliderLedBrightness->setOnValueChanged([this](float value) { SilkyRgbService::applyValue("brightness", std::to_string((int)value)); });
    
        // Adaptive Brightness switch
        switchAdaptiveBrightness = createSwitch(_("ADAPTIVE BRIGHTNESS"), "led.brightness.adaptive", _("Automatically adapts LED brightness to screen brightness. (Overrides the setting above.)"), false, false, hasRequiredSetting("brightness.adaptive"));
        switchAdaptiveBrightness->setOnChangedCallback([this]() {
            SilkyRgbService::applyValue("brightness.adaptive", switchAdaptiveBrightness->getState() ? "1" : "0");
            if (switchAdaptiveBrightness->getState()) {
                SilkyRgbService::updateScreenBrightness();
            } else {
                // If adaptive brightness is turned off, immediately apply the brightness value from the slider
                SilkyRgbService::applyValue("brightness", std::to_string((int)sliderLedBrightness->getValue()));
            }
        });
    
        if (hasRequiredSetting("battery.low") == true || hasRequiredSetting("battery.charging") == true || hasRequiredSetting("battery.low.threshold") == true)
            addGroup(_("BATTERY CHARGE INDICATION"));
    
        // Low battery threshold slider
        sliderLowBatteryThreshold = createSlider(_("LOW BATTERY THRESHOLD"), 0.f, 30.f, 1.f, "%", _("Threshold for low battery indication."), hasRequiredSetting("battery.low.threshold"));
        setConfigValueForSlider(sliderLowBatteryThreshold, DEFAULT_LOW_BATTERY_THRESHOLD, "led.battery.low.threshold");
        sliderLowBatteryThreshold->setOnValueChanged([this](float value) { SilkyRgbService::applyValue("battery.low.threshold", std::to_string((int)value)); });
        optionListBatteryLow = createBatteryIndicationOptionList("battery.low", "LOW BATTERY INDICATION", "Select the type of low battery indication.");
        optionListBatteryLow->setSelectedChangedCallback([this](std::string value) { SilkyRgbService::applyValue("battery.low", value); });
        optionListBatteryCharging = createBatteryIndicationOptionList("battery.charging", "BATTERY CHARGING INDICATION", "Select the type of battery charging indication.");
        optionListBatteryCharging->setSelectedChangedCallback([this](std::string value) { SilkyRgbService::applyValue("battery.charging", value); });
    
    
        if (hasRequiredSetting("retroachievements") == true)
            addGroup(_("RETRO ACHIEVEMENT INDICATION"));
        switchRetroAchievements = createSwitch(_("ACHIEVEMENT EFFECT"), "led.retroachievements", _("Honor your retro achievements with a LED effect."), true, false, hasRequiredSetting("retroachievements"));
    }


    addSaveFunc([this] {
        // Read all variables from the respective UI elements and set the respective values in knulli.conf
        if (switchEnableRgb != nullptr) {
            SystemConf::getInstance()->set("led.enabled", switchEnableRgb->getState() ? "1" : "0");
        }
        if (optionListMode != nullptr) {
            SystemConf::getInstance()->set("led.mode", optionListMode->getSelected());
        }
        if (optionListPalette != nullptr) {
            SystemConf::getInstance()->set("led.palette", optionListPalette->getSelected());
        }
        if (sliderLedBrightness != nullptr) {
            SystemConf::getInstance()->set("led.brightness", std::to_string((int) sliderLedBrightness->getValue()));
        }
        if (switchAdaptiveBrightness != nullptr) {
            SystemConf::getInstance()->set("led.brightness.adaptive", (switchAdaptiveBrightness->getState() ? "1" : "0"));
        }
        if (sliderLowBatteryThreshold != nullptr) {
            SystemConf::getInstance()->set("led.battery.low.threshold", std::to_string((int) sliderLowBatteryThreshold->getValue()));
        }
        if (optionListBatteryLow != nullptr) {
            SystemConf::getInstance()->set("led.battery.low", optionListBatteryLow->getSelected());
        }
        if (optionListBatteryCharging != nullptr) {
            SystemConf::getInstance()->set("led.battery.charging", optionListBatteryCharging->getSelected());
        }
        if (switchRetroAchievements != nullptr) {
            SystemConf::getInstance()->set("led.retroachievements", (switchRetroAchievements->getState() ? "1" : "0"));
        }
        if (switchPaletteInvert != nullptr) {
            SystemConf::getInstance()->set("led.palette.invert", (switchPaletteInvert->getState() ? "1" : "0"));
        }
        if (switchPaletteInvertSecondary != nullptr) {
            SystemConf::getInstance()->set("led.palette.invert.secondary", (switchPaletteInvertSecondary->getState() ? "1" : "0"));
        }
        if (optionListPaletteMod != nullptr) {
            SystemConf::getInstance()->set("led.palette.mod", optionListPaletteMod->getSelected());
        }
		SystemConf::getInstance()->saveSystemConf();
		Scripting::fireEvent(MENU_EVENT_NAME);

        if (switchEnableRgb != nullptr) {
            if (switchEnableRgb->getState()) {
                SilkyRgbService::reloadConfig();
            }
        }        
    });

}

std::shared_ptr<SwitchComponent> SilkyGuiRgbSettings::createSwitchRgbEnabled()
{
	auto switchRgbEnabled = std::make_shared<SwitchComponent>(mWindow);
	bool enabled = true; // default to enabled if setting is not present
	if (!SystemConf::getInstance()->get("led.enabled").empty()) {
		enabled = SystemConf::getInstance()->getBool("led.enabled");
	}
	switchRgbEnabled->setState(enabled);
	addWithDescription(_("ENABLE RGB LED"),_("Enable or disable the RGB LED."), switchRgbEnabled);

    return switchRgbEnabled;
}

// Creates a new mode option list
std::shared_ptr<OptionListComponent<std::string>> SilkyGuiRgbSettings::createModeOptionList()
{
    auto optionsLedMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MODE"), false);

    std::string selectedLedMode = "null";
    std::vector<ModeInfo> availableModes = SilkyRgbService::getAvailableModes();

    if (availableModes.empty()) {
        LOG(LogError) << "No RGB modes available from SilkyRgbService, adding default options.";
        optionsLedMode->add(_("None"), "null", selectedLedMode == "null");
    }
    else
    {   
        selectedLedMode = DEFAULT_LED_MODE;
        std::string configuredLedMode = SystemConf::getInstance()->get("led.mode");
        if (!configuredLedMode.empty()) {
            for (const auto& mode : availableModes) {
                if (configuredLedMode == mode.id) {
                    selectedLedMode = configuredLedMode;
                    break;
                }
            }
        }

        for (const auto& mode : availableModes) {
            if (mode.name == "Glitch") {
                continue;
            }
            optionsLedMode->add(_(mode.name.c_str()), mode.id, mode.id == selectedLedMode);
        }
    }

    if (hasRequiredSetting("mode") == true)
        addWithDescription(_("MODE"), _("Set the default LED animation"), optionsLedMode);
    return optionsLedMode;
}

std::shared_ptr<OptionListComponent<std::string>> SilkyGuiRgbSettings::createPaletteOptionList(const std::string& setting, const std::string& title, const std::string& description)
{
    auto optionsLedPalette = std::make_shared<OptionListComponent<std::string>>(mWindow, _(title.c_str()), false);

    std::string configKey = "led." + setting;
    std::string selectedLedPalette = SystemConf::getInstance()->get(configKey);
    mPalettes = SilkyRgbService::getAvailablePalettes();
    if (selectedLedPalette.empty() && !mPalettes.empty())
        selectedLedPalette = DEFAULT_LED_PALETTE;
    optionsLedPalette->add(_(CUSTOM_LED_PALETTE), CUSTOM_LED_PALETTE, selectedLedPalette == CUSTOM_LED_PALETTE);
    for (const auto& palette : mPalettes) {
        optionsLedPalette->add(palette.name, palette.id, selectedLedPalette == palette.id);
    }
    addWithDescription(_(title.c_str()), _(description.c_str()), optionsLedPalette);
    return optionsLedPalette;
}

std::shared_ptr<OptionListComponent<std::string>> SilkyGuiRgbSettings::createColorOptionList(const std::string& setting, const std::string& title, const std::string& description)
{
    auto optionsColor = std::make_shared<OptionListComponent<std::string>>(mWindow, _(title.c_str()), false);

    std::string configKey = "led." + setting;
    std::string selectedColor = SystemConf::getInstance()->get(configKey);
    if (selectedColor.empty() && optionListPalette != nullptr) {
        // If no color is set but a palette is selected, try to default to the primary color of the selected palette
        std::string selectedPaletteId = optionListPalette->getSelected();
        std::shared_ptr<PaletteInfo> paletteInfo = getPaletteInfoById(selectedPaletteId);
        if (paletteInfo != nullptr) {
            if (setting == "color.primary") {
                selectedColor = paletteInfo->colorPrimary;
            } else if (setting == "color.secondary") {
                selectedColor = paletteInfo->colorSecondary;
            }
        }
    }

    std::vector<ColorInfo> availableColors = SilkyRgbService::getAvailableColors();

    for (const auto& color : availableColors) {
        optionsColor->add(color.name, color.id, selectedColor == color.id);
    }

    if (hasRequiredSetting(setting) == true)
        addWithDescription(_(title.c_str()), _(description.c_str()), optionsColor);
    return optionsColor;
}

std::shared_ptr<OptionListComponent<std::string>> SilkyGuiRgbSettings::createBatteryIndicationOptionList(const std::string& setting, const std::string& title, const std::string& description)
{
    auto optionsBatteryIndication = std::make_shared<OptionListComponent<std::string>>(mWindow, _(title.c_str()), false);

    std::string configKey = "led." + setting;
    std::string selectedOption = SystemConf::getInstance()->get(configKey);
    if (selectedOption.empty() || (selectedOption != "off" && selectedOption != "notification" && selectedOption != "continuous"))
        selectedOption = DEFAULT_BATTERY_MODE;

    optionsBatteryIndication->add(_("None"), "off", selectedOption == "off");
    optionsBatteryIndication->add(_("Notification"), "notification", selectedOption == "notification");
    optionsBatteryIndication->add(_("Continuous"), "continuous", selectedOption == "continuous");

    if (hasRequiredSetting(setting) == true)
        addWithDescription(_(title.c_str()), _(description.c_str()), optionsBatteryIndication);
    return optionsBatteryIndication;
}

std::shared_ptr<OptionListComponent<std::string>> SilkyGuiRgbSettings::createPaletteModOptionList()
{
    auto optionsPaletteMod = std::make_shared<OptionListComponent<std::string>>(mWindow, _("PALETTE MODIFICATION"), false);

    std::string selectedPaletteMod = SystemConf::getInstance()->get("led.palette.mod");
    if (selectedPaletteMod.empty())
        selectedPaletteMod = "none";

    optionsPaletteMod->add(_("None"), "none", selectedPaletteMod == "none");
    optionsPaletteMod->add(_("Twilight"), "twilight", selectedPaletteMod == "twilight");
    optionsPaletteMod->add(_("Sparkle"), "sparkle", selectedPaletteMod == "sparkle");
    optionsPaletteMod->add(_("Haze"), "haze", selectedPaletteMod == "haze");

    if (hasRequiredSetting("palette.mod") == true)
        addWithDescription(_("PALETTE MOD"), _("Select a modification effect for the color palette."), optionsPaletteMod);
    return optionsPaletteMod;
}

void SilkyGuiRgbSettings::applyValue(const std::string& key, const std::string& value)
{
    SilkyRgbService::applyValue(key, value);
}

bool SilkyGuiRgbSettings::hasRequiredSetting(std::string setting)
{
    return std::find(requiredSettings.begin(), requiredSettings.end(), setting) != requiredSettings.end();
}
