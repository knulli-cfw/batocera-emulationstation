#include "guis/knulli/GuiRgbSettings.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
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

constexpr const char* MENU_EVENT_NAME = "rgb-changed";

constexpr char RGB_DELIMITER = ' ';
constexpr const char* DEFAULT_LED_MODE = "1";
constexpr float DEFAULT_COLOR_RED = 148;
constexpr float DEFAULT_COLOR_GREEN = 255;
constexpr float DEFAULT_COLOR_BLUE = 0;
constexpr float DEFAULT_BRIGHTNESS = 100;
constexpr float DEFAULT_SPEED = 15;
constexpr float DEFAULT_LOW_BATTERY_THRESHOLD = 20;
constexpr const char* DEFAULT_SWITCH_ON = "1";

GuiRgbSettings::GuiRgbSettings(Window* window) : GuiSettings(window, _("RGB LED SETTINGS").c_str())
{
    addGroup(_("REGULAR LED MODE AND COLOR"));

    // LED Mode Options
    optionListMode = createModeOptionList();

    // LED Brightness Slider
    sliderLedBrightness = createSlider("BRIGHTNESS", 0.f, 255.f, 1.f, "", "");
    setConfigValueForSlider(sliderLedBrightness, DEFAULT_BRIGHTNESS, "led.brightness");

    // LED Speed Slider
    sliderLedSpeed = createSlider("SPEED", 1.f, 255.f, 1.f, "", "Not applicable for all devices/modes. Warning: High speed may cause seizures for people with photosensitive epilepsy.");
    setConfigValueForSlider(sliderLedSpeed, DEFAULT_SPEED, "led.speed");

    // LED Colour Sliders
    std::array<float, 3> rgbValues = getRgbValues();
    sliderLedRed = createSlider("RED", 0.f, 255.f, 1.f, "", "");
    sliderLedRed->setValue(rgbValues[0]);
    sliderLedGreen = createSlider("GREEN", 0.f, 255.f, 1.f, "", "");
    sliderLedGreen->setValue(rgbValues[1]);
    sliderLedBlue = createSlider("BLUE", 0.f, 255.f, 1.f, "", "");
    sliderLedBlue->setValue(rgbValues[2]);

    addGroup(_("BATTERY CHARGE INDICATION"));

    // Low battery threshold slider
    sliderLowBatteryThreshold = createSlider("LOW BATTERY THRESHOLD", 1.f, 100.f, 5.f, "%", "Show yellow/red breathing when battery is below this threshold. Set to 0 to disable.");
    setConfigValueForSlider(sliderLowBatteryThreshold, DEFAULT_LOW_BATTERY_THRESHOLD, "led.battery.low");
    optionListBatteryCharging = createSwitch("BATTERY CHARGING", "led.battery.charging", "Show green breathing while device is charging.");

    addGroup(_("RETRO ACHIEVEMENT INDICATION"));
    optionListRetroAchievements = createSwitch("ACHIEVEMENT EFFECT", "led.retroachievements", "Honor your retro achievements with a LED effect.");

    addSaveFunc([this] {
        SystemConf::getInstance()->set("led.mode", optionListMode->getSelected());
        SystemConf::getInstance()->set("led.brightness", std::to_string((int) sliderLedBrightness->getValue()));
        SystemConf::getInstance()->set("led.speed", std::to_string((int) sliderLedSpeed->getValue()));
        setRgbValues(sliderLedRed->getValue(), sliderLedGreen->getValue(), sliderLedBlue->getValue());
        SystemConf::getInstance()->set("led.battery.low", std::to_string((int) sliderLowBatteryThreshold->getValue()));
        SystemConf::getInstance()->set("led.battery.charging", optionListBatteryCharging->getSelected());
        SystemConf::getInstance()->set("led.retroachievements", optionListRetroAchievements->getSelected());
		SystemConf::getInstance()->saveSystemConf();
		Scripting::fireEvent(MENU_EVENT_NAME);
    });
}

std::shared_ptr<OptionListComponent<std::string>> GuiRgbSettings::createModeOptionList()
{
    auto optionsLedMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MODE"), false);

    std::string selectedLedMode = SystemConf::getInstance()->get("led.mode");
    if (selectedLedMode.empty())
        selectedLedMode = DEFAULT_LED_MODE;

    optionsLedMode->add(_("NONE"), "0", selectedLedMode == "0");
    optionsLedMode->add(_("STATIC"), "1", selectedLedMode == "1");
    optionsLedMode->add(_("BREATHING (FAST)"), "2", selectedLedMode == "2");
    optionsLedMode->add(_("BREATHING (MEDIUM)"), "3", selectedLedMode == "3");
    optionsLedMode->add(_("BREATHING (SLOW)"), "4", selectedLedMode == "4");
    optionsLedMode->add(_("SINGLE RAINBOW"), "5", selectedLedMode == "5");
    optionsLedMode->add(_("MULTI RAINBOW"), "6", selectedLedMode == "6");

    addWithDescription(_("MODE"), _("Not every mode is available on every device."), optionsLedMode);
    return optionsLedMode;
}

std::shared_ptr<SliderComponent> GuiRgbSettings::createSlider(std::string label, float min, float max, float step, std::string unit, std::string description)
{
    std::shared_ptr<SliderComponent> slider = std::make_shared<SliderComponent>(mWindow, min, max, step, unit);
    if (description.empty()) {
        addWithLabel(label, slider);
    } else {
        addWithDescription(label, description, slider);
    }
    return slider;
}

void GuiRgbSettings::setConfigValueForSlider(std::shared_ptr<SliderComponent> slider, float defaultValue, std::string variable) {
    float selectedValue = defaultValue;
    std::string configuredValue = SystemConf::getInstance()->get(variable);
    if (!configuredValue.empty()) {
        selectedValue = Utils::String::toFloat(configuredValue);
    }
    slider->setValue(selectedValue);
}

std::shared_ptr<OptionListComponent<std::string>> GuiRgbSettings::createSwitch(std::string label, std::string variable, std::string description) {
    std::shared_ptr<OptionListComponent<std::string>> optionList = std::make_shared<OptionListComponent<std::string>>(mWindow, label, false);

    std::string selected = SystemConf::getInstance()->get(variable);
    if (selected.empty())
        selected = DEFAULT_SWITCH_ON;

    optionList->add(_("OFF"), "0", selected == "0");
    optionList->add(_("ON"), "1", selected == "1");

    addWithDescription(label, description, optionList);

    return optionList;
}

std::array<float, 3> GuiRgbSettings::getRgbValues() {
    std::string colour = SystemConf::getInstance()->get("led.colour");
    if (colour.empty()) {
        return {0, 0, 0};
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

    return {red, green, blue};
}

void GuiRgbSettings::setRgbValues(float red, float green, float blue) {
    std::string colour = std::to_string((int) red) + RGB_DELIMITER + std::to_string((int) green) + RGB_DELIMITER + std::to_string((int) blue);
    SystemConf::getInstance()->set("led.colour", colour);
}