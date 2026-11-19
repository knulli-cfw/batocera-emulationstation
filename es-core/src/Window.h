#pragma once
#ifndef ES_CORE_WINDOW_H
#define ES_CORE_WInDOW_H

#include "HelpPrompt.h"
#include "InputConfig.h"
#include "Settings.h"
#include "math/Vector2f.h"
#include "math/Vector2i.h"
#include <memory>
#include <functional>

class FileData;
class Font;
class GuiComponent;
class HelpComponent;
class ImageComponent;
class InputConfig;
class TextCache;
class Transform4x4f;
struct HelpStyle;
class TextureResource;
class GuiInfoPopup;
class AsyncNotificationComponent;
class ThemeData;
class TextComponent;
class ControllerActivityComponent;
class VolumeInfoComponent;
class BatteryIndicatorComponent;
class Splash;
class IBindable;

class Window
{
public:
	class ScreenSaver {
	public:
		virtual void startScreenSaver() = 0;
		virtual void stopScreenSaver() = 0;
		virtual void nextVideo() = 0;
		virtual void renderScreenSaver() = 0;
		virtual bool allowSleep() = 0;
		virtual void update(int deltaTime) = 0;
		virtual bool isScreenSaverActive() = 0;
		virtual FileData* getCurrentGame() = 0;
		virtual void launchGame() = 0;
		virtual void resetCounts() = 0;
	};

	Window();
	~Window();

	void pushGui(GuiComponent* gui);
	void displayNotificationMessage(std::string message, int duration = -1); 
	void removeGui(GuiComponent* gui);
	GuiComponent* peekGui();
	inline int getGuiStackSize() { return (int)mGuiStack.size(); }

	void textInput(const char* text);
	void input(InputConfig* config, Input input);
	void update(int deltaTime);
	void render();

	bool init(bool initRenderer = true, bool initInputManager = true);
	void deinit(bool deinitRenderer = true);

	void normalizeNextUpdate();

	inline bool isSleeping() const { return mSleeping; }
	unsigned int getTimeSinceLastInput() const { return mTimeSinceLastInput; }

	// The home and the game lists, with nothing over them, draw a frame only when it changes
	// (Settings "RenderOnChange")
	bool drawsOnlyChanges();

	// Idle pause: after "IdlePause" seconds without input (0: never), the views' videos stop at the end of
	// their loop ("IdlePauseVideo") and their slideshows stop moving ("IdlePauseSlides"); input resumes them
	enum IdlePart { IDLE_VIDEO, IDLE_SLIDES };
	bool isIdlePaused(IdlePart part) const { return part == IDLE_VIDEO ? mIdleVideo : mIdleSlides; }
	bool getAllowSleep();
	void setAllowSleep(bool sleep);
	
	// Splash screen
	std::string getCustomSplashScreenImage();
	void setCustomSplashScreen(std::string imagePath, std::string customText, IBindable* bindable = nullptr);
	void renderSplashScreen(std::string text, float percent = -1, float opacity = 1);
	void renderSplashScreen(float opacity = 1, bool swapBuffers = true);
	void closeSplashScreen();

	void renderHelpPromptsEarly(const Transform4x4f& transform); // used to render HelpPrompts before a fade
	void setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style);

	void setScreenSaver(ScreenSaver* screenSaver) { mScreenSaver = screenSaver; }

	void stopNotificationPopups();

	void startScreenSaver();
	bool cancelScreenSaver();
	void renderScreenSaver();

	// Back from a system suspend (the time asleep counts in CLOCK_BOOTTIME, not in CLOCK_MONOTONIC or SDL's
	// ticks): the screen saver stops and the idle times start over, as on a key, so the view shows at once
	// (the key that woke the system never reaches ES). Called on every turn of the main loop.
	void checkSystemResume();

	void postToUiThread(const std::function<void()>& func, void* data = nullptr);
	void unregisterPostedFunctions(void* data);
	void reactivateGui();

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	std::shared_ptr<BatteryIndicatorComponent>	getBatteryIndicator() { return mBatteryIndicator; }

	AsyncNotificationComponent* createAsyncNotificationComponent(bool actionLine = false);

	bool isCalibratingGun() { return mCalibrationText != nullptr; }
	void setGunCalibrationState(bool isCalibrating);

	// Mouse management
	void processMouseMove(int x, int y, bool touchScreen);
	void processMouseWheel(int delta);	
	bool processMouseButton(int button, bool down, int x, int y);
	void releaseMouseCapture() { mMouseCapture = nullptr; }
	void setMouseCapture(GuiComponent* mouseCapture) { mMouseCapture = mouseCapture; }
	bool hasMouseCapture(GuiComponent* mouseCapture) { return mMouseCapture == mouseCapture; }

private:
	std::vector<GuiComponent*> hitTest(int x, int y);

	void processPostedFunctions();
	void renderSindenBorders();

	std::vector<AsyncNotificationComponent*> mAsyncNotificationComponent;
	void updateAsyncNotifications(int deltaTime);
	void renderAsyncNotifications(const Transform4x4f& trans);
	
	struct PostedFunction
	{
		std::function<void()> func;
		void* container;
	};

	std::vector<PostedFunction> mFunctions;

	std::vector<GuiInfoPopup*> mNotificationPopups;
	void updateNotificationPopups(int deltaTime);
	void layoutNotificationPopups();

	void processNotificationMessages();
	void processSongTitleNotifications();

	void onSleep();
	void onWake();

	HelpComponent*	mHelp;
	ImageComponent* mBackgroundOverlay;
	ScreenSaver*	mScreenSaver;	
	bool			mRenderScreenSaver;

	std::vector<GuiComponent*> mScreenExtras;
	std::vector<GuiComponent*> mGuiStack;

	typedef std::pair<std::string, int> NotificationMessage;
	std::vector<NotificationMessage> mNotificationMessages; 

	std::vector< std::shared_ptr<Font> > mDefaultFonts;
	std::shared_ptr<Splash> mSplash;

	int mFrameTimeElapsed;
	int mFrameCountElapsed;
	int mFrameDrawnStart;
	int mAverageDeltaTime;

	std::unique_ptr<TextCache> mFrameDataText;

	int mClockElapsed;
	std::shared_ptr<TextComponent>	mClock;
	std::shared_ptr<ControllerActivityComponent>	mControllerActivity;
	std::shared_ptr<VolumeInfoComponent> mVolumeInfo;
	std::shared_ptr<VolumeInfoComponent> mBrightnessInfo;
	std::shared_ptr<BatteryIndicatorComponent>	mBatteryIndicator;	

	float mNotificationOffsetY = 0.02f;

	bool mNormalizeNextUpdate;

	bool mAllowSleep;
	bool mSleeping;
	unsigned int mTimeSinceLastInput;
	bool mIdleVideo;
	bool mIdleSlides;
	long long mSuspendedMs;	// CLOCK_BOOTTIME less CLOCK_MONOTONIC at the last check; -1 before the first
	void updateIdlePause();

	bool mRenderedHelpPrompts;

	std::shared_ptr<TextComponent>	mCalibrationText;

	int mTransitionOffset;

	std::shared_ptr<TextureResource> mGunAimTexture;
	std::shared_ptr<TextureResource> mMouseCursorTexture;

	GuiComponent* mMouseCapture;
	Vector2i	  mLastMousePoint;
	int			  mLastShowCursor;

	void renderMenuBackgroundShader();
	void resetMenuBackgroundShader();
	unsigned int mMenuBackgroundShaderTextureCache;
};

#endif // ES_CORE_WINDOW_H
