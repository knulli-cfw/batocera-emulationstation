#pragma once
#include "guis/knulli/ExtendedGuiSettings.h"
#include "components/SliderComponent.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include <array>
#include <memory>

class SilkyGuiRgbSettings : public ExtendedGuiSettings
{
public:
    SilkyGuiRgbSettings(Window* window);

private:
    
    std::shared_ptr<OptionListComponent<std::string>> createModeOptionList();
    std::shared_ptr<OptionListComponent<std::string>> createPaletteOptionList(const std::string& configKey, const std::string& title, const std::string& description);
    std::shared_ptr<OptionListComponent<std::string>> createPaletteModOptionList();
    std::shared_ptr<OptionListComponent<std::string>> createBatteryIndicationOptionList(const std::string& configKey, const std::string& title, const std::string& description);
    void applyValue(const std::string& key, const std::string& value);
    bool hasRequiredSetting(std::string setting);

    std::vector<std::string> requiredSettings;

    std::shared_ptr<OptionListComponent<std::string>> optionListMode;
    std::shared_ptr<OptionListComponent<std::string>> optionListPalettePrimary;
    std::shared_ptr<OptionListComponent<std::string>> optionListBatteryCharging;
    std::shared_ptr<OptionListComponent<std::string>> optionListBatteryLow;
    std::shared_ptr<OptionListComponent<std::string>> optionListPaletteMod;

    std::shared_ptr<SliderComponent> sliderLedBrightness;
    std::shared_ptr<SliderComponent> sliderLedSpeed;
    std::shared_ptr<SwitchComponent> switchAdaptiveBrightness;
    std::shared_ptr<SwitchComponent> switchPaletteInvert;
    std::shared_ptr<SwitchComponent> switchPaletteInvertSecondary;
    std::shared_ptr<SliderComponent> sliderLowBatteryThreshold;
    std::shared_ptr<SwitchComponent> switchBatteryCharging;
    std::shared_ptr<SwitchComponent> switchRetroAchievements;

};
