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

GuiRgbSettings::GuiRgbSettings(Window* window) : GuiSettings(window, _("RGB LED SETTINGS").c_str())
{
    addGroup(_("REGULAR LED MODE AND COLOR"));

    // LED Mode Options
    auto optionsLedMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MODE"), false);

    std::string selectedLedMode = SystemConf::getInstance()->get("led.mode");
    if (selectedLedMode.empty())
        selectedLedMode = "1";

    optionsLedMode->add(_("NONE"),               "0", selectedLedMode == "0");
    optionsLedMode->add(_("STATIC"),             "1", selectedLedMode == "1");
    optionsLedMode->add(_("BREATHING (FAST)"),   "2", selectedLedMode == "2");
    optionsLedMode->add(_("BREATHING (MEDIUM)"), "3", selectedLedMode == "3");
    optionsLedMode->add(_("BREATHING (SLOW)"),   "4", selectedLedMode == "4");
    optionsLedMode->add(_("SINGLE RAINBOW"),     "5", selectedLedMode == "5");
    optionsLedMode->add(_("MULTI RAINBOW"),      "6", selectedLedMode == "6");

    addWithDescription(_("MODE"), _("Not every mode is available on every device."), optionsLedMode);

    // LED Brightness Slider
    auto sliderLedBrightness = createSlider("BRIGHTNESS", 0.f, 255.f, 1.f, "", "");
    setDefaultValueForSlider(sliderLedBrightness, 100.f, "led.brightness");

    // LED Speed Slider
    auto sliderLedSpeed = createSlider("SPEED", 1.f, 255.f, 1.f, "", "Not applicable for all devices/modes. Warning: High speed may cause seizures for people with photosensitive epilepsy.");
    setDefaultValueForSlider(sliderLedSpeed, 15.f, "led.speed");

    // LED Colour Sliders
    auto sliderLedRed = createSlider("RED", 1.f, 255.f, 1.f, "", "");
    auto sliderLedGreen = createSlider("GREEN", 1.f, 255.f, 1.f, "", "");
    auto sliderLedBlue = createSlider("BLUE", 1.f, 255.f, 1.f, "", "");

    addGroup(_("BATTERY CHARGE INDICATION"));

    // Low battery threshold slider
    auto sliderLowBatteryThreshold = createSlider("LOW BATTERY THRESHOLD", 1.f, 100.f, 5.f, "%", "Show yellow/red breathing when battery is below this threshold. Set to 0 to disable.");
    setDefaultValueForSlider(sliderLowBatteryThreshold, 15.f, "led.battery.low");
    createSwitch("BATTERY CHARGING", "led.battery.charging", "Show green breathing while device is charging.");

    addGroup(_("RETRO ACHIEVEMENT INDICATION"));
    createSwitch("ACHIEVEMENT EFFECT", "led.retroachievements", "Honor your retro achievements with a LED effect.");
}

std::shared_ptr<SliderComponent> GuiRgbSettings::createSlider(std::string label, float min, float max, float step, std::string unit, std::string description) {
    std::shared_ptr<SliderComponent> slider = std::make_shared<SliderComponent>(mWindow, min, max, step, unit);
    if (description.empty()) {
        addWithLabel(label, slider);
    } else {
        addWithDescription(label, description, slider);
    }
    return slider;
}

void GuiRgbSettings::setDefaultValueForSlider(std::shared_ptr<SliderComponent> slider, float defaultValue, std::string variable) {
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
        selected = "1";

    optionList->add(_("OFF"), "0", selected == "0");
    optionList->add(_("ON"), "1", selected == "1");

    addWithDescription(label, description, optionList);

    return optionList;
}