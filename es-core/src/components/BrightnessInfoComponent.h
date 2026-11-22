#pragma once

#include "components/VolumeInfoComponent.h"
#include <cstdint>

// The volume popup, next to it, for the screen backlight: /sys/class/backlight, or the
// Allwinner display driver (/dev/disp) on boards without it, like knulli-brightness.
// Only changes made by the user show it: not the level found at start, and not
// the battery saver dimming or turning off the screen, or restoring it.
class BrightnessInfoComponent : public VolumeInfoComponent
{
public:
	BrightnessInfoComponent(Window* window);
	~BrightnessInfoComponent();

protected:
	int readLevel() override;
	std::string glyph(int level) override;
	bool showChange() override;

private:
	std::string mBrightnessPath;
	int mDispFd;
	int mMaxBrightness;
	int mLastLevel;

	int64_t mLastReadTime;
	int64_t mQuietUntil;
};
