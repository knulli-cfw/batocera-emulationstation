#pragma once
#ifndef ES_APP_GUIS_GUI_INFO_POPUP_H
#define ES_APP_GUIS_GUI_INFO_POPUP_H

#include "GuiComponent.h"
#include "Window.h"

class ComponentGrid;
class ImageComponent;
class NinePatchComponent;
class TextComponent;

class GuiInfoPopup : public GuiComponent
{
public:
	GuiInfoPopup(Window* window, std::string message, int duration);

	void render(const Transform4x4f& parentTrans) override;	
	void update(int deltaTime) override;

	float getFadeOut() { return mFadeOut; }
	bool isRunning() { return mRunning; };

	// Gives way sooner, another one waiting: at most that much longer
	void shorten(int ms);

	std::string         getMessage() { return mMessage; }

private:
	std::string         mMessage;

	int                 mStartTime;
	int                 mDuration;
	bool                mRunning;	

	float				mFadeOut;

	ComponentGrid*      mGrid;
	NinePatchComponent* mFrame;

	// In a theme's style (Window::PopupStyle): a box with an icon and the text on one line
	bool                mThemed;
	unsigned int        mBackgroundColor;
	unsigned int        mBorderColor;
	float               mCornerRadius;
	std::shared_ptr<ImageComponent> mIcon;
	std::shared_ptr<TextComponent>  mText;
};

#endif // ES_APP_GUIS_GUI_INFO_POPUP_H
