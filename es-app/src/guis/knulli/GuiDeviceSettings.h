#pragma once
#include "guis/knulli/ExtendedGuiSettings.h"

class GuiDeviceSettings : public ExtendedGuiSettings
{
public:
        GuiDeviceSettings(Window* window);

private:
        void openPowerManagementSettings();
        void openRgbLedSettings();
        void installPico8();

        std::shared_ptr<OptionListComponent<std::string>> createUsbModeOptionList();
        std::shared_ptr<OptionListComponent<std::string>> createToggleSwitchModeOptionList();

        std::shared_ptr<OptionListComponent<std::string>> optionsUsbMode;
        std::shared_ptr<OptionListComponent<std::string>> optionsToggleSwitchMode;

        std::shared_ptr<SwitchComponent> switchTelemetryStatistics;

};
