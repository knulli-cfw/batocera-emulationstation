#include "BrightnessInfoComponent.h"
#include "LocaleES.h"
#include "math/Misc.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <SDL_timer.h>

#ifndef WIN32
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#endif

// knulli-brightness keeps the user's level in this file while "dim" or "dispoff"
// is in effect, and removes it once "undim" or "dispon" has restored the level
#define DIMMED_MARKER		"/var/run/knulli-brightness"

// Changes are taken silently while dimmed, during this time after it, and after a gap
// between two reads (ES stopped by the battery saver, system suspended, game running)
#define QUIET_TIME			1500
#define MAX_READ_GAP		1000

// sunxi display driver, as used by /usr/bin/brightness (levels 0-255)
#define SUNXI_DISP_DEVICE			"/dev/disp"
#define DISP_LCD_GET_BRIGHTNESS		0x103
#define SUNXI_MAX_BRIGHTNESS		255

static int64_t clockMs()
{
#ifdef WIN32
	return SDL_GetTicks();
#else
	// Unlike SDL_GetTicks, CLOCK_BOOTTIME keeps counting while the system is suspended
	struct timespec ts;
	clock_gettime(CLOCK_BOOTTIME, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

BrightnessInfoComponent::BrightnessInfoComponent(Window* window)
	: VolumeInfoComponent(window), mDispFd(-1), mMaxBrightness(0), mLastLevel(-1), mLastReadTime(0), mQuietUntil(0)
{
	moveToSlot(1);

#ifndef WIN32
	for (auto dir : Utils::FileSystem::getDirContent("/sys/class/backlight"))
	{
		std::string brightnessPath = dir + "/brightness";
		std::string maxBrightnessPath = dir + "/max_brightness";

		if (access(brightnessPath.c_str(), R_OK) != 0 || access(maxBrightnessPath.c_str(), R_OK) != 0)
			continue;

		int maxBrightness = Utils::String::toInteger(Utils::FileSystem::readAllText(maxBrightnessPath));
		if (maxBrightness <= 0)
			continue;

		mBrightnessPath = brightnessPath;
		mMaxBrightness = maxBrightness;
		break;
	}

	if (mBrightnessPath.empty())
	{
		mDispFd = open(SUNXI_DISP_DEVICE, O_RDWR | O_CLOEXEC);
		if (mDispFd >= 0)
			mMaxBrightness = SUNXI_MAX_BRIGHTNESS;
	}
#endif
}

BrightnessInfoComponent::~BrightnessInfoComponent()
{
#ifndef WIN32
	if (mDispFd >= 0)
		close(mDispFd);
#endif
}

std::string BrightnessInfoComponent::glyph(int level)
{
	return _U("\uF185"); // a sun
}

int BrightnessInfoComponent::readLevel()
{
	if (mBrightnessPath.empty() && mDispFd < 0)
		return -1;

	int64_t now = clockMs();

#ifndef WIN32
	if (access(DIMMED_MARKER, F_OK) == 0)
		mQuietUntil = now + QUIET_TIME;
#endif

	if (mLastReadTime != 0 && now - mLastReadTime > MAX_READ_GAP)
		mQuietUntil = now + QUIET_TIME;

	mLastReadTime = now;

	int value = 0;
	if (!mBrightnessPath.empty())
		value = Utils::String::toInteger(Utils::FileSystem::readAllText(mBrightnessPath));
#ifndef WIN32
	else
	{
		unsigned long args[3] = { 0, 0, 0 }; // screen 0
		value = ioctl(mDispFd, DISP_LCD_GET_BRIGHTNESS, args);
		if (value < 0)
			return mLastLevel; // keep the last level on a failed read
	}
#endif

	if (value < 0)
		value = 0;

	// Same rounding as knulli-brightness
	mLastLevel = Math::min(100, (value * 100 + mMaxBrightness / 2) / mMaxBrightness);
	return mLastLevel;
}

bool BrightnessInfoComponent::showChange()
{
	return clockMs() >= mQuietUntil;
}
