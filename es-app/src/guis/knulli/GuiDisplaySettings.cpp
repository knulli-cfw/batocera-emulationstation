#include "guis/knulli/GuiDisplaySettings.h"
#include "guis/knulli/syscalls/DisplaySettings.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "Scripting.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"
#include "CapabilityCheck.h"
#include <cctype>
#include <vector>
#include <string>

const std::string CONF_PREFIX = "system.display.";

GuiDisplaySettings::GuiDisplaySettings(Window* window) : ExtendedGuiSettings(window, _("DISPLAY SETTINGS").c_str())
{

	std::vector<std::string> capabilities = DisplaySettings::getCapabilities();
	if (capabilities.size() == 0)
		return;

	for (const std::string& capability : capabilities)
	{

		// Let's not make sliders for empty capability names
		if (capability.empty())
			continue;

        // make an uppercase copy safely
        std::string label = capability;
        std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c){ return std::toupper(c); });
		std::shared_ptr<SliderComponent> slider = createSlider(_(label.c_str()), 0.f, 100.f, 5.f, "", _(""), true);
		setConfigValueForSlider(slider, 50.f, CONF_PREFIX + capability);

		// on change, set the value via DisplaySettings syscall
		slider->setOnValueChanged([this, capability](float value) {
			DisplaySettings::set(capability, (int)value);
		});

		// store sliders and labels for save function
		sliderLabels.push_back(capability);
		sliders.push_back(slider);
	}

	addSaveFunc([this]
	{
		for (size_t i = 0; i < sliders.size(); i++)
		{
			float value = sliders[i]->getValue();
			SystemConf::getInstance()->set(CONF_PREFIX + sliderLabels[i], std::to_string((int)value));
		}
		SystemConf::getInstance()->saveSystemConf();
	});

}
