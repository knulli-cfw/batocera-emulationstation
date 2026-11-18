#pragma once
#ifndef ES_CORE_COMPONENTS_SLIDESHOW_COMPONENT_H
#define ES_CORE_COMPONENTS_SLIDESHOW_COMPONENT_H

#include "GuiComponent.h"
#include "math/Vector4f.h"
#include <memory>
#include <string>
#include <vector>

class TextureResource;

// <slideshow>: captures shown one at a time at whole-pixel scales (SlideLayout::fit). In a grid's item
// template it moves only on the selected item: every <interval> seconds the next capture slides in from
// the right in <duration> seconds, with dots for the position; the other items show the first capture.
// <path> lists the captures, one per line ("{system:slides}" on the system grid).
class SlideshowComponent : public GuiComponent
{
public:
	SlideshowComponent(Window* window);
	~SlideshowComponent();

	std::string getThemeTypeName() override { return "slideshow"; }

	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void onHide() override;
	void onScreenSaverActivate() override;
	void onScreenSaverDeactivate() override;

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

	void setPaths(const std::string& paths);

private:
	bool isActive();
	void rewind();
	std::shared_ptr<TextureResource> loadTexture(const std::string& path);
	void drawSlide(const std::shared_ptr<TextureResource>& texture, float dx, const Transform4x4f& trans);
	void drawDots(int shown, const Transform4x4f& trans);

	std::string mPathsValue;
	std::vector<std::string> mPaths;

	int mCurrent;
	int mElapsed;
	float mProgress;	// of the slide to the next capture, 0 to 1; -1 while waiting
	bool mScreenSaverActive;

	std::shared_ptr<TextureResource> mTexture;
	std::shared_ptr<TextureResource> mNextTexture;

	int mInterval;
	int mDuration;
	unsigned int mColor;
	float mSaturation;
	float mRoundCorners;
	Vector4f mRoi;

	bool mDots;
	int mMaxDots;
	float mDotSize;
	Vector2f mDotsOffset;
	unsigned int mDotColor;
	unsigned int mDotSelectedColor;
};

#endif // ES_CORE_COMPONENTS_SLIDESHOW_COMPONENT_H
