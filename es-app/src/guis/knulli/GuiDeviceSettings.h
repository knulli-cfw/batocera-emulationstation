#pragma once
#include "guis/GuiSettings.h"

class GuiDeviceSettings : public GuiSettings
{
public:
        GuiDeviceSettings(Window* window);

private:
        void openPowerManagementSettings();
        void openRgbLedSettings();
        void installPico8();
        std::shared_ptr<OptionListComponent<std::string>> createUsbModeOptionList();

        std::shared_ptr<OptionListComponent<std::string>> optionsUsbMode;
};
