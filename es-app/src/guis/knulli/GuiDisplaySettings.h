#pragma once
#include "ExtendedGuiSettings.h"
#include <vector>
#include <string>

class GuiDisplaySettings : public ExtendedGuiSettings
{
public:
        GuiDisplaySettings(Window* window);

private:
        std::vector<std::string> sliderLabels;
        std::vector<std::shared_ptr<SliderComponent>> sliders;

};
