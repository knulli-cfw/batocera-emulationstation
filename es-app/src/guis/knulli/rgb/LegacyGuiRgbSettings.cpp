#include "guis/knulli/rgb/LegacyGuiRgbSettings.h"
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
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include "LegacyRgbService.h"
#include "guis/knulli/BoardCheck.h"

#include "Log.h"

const std::vector<std::string> RGB_BOARDS_H700 = {"rg40xx-h", "rg40xx-v", "rg-cubexx"};
const std::vector<std::string> RGB_BOARDS_A133 = {"trimui-smart-pro", "trimui-brick"};
const std::vector<std::string> RGB_BOARDS_A523 = {"trimui-smart-pro-s"};

constexpr const char* MENU_EVENT_NAME = "rgb-changed";

constexpr char RGB_DELIMITER = ' ';
constexpr const char* DEFAULT_LED_MODE = "1";
constexpr float DEFAULT_COLOR_RED = 148;
constexpr float DEFAULT_COLOR_GREEN = 255;
constexpr float DEFAULT_COLOR_BLUE = 0;
constexpr float DEFAULT_BRIGHTNESS = 100;
constexpr float DEFAULT_SPEED = 15;
constexpr float DEFAULT_LOW_BATTERY_THRESHOLD = 20;

// Constructor creates a new GuiRgbSettings menu.
LegacyGuiRgbSettings::LegacyGuiRgbSettings(Window* window) : ExtendedGuiSettings(window, _("RGB LED SETTINGS").c_str())
{
    LOG(LogError) << "Constructor called";
    // Temporary disable LegacyRgbService to be able to interact with the RGB LEDs directly
    LegacyRgbService::stop();

    // TODO: This should not be hard-coded, it should be read from a file or a service.
    isH700 = BoardCheck::isBoard(RGB_BOARDS_H700);
    isA133 = BoardCheck::isBoard(RGB_BOARDS_A133);
    isA523 = BoardCheck::isBoard(RGB_BOARDS_A523);

    LOG(LogError) << "Checks done";

    addGroup(_("REGULAR LED MODE AND COLOR"));

    // LED Mode Options
    optionListMode = createModeOptionList();

    LOG(LogError) << "Mode option list created";

    // LED Brightness Slider
    sliderLedBrightness = createSlider(_("BRIGHTNESS"), 0.f, 100.f, 5.f, "", "", (isH700 || isA133 || isA523));    
    setConfigValueForSlider(sliderLedBrightness, DEFAULT_BRIGHTNESS, "led.brightness");

    LOG(LogError) << "Brightness slider created";

    // Adaptive Brightness switch
    switchAdaptiveBrightness = createSwitch(_("ADAPTIVE BRIGHTNESS"), "led.brightness.adaptive", _("Automatically adapts LED brightness to screen brightness (based on the brightness setting above)."), true, false, (isH700 || isA133 || isA523));

    // LED Speed Slider
    sliderLedSpeed = createSlider(_("SPEED"), 1.f, 100.f, 5.f, "", _("Not applicable for all devices/modes. Warning: High speed may cause seizures for people with photosensitive epilepsy."), isH700);
    setConfigValueForSlider(sliderLedSpeed, DEFAULT_SPEED, "led.speed");

    // LED Colour Sliders
    std::array<float, 3> rgbValues = getRgbValues();
    sliderLedRed = createSlider(_("RED"), 0.f, 255.f, 10.f, "", "", (isH700 || isA133 || isA523));
    sliderLedRed->setValue(rgbValues[0]);
    sliderLedGreen = createSlider(_("GREEN"), 0.f, 255.f, 10.f, "", "", (isH700 || isA133 || isA523));
    sliderLedGreen->setValue(rgbValues[1]);
    sliderLedBlue = createSlider(_("BLUE"), 0.f, 255.f, 10.f, "", "", (isH700 || isA133 || isA523));
    sliderLedBlue->setValue(rgbValues[2]);
    addEntry(_("RESTORE DEFAULT COLORS"), true, [this] { restoreDefaultColors(); });

    addGroup(_("BATTERY CHARGE INDICATION"));

    // Low battery threshold slider
    sliderLowBatteryThreshold = createSlider(_("LOW BATTERY THRESHOLD"), 0.f, 100.f, 5.f, "%", _("Show yellow/red breathing when battery is below this threshold. Set to 0 to disable."), (isH700 || isA133 || isA523));
    setConfigValueForSlider(sliderLowBatteryThreshold, DEFAULT_LOW_BATTERY_THRESHOLD, "led.battery.low");
    switchBatteryCharging = createSwitch(_("BATTERY CHARGING"), "led.battery.charging", _("Show green breathing while device is charging."), true, false, (isH700 || isA133 || isA523));


    addGroup(_("RETRO ACHIEVEMENT INDICATION"));
    switchRetroAchievements = createSwitch(_("ACHIEVEMENT EFFECT"), "led.retroachievements", _("Honor your retro achievements with a LED effect."), true, false, (isH700 || isA133 || isA523));

    initializeOnChangeListeners();
    applyValues();
    addSaveFunc([this] {
        // Read all variables from the respective UI elements and set the respective values in batocera.conf
        SystemConf::getInstance()->set("led.mode", optionListMode->getSelected());
        SystemConf::getInstance()->set("led.brightness", std::to_string((int) sliderLedBrightness->getValue()));
        SystemConf::getInstance()->set("led.brightness.adaptive", (switchAdaptiveBrightness->getState() ? "1" : "0"));
        SystemConf::getInstance()->set("led.speed", std::to_string((int) sliderLedSpeed->getValue()));
        setRgbValues(sliderLedRed->getValue(), sliderLedGreen->getValue(), sliderLedBlue->getValue());
        SystemConf::getInstance()->set("led.battery.low", std::to_string((int) sliderLowBatteryThreshold->getValue()));
        SystemConf::getInstance()->set("led.battery.charging", (switchBatteryCharging->getState() ? "1" : "0"));
        SystemConf::getInstance()->set("led.retroachievements", (switchRetroAchievements->getState() ? "1" : "0"));
		SystemConf::getInstance()->saveSystemConf();
		Scripting::fireEvent(MENU_EVENT_NAME);

        // Reactivate the RGB Service
        LegacyRgbService::start();
    });

}

// Creates a new mode option list
std::shared_ptr<OptionListComponent<std::string>> LegacyGuiRgbSettings::createModeOptionList()
{
    auto optionsLedMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MODE"), false);

    std::string selectedLedMode = SystemConf::getInstance()->get("led.mode");
    if (selectedLedMode.empty() || !isSupportedMode(selectedLedMode))
        selectedLedMode = DEFAULT_LED_MODE;

    optionsLedMode->add(_("NONE"), "0", selectedLedMode == "0");
    if (isH700 || isA133 || isA523) {
        optionsLedMode->add(_("STATIC"), "1", selectedLedMode == "1");
    } else if (selectedLedMode == "1") {
        selectedLedMode = DEFAULT_LED_MODE;
    }
    if (isH700) {
        optionsLedMode->add(_("BREATHING (FAST)"), "2", selectedLedMode == "2");
    } else if (selectedLedMode == "2") {
        selectedLedMode = DEFAULT_LED_MODE;
    }
    if (isH700 || isA133 || isA523) {
        optionsLedMode->add(_("BREATHING (MEDIUM)"), "3", selectedLedMode == "3");
    } else if (selectedLedMode == "3") {
        selectedLedMode = DEFAULT_LED_MODE;
    }
    if (isH700) {
        optionsLedMode->add(_("BREATHING (SLOW)"), "4", selectedLedMode == "4");
    } else if (selectedLedMode == "4") {
        selectedLedMode = DEFAULT_LED_MODE;
    }
    if (isH700 || isA133 || isA523) {
        optionsLedMode->add(_("SINGLE RAINBOW"), "5", selectedLedMode == "5");
    } else if (selectedLedMode == "5") {
        selectedLedMode = DEFAULT_LED_MODE;
    }
    if (isH700) {
        optionsLedMode->add(_("MULTI RAINBOW"), "6", selectedLedMode == "6");
    } else if (selectedLedMode == "6") {
        selectedLedMode = DEFAULT_LED_MODE;
    }

    addWithDescription(_("MODE"), _("Set the default LED animation. (Not all of the settings below are applicable to every mode.)"), optionsLedMode);
    return optionsLedMode;
}


// Retrieves RGB value settings from batocera.conf as an array of floats
std::array<float, 3> LegacyGuiRgbSettings::getRgbValues()
{
    std::string colour = SystemConf::getInstance()->get("led.colour");
    if (colour.empty()) {
        return {DEFAULT_COLOR_RED, DEFAULT_COLOR_GREEN, DEFAULT_COLOR_BLUE};
    }

    std::vector<std::string> rgbValues;
    std::stringstream stringStream(colour);
    std::string item;

    while (getline(stringStream, item, RGB_DELIMITER)) {
        rgbValues.push_back(item);
    }

    int red = std::stoi(rgbValues[0]);
    int green = std::stoi(rgbValues[1]);
    int blue = std::stoi(rgbValues[2]);

    return { static_cast<float>(red), static_cast<float>(green), static_cast<float>(blue) };
}

// Concatenates the RGB values and stores them in batocera.conf.
void LegacyGuiRgbSettings::setRgbValues(float red, float green, float blue)
{
    std::string colour = std::to_string((int) red) + RGB_DELIMITER + std::to_string((int) green) + RGB_DELIMITER + std::to_string((int) blue);
    SystemConf::getInstance()->set("led.colour", colour);
}

void LegacyGuiRgbSettings::initializeOnChangeListeners()
{
        optionListMode->setSelectedChangedCallback([this](std::string value) { applyValues(); });
        sliderLedBrightness->setOnValueChanged([this](float value) { applyValues(); });
        sliderLedSpeed->setOnValueChanged([this](float value) { applyValues(); });
        sliderLedRed->setOnValueChanged([this](float value) { applyValues(); });
        sliderLedGreen->setOnValueChanged([this](float value) { applyValues(); });
        sliderLedBlue->setOnValueChanged([this](float value) { applyValues(); });
}

void LegacyGuiRgbSettings::applyValues()
{
    std::string selectedMode = optionListMode->getSelected();
    int selectedBrightness = (int) sliderLedBrightness->getValue();
    int selectedSpeed = (int) sliderLedSpeed->getValue();
    int selectedRed = (int) sliderLedRed->getValue();
    int selectedGreen = (int) sliderLedGreen->getValue();
    int selectedBlue = (int) sliderLedBlue->getValue();
    LegacyRgbService::setRgb(std::stoi(selectedMode), selectedBrightness, selectedSpeed, selectedRed, selectedGreen, selectedBlue);
}

void LegacyGuiRgbSettings::restoreDefaultColors()
{
    sliderLedRed->setValue(DEFAULT_COLOR_RED);
    sliderLedGreen->setValue(DEFAULT_COLOR_GREEN);
    sliderLedBlue->setValue(DEFAULT_COLOR_BLUE);
    applyValues();
}

bool LegacyGuiRgbSettings::isSupportedMode(const std::string& mode) {
    std::vector<std::string> modes = {"0", "1", "2", "3", "4", "5", "6"};
    if (std::any_of(modes.begin(), modes.end(), [&](const std::string& s) { return s == mode; })) {
        return true;
    }
    return false;
}
