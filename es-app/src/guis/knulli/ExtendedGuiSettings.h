#pragma once

#include "guis/GuiSettings.h"

#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"

class ExtendedGuiSettings : public GuiSettings
{
public:
    ExtendedGuiSettings(Window* window, const char* title);

    std::shared_ptr<SliderComponent> createSlider(std::string label, float min, float max, float step, std::string unit, std::string description, bool show);
    void setConfigValueForSlider(std::shared_ptr<SliderComponent> slider, float defaultValue, std::string settingsID);
    std::shared_ptr<SwitchComponent> createSwitch(std::string label, std::string variable, std::string description, bool defaultValue, bool hasAuto, bool show);
};
