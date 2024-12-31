#pragma once
#include "guis/GuiSettings.h"

class GuiRgbSettings : public GuiSettings
{
public:
        GuiRgbSettings(Window* window);
private:
        std::shared_ptr<SliderComponent> addSlider(std::string label, float min, float max, float step, std::string unit, std::string description);
        void setDefaultValueForSlider(std::shared_ptr<SliderComponent> slider, float defaultValue, std::string settingsID);
        std::shared_ptr<OptionListComponent<std::string>> addOptionList(const std::string& title, const std::vector<std::pair<std::string, std::string>>& values, const std::string& settingsID, bool storeInSettings, const std::function<void()>& onChanged);

};
