#pragma once
#include "guis/GuiSettings.h"

class GuiTools : public GuiSettings
{
public:
        GuiTools(Window* window);

private:
        void openPowerManagementSettings();
};
