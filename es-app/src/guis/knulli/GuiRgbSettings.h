#pragma once
#include "ExtendedGuiSettings.h"
#include "components/SliderComponent.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include <array>
#include <memory>

class GuiRgbSettings : public ExtendedGuiSettings
{
public:
    GuiRgbSettings(Window* window);

private:
    std::shared_ptr<OptionListComponent<std::string>> createModeOptionList();

    std::array<float, 3> getRgbValues();
    void setRgbValues(float red, float green, float blue);
    void initializeOnChangeListeners();
    void applyValues();
    void restoreDefaultColors();

    bool isH700;
    bool isA133;
    std::shared_ptr<OptionListComponent<std::string>> optionListMode;
    std::shared_ptr<SliderComponent> sliderLedBrightness;
    std::shared_ptr<SliderComponent> sliderLedSpeed;
    std::shared_ptr<SliderComponent> sliderLedRed;
    std::shared_ptr<SliderComponent> sliderLedGreen;
    std::shared_ptr<SliderComponent> sliderLedBlue;
    std::shared_ptr<SwitchComponent> switchAdaptiveBrightness;
    std::shared_ptr<SliderComponent> sliderLowBatteryThreshold;
    std::shared_ptr<SwitchComponent> switchBatteryCharging;
    std::shared_ptr<SwitchComponent> switchRetroAchievements;

};