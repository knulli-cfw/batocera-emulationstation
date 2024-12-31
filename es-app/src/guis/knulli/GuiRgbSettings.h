#pragma once
#include "guis/GuiSettings.h"
#include "components/SliderComponent.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"

class GuiRgbSettings : public GuiSettings
{
public:
    GuiRgbSettings(Window* window);
private:
    std::shared_ptr<SliderComponent> createSlider(std::string label, float min, float max, float step, std::string unit, std::string description);
    void setDefaultValueForSlider(std::shared_ptr<SliderComponent> slider, float defaultValue, std::string settingsID);
    std::shared_ptr<OptionListComponent<std::string>> createSwitch(std::string label, std::string variable, std::string description);
};