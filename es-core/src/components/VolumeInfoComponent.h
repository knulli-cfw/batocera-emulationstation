#pragma once

#include <mutex>
#include <string>
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

	// Shown (fading out included)
	bool isShowing() const { return mDisplayTime >= 0; }

protected:
	// Level shown by the popup, in percent, or -1 when there is none
	virtual int readLevel();
	// False to take a new level without showing the popup
	virtual bool showChange() { return true; }
	// Horizontal place of the popup: 0 is the top left corner, 1 is next to it (in a theme's style, the window places it)
	void moveToSlot(int slot);
	// In a theme's style: the glyph before the bar for a level
	virtual std::string glyph(int level);

private:
	NinePatchComponent* mFrame;
	TextComponent*		mLabel;

	int mVolume;

	int mCheckTime;
	int mDisplayTime;

	unsigned int mFillColor;
	int mTrackDivider;

	// In a theme's style (Window::PopupStyle): a pill with a glyph, a horizontal bar and the level
	bool mThemed;
	unsigned int mBackgroundColor;
	unsigned int mBorderColor;
	unsigned int mTrackColor;
	float mCornerRadius;
	TextComponent* mGlyph;
	float mBarX;
	float mBarWidth;
};