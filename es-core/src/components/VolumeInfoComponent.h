#pragma once

#include <mutex>
#include "GuiComponent.h"

class ComponentGrid;
class NinePatchComponent;
class TextComponent;
class Window;

class VolumeInfoComponent : public GuiComponent
{
public:
	VolumeInfoComponent(Window* window, bool actionLine = true);
	~VolumeInfoComponent();

	void render(const Transform4x4f& parentTrans) override;
	void update(int deltaTime) override;

	void reset() { mVolume = -1; }

protected:
	// Level shown by the popup, in percent, or -1 when there is none
	virtual int readLevel();
	// False to take a new level without showing the popup
	virtual bool showChange() { return true; }
	// Horizontal place of the popup: 0 is the top left corner, 1 is next to it
	void moveToSlot(int slot);

private:
	NinePatchComponent* mFrame;
	TextComponent*		mLabel;

	int mVolume;

	int mCheckTime;
	int mDisplayTime;

	unsigned int mFillColor;
	int mTrackDivider;
};