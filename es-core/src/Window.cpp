#include "Window.h"

#include "components/HelpComponent.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "resources/TextureResource.h"
#include "InputManager.h"
#include "Log.h"
#include "Scripting.h"
#include <algorithm>
#include <iomanip>
#include "guis/GuiInfoPopup.h"
#include "SystemConf.h"
#include "LocaleES.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include "ThemeData.h"
#include <mutex>
#include <time.h>
#include "components/AsyncNotificationComponent.h"
#include "components/PostProcessShaderComponent.h"
#include "components/ControllerActivityComponent.h"
#include "components/BatteryIndicatorComponent.h"
#include "guis/GuiMsgBox.h"
#include "components/VolumeInfoComponent.h"
#include "components/VideoVlcComponent.h"
#include "components/VideoLeadIn.h"
#include "components/BrightnessInfoComponent.h"
#include "Splash.h"
#include "PowerSaver.h"
#include "renderers/Renderer.h"
#include "BindingManager.h"

#if WIN32
#include <SDL_syswm.h>
#endif

Window::Window() : mNormalizeNextUpdate(false), mFrameTimeElapsed(0), mFrameCountElapsed(0), mFrameDrawnStart(0), mAverageDeltaTime(10),
  mAllowSleep(true), mSleeping(false), mTimeSinceLastInput(0), mIdleVideo(false), mIdleSlides(false), mRenderOnChange(false), mIdlePauseSeconds(0), mSuspendedMs(-1), mScreenSaver(NULL), mRenderScreenSaver(false), mClockElapsed(0), mMouseCapture(nullptr), mMenuBackgroundShaderTextureCache(-1)
{			
	mTransitionOffset = 0;

	mHelp = new HelpComponent(this);
	mBackgroundOverlay = new ImageComponent(this);
	mBackgroundOverlay->setImage(":/scroll_gradient.png"); 

	mSplash = nullptr;
	mLastShowCursor = -2;
}

Window::~Window()
{
	resetMenuBackgroundShader();

	for (auto extra : mScreenExtras)
		delete extra;

	delete mBackgroundOverlay;

	// delete all our GUIs
	while(peekGui())
		delete peekGui();

	delete mHelp;
}

void Window::pushGui(GuiComponent* gui)
{
	resetMenuBackgroundShader();

	if (mGuiStack.size() > 0)
	{
		auto& top = mGuiStack.back();
		top->topWindow(false);		
	}

	hitTest(-1, -1);

	gui->onShow();
	mGuiStack.push_back(gui);
	gui->updateHelpPrompts();
}

void Window::removeGui(GuiComponent* gui)
{
	resetMenuBackgroundShader();

	if (mMouseCapture == gui)
		mMouseCapture = nullptr;

	for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
	{
		if(*i == gui)
		{						
			gui->onHide();
			i = mGuiStack.erase(i);

			if(i == mGuiStack.cend() && mGuiStack.size()) // we just popped the stack and the stack is not empty
			{
				mGuiStack.back()->updateHelpPrompts();
				mGuiStack.back()->topWindow(true);
			}

			return;
		}
	}
}

GuiComponent* Window::peekGui()
{
	if(mGuiStack.size() == 0)
		return NULL;

	return mGuiStack.back();
}

bool Window::init(bool initRenderer, bool initInputManager)
{
	LOG(LogInfo) << "Window::init";

	VideoLeadIn::pause(false);

	if (initRenderer)
	{
		if (!Renderer::init())
		{
			LOG(LogError) << "Renderer failed to initialize!";
			return false;
		}
	}
	else 
		Renderer::activateWindow();

	if (initInputManager)
		InputManager::getInstance()->init();

	ResourceManager::getInstance()->reloadAll();

	//keep a reference to the default fonts, so they don't keep getting destroyed/recreated
	if(mDefaultFonts.empty())
	{
		mDefaultFonts.push_back(Font::get(FONT_SIZE_SMALL));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_MEDIUM));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_LARGE));
	}

	// The theme's fade behind menus (init runs again after each game): the built-in one until a theme sets one
	mBackgroundOverlay->setImage(ThemeData::getMenuTheme()->Background.fadePath);
	mBackgroundOverlay->setResize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (mClock == nullptr)
	{
		mClock = std::make_shared<TextComponent>(this);
		mClock->setFont(Font::get(FONT_SIZE_SMALL));
		mClock->setHorizontalAlignment(ALIGN_RIGHT);
		mClock->setVerticalAlignment(ALIGN_TOP);
		mClock->setPosition(Renderer::getScreenWidth()*0.94, Renderer::getScreenHeight()*0.9965 - Font::get(FONT_SIZE_SMALL)->getHeight());
		mClock->setSize(Renderer::getScreenWidth()*0.05, 0);
		mClock->setColor(0x777777FF);
	}

	if (mControllerActivity == nullptr)
		mControllerActivity = std::make_shared<ControllerActivityComponent>(this);

	if (mBatteryIndicator == nullptr)
		mBatteryIndicator = std::make_shared<BatteryIndicatorComponent>(this);
	
	if (mVolumeInfo == nullptr)
		mVolumeInfo = std::make_shared<VolumeInfoComponent>(this);
	else
		mVolumeInfo->reset();

	if (mBrightnessInfo == nullptr)
		mBrightnessInfo = std::make_shared<BrightnessInfoComponent>(this);
	else
		mBrightnessInfo->reset();

	// update our help because font sizes probably changed
	if (peekGui())
		peekGui()->updateHelpPrompts();

	return true;
}

void Window::reactivateGui()
{
	for (auto extra : mScreenExtras)
		extra->onShow();

	for (auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
		(*i)->onShow();

	if (peekGui())
		peekGui()->updateHelpPrompts();
}

// A GUI drawn over the whole screen: as large as the screen, and so is one of its children (its menu, or
// its background); a dialog is smaller, and so is a menu that is not full screen
static bool coversScreen(GuiComponent* gui)
{
	auto covers = [](GuiComponent* comp) { return comp->getSize().x() >= Renderer::getScreenWidth() && comp->getSize().y() >= Renderer::getScreenHeight(); };
	if (gui == nullptr || !covers(gui))
		return false;

	for (int i = 0; i < gui->getChildCount(); i++)
		if (covers(gui->getChild(i)))
			return true;

	return false;
}

bool Window::deinit(bool deinitRenderer, bool forGame)
{
	resetMenuBackgroundShader();

	for (auto extra : mScreenExtras)
		extra->onHide();

	// Hide all GUI elements on uninitialisation - this disable
	for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
		(*i)->onHide();

	bool probeStopped = VideoLeadIn::pause(true);
	bool playersStopped = VideoVlcComponent::waitForPlayers(1500);
	if (forGame && (!playersStopped || !probeStopped))
	{
		VideoLeadIn::pause(false);
		for (auto extra : mScreenExtras)
			extra->onShow();
		if (peekGui())
			peekGui()->onShow();
		return false;
	}

	if (deinitRenderer)
		InputManager::getInstance()->deinit();

	TextureResource::clearQueue();
	ResourceManager::getInstance()->unloadAll();

	if (deinitRenderer)
		Renderer::deinit();
	return true;
}

void Window::textInput(const char* text)
{
	if(peekGui())
		peekGui()->textInput(text);
}

void Window::input(InputConfig* config, Input input)
{
	if (config == nullptr)
		return;
	
	if (config->getDeviceIndex() >= 0 && Settings::getInstance()->getBool("FirstJoystickOnly"))
	{
		// Find first player controller info
		auto playerDevices = InputManager::getInstance()->lastKnownPlayersDeviceIndexes();
		auto playerDevice = playerDevices.find(0); 
		if (playerDevice != playerDevices.cend())
		{
			if (config->getDeviceIndex() != playerDevice->second.index)
				return;
		}
		else if (config->getDeviceIndex() > 0) // Not found ? Use SDL device index
			return;
	}

	if (mScreenSaver) 
	{
		if (mScreenSaver->isScreenSaverActive() && Settings::getInstance()->getBool("ScreenSaverControls") &&
			((Settings::getInstance()->getString("ScreenSaverBehavior") == "slideshow") || 			
			(Settings::getInstance()->getString("ScreenSaverBehavior") == "random video")))
		{
			if (config->isMappedLike("right", input) || config->isMappedTo("select", input))
			{
				if (input.value != 0) // handle screensaver control
					mScreenSaver->nextVideo();
					
				mTimeSinceLastInput = 0;
				return;
			}
			else if (config->isMappedTo("start", input) && input.value != 0 && mScreenSaver->getCurrentGame() != nullptr)
			{
				// launch game!				
				cancelScreenSaver();
				mScreenSaver->launchGame();
				// to force handling the wake up process
				mSleeping = true;
			}
		}
	}

	if (cancelScreenSaver())
		return;

	if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_g && SDL_GetModState() & KMOD_LCTRL) // && Settings::getInstance()->getBool("Debug"))
	{
		// toggle debug grid with Ctrl-G
		Settings::setDebugGrid(!Settings::DebugGrid());
	}
	else if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_t && SDL_GetModState() & KMOD_LCTRL) // && Settings::getInstance()->getBool("Debug"))
	{
		// toggle TextComponent debug view with Ctrl-T
		Settings::setDebugText(!Settings::DebugText());
	}
	else if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_i && SDL_GetModState() & KMOD_LCTRL) // && Settings::getInstance()->getBool("Debug"))
	{
		// toggle TextComponent debug view with Ctrl-I
		Settings::setDebugImage(!Settings::DebugImage());
	}
	else if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_m && SDL_GetModState() & KMOD_LCTRL) // && Settings::getInstance()->getBool("Debug"))
	{
		// toggle TextComponent debug view with Ctrl-I
		Settings::setDebugMouse(!Settings::DebugMouse());
	}
	else
	{
		if (mControllerActivity != nullptr)
			mControllerActivity->input(config, input);

		if (peekGui())
			peekGui()->input(config, input); // this is where the majority of inputs will be consumed: the GuiComponent Stack
	}
}

// Notification messages
static std::mutex mNotificationMessagesLock;

void Window::displayNotificationMessage(std::string message, int duration)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	if (duration <= 0)
	{
		duration = Settings::getInstance()->getInt("audio.display_titles_time");
		if (duration <= 2 || duration > 120)
			duration = 10;

		duration *= 1000;
	}

	NotificationMessage msg;
	msg.first = message;
	msg.second = duration;
	mNotificationMessages.push_back(msg);
}


void Window::processNotificationMessages()
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	if (mNotificationMessages.empty())
		return;
	
	NotificationMessage msg = mNotificationMessages.back();
	mNotificationMessages.pop_back();

	LOG(LogDebug) << "Notification message :" << msg.first.c_str();
	
	if (mNotificationPopups.size() == 0)
		PowerSaver::pause();

	auto infoPopup = new GuiInfoPopup(this, msg.first, msg.second);
	mNotificationPopups.push_back(infoPopup);

	layoutNotificationPopups();
}

void Window::stopNotificationPopups()
{
	if (mNotificationPopups.size() == 0)
		return;

	for (auto ip : mNotificationPopups)
		delete ip;

	mNotificationPopups.clear();
	PowerSaver::resume();
}

// With a theme's cap on the popups shown at once, the first one gives way after a while to those waiting; a
// waiting one's time starts when it shows
#define POPUP_WAITING_MS 2500

int Window::shownNotificationCount() const
{
	int count = (int)mNotificationPopups.size();
	if (mNoticeStyle.enabled && mNoticeStyle.maxCount > 0)
		count = std::min(count, mNoticeStyle.maxCount);

	return count;
}

void Window::updateNotificationPopups(int deltaTime)
{
	bool changed = false;

	int shown = shownNotificationCount();
	if (shown > 0 && shown < (int)mNotificationPopups.size())
		mNotificationPopups.front()->shorten(POPUP_WAITING_MS);

	for (int i = mNotificationPopups.size() - 1 ; i >= 0 ; i--)
	{
		if (i >= shown)
			continue;

		mNotificationPopups[i]->update(deltaTime);

		if (!mNotificationPopups[i]->isRunning())
		{
			delete mNotificationPopups[i];

			auto it = mNotificationPopups.begin();
			std::advance(it, i);
			mNotificationPopups.erase(it);

			changed = true;
		}
		else if (mNotificationPopups[i]->getFadeOut() != 0)
			changed = true;
	}

	if (changed)
	{
		if (mNotificationPopups.size() == 0)
			PowerSaver::resume();
		else
			layoutNotificationPopups();
	}
}

void Window::layoutNotificationPopups()
{
	if (mNoticeStyle.enabled)
	{
		// From the theme's place, one under the other; one going away lets the next ones move up
		float posY = mNoticeStyle.pos.y() - mNoticeStyle.size.y() * mNoticeStyle.origin.y();
		int shown = shownNotificationCount();

		for (int i = 0; i < shown; i++)
		{
			auto popup = mNotificationPopups[i];
			float offset = popup->getSize().y() + mNoticeStyle.spacing;

			float fadingOut = popup->getFadeOut();
			if (fadingOut != 0)
			{
				fadingOut = fadingOut - 1;
				fadingOut = Math::lerp(0, 1, fadingOut * fadingOut * fadingOut + 1);
				posY -= offset * fadingOut;
			}

			popup->setPosition(Math::round(mNoticeStyle.pos.x() - popup->getSize().x() * mNoticeStyle.origin.x()), Math::round(posY), 0);
			posY += offset;
		}

		return;
	}

	float posY = Renderer::getScreenHeight() * 0.02f;
	int paddingY = (int)posY;

	for (auto popup : mNotificationPopups)
	{
		float fadingOut = popup->getFadeOut();
		if (fadingOut != 0)
		{
			// cubic ease in
			fadingOut = fadingOut - 1;
			fadingOut = Math::lerp(0, 1, fadingOut*fadingOut*fadingOut + 1);

			posY -= (popup->getSize().y() + paddingY) * fadingOut;
		}

		popup->setPosition(popup->getPosition().x(), posY, 0);
		posY += popup->getSize().y() + paddingY;
	}
}

void Window::processSongTitleNotifications()
{
	if (AudioManager::getInstance()->songNameChanged())
	{
		if (Settings::getInstance()->getBool("audio.display_titles"))
		{
			std::string songName = AudioManager::getInstance()->getSongName();
			if (!songName.empty())
			{
				std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

				for (int i = mNotificationPopups.size() - 1; i >= 0; i--)
				{
					if (mNotificationPopups[i]->getMessage().find(_U("\uF028")) != std::string::npos)
					{
						delete mNotificationPopups[i];

						auto it = mNotificationPopups.begin();
						std::advance(it, i);
						mNotificationPopups.erase(it);
					}
				}

				lock.unlock();
				displayNotificationMessage(_U("\uF028  ") + songName); // _("Now playing: ") + 
			}
		}

		AudioManager::getInstance()->resetSongNameChangedFlag();
	}	
}

void Window::update(int deltaTime)
{
	if (mLastShowCursor >= 0)
	{
		mLastShowCursor += deltaTime;
		if (mLastShowCursor > 5000)
		{
			SDL_ShowCursor(0);
			mLastShowCursor = -1;
		}
	}

	processPostedFunctions();
	processSongTitleNotifications();
	processNotificationMessages();

	if (mNormalizeNextUpdate)
	{
		mNormalizeNextUpdate = false;
		if (deltaTime > mAverageDeltaTime)
			deltaTime = mAverageDeltaTime;
	}

	for (auto extra : mScreenExtras)
		extra->update(deltaTime);

	if (mVolumeInfo)
		mVolumeInfo->update(deltaTime);

	if (mBrightnessInfo)
		mBrightnessInfo->update(deltaTime);

	mFrameTimeElapsed += deltaTime;
	mFrameCountElapsed++;
	if (mFrameTimeElapsed > 500)
	{
		mAverageDeltaTime = mFrameTimeElapsed / mFrameCountElapsed;

		if (Settings::DrawFramerate())
		{
			std::stringstream ss;

			// fps, and the frames drawn (RenderOnChange draws only those that change)
			ss << std::fixed << std::setprecision(1) << (1000.0f * (float)mFrameCountElapsed / (float)mFrameTimeElapsed) << "fps, ";
			ss << std::fixed << std::setprecision(2) << ((float)mFrameTimeElapsed / (float)mFrameCountElapsed) << "ms";
			ss << ", " << std::fixed << std::setprecision(1) << (1000.0f * (float)(Renderer::getCurrentFrame() - mFrameDrawnStart) / (float)mFrameTimeElapsed) << " drawn/s";

			// vram
			float textureVramUsageMb = TextureResource::getTotalMemUsage(false) / 1024.0f / 1024.0f;
			float textureTotalUsageMb = TextureResource::getTotalTextureSize() / 1024.0f / 1024.0f;
			float fontVramUsageMb = Font::getTotalMemUsage() / 1024.0f / 1024.0f;
			size_t max_texture = Settings::getInstance()->getInt("MaxVRAM");

			ss << "\nFont VRAM: " << fontVramUsageMb << " Tex VRAM: " << textureVramUsageMb << " Known Tex: " << textureTotalUsageMb << " Max VRAM: " << max_texture;

			mFrameDataText = std::unique_ptr<TextCache>(mDefaultFonts.at(0)->buildTextCache(ss.str(), Vector2f(50.f, 50.f), 0xFFFF40FF, 0.0f, ALIGN_LEFT, 1.2f));			
		}

		mFrameTimeElapsed = 0;
		mFrameCountElapsed = 0;
		mFrameDrawnStart = Renderer::getCurrentFrame();
	}

	/* draw the clock */ 
	if (Settings::DrawClock() && mClock) 
	{
		mClockElapsed -= deltaTime;
		if (mClockElapsed <= 0)
		{
			time_t     clockNow = time(0);
			struct tm  clockTstruct = *localtime(&clockNow);

			if (clockTstruct.tm_year > 100) 
			{ 
				// Display the clock only if year is more than 1900+100 ; rpi have no internal clock and out of the networks, the date time information has no value
				// Visit http://en.cppreference.com/w/cpp/chrono/c/strftime for more information about date/time format

				std::string clockBuf;
				if (Settings::ClockMode12())
					clockBuf = Utils::Time::timeToString(clockNow, "%I:%M %p");
				else
					clockBuf = Utils::Time::timeToString(clockNow, "%H:%M");

				mClock->setText(clockBuf);
			}

			mClockElapsed = 1000; // next update in 1000ms
		}
	}

	mTimeSinceLastInput += deltaTime;
	updateIdlePause();

	if (peekGui())
		peekGui()->update(deltaTime);

	// Update the screensaver
	if (mScreenSaver)
		mScreenSaver->update(deltaTime);

	// update pads 
	if (mControllerActivity)
		mControllerActivity->update(deltaTime);

	if (mBatteryIndicator)
		mBatteryIndicator->update(deltaTime);

	updateAsyncNotifications(deltaTime);
	updateNotificationPopups(deltaTime);

	AudioManager::update(deltaTime);
}

static std::vector<unsigned int> _gunAimColors = { 0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF, 0xFF0000FF, 0xFFFF0000, 0xFF00FF00 };

void Window::renderSindenBorders()
{
	bool drawGunBorders = false;

	for (auto gun : InputManager::getInstance()->getGuns())
		if (gun->needBorders()) 
			drawGunBorders = true;		

	// normal (default) : draw borders when required
	// hidden : the border are not displayed (assume that there are provided by an other way like bezels)
	// force  : force even if no gun requires it (or even no gun at all)
	// gameonly : borders are not visible in es, just in game (for boards having a sinden gun alway plugged in)
	auto bordersMode = SystemConf::getInstance()->get("controllers.guns.bordersmode");
	if (drawGunBorders)
	  if(bordersMode == "hidden" || bordersMode == "gameonly")
	    drawGunBorders = false;

	if (!drawGunBorders)
	  if(bordersMode == "force")
	    // SETTING FOR DEBUGGING BORDERS
	    drawGunBorders = true;

	if (drawGunBorders && !mRenderScreenSaver)
	{
		int outerBorderWidth = ceil(Renderer::getScreenWidth() * 0.00f);
		int innerBorderWidth = ceil(Renderer::getScreenWidth() * 0.02f);

		// sinden.bordersize=thin/big/medium
		auto bordersize = SystemConf::getInstance()->get("controllers.guns.borderssize");
		if (bordersize == "thin")
		{
			outerBorderWidth = ceil(Renderer::getScreenWidth() * 0.00f);
			innerBorderWidth = ceil(Renderer::getScreenWidth() * 0.01f);
		}
		else if (bordersize == "big")
		{
			outerBorderWidth = ceil(Renderer::getScreenWidth() * 0.01f);
			innerBorderWidth = ceil(Renderer::getScreenWidth() * 0.02f);
		}

		Renderer::setScreenMargin(0, 0);
		Renderer::setMatrix(Transform4x4f::Identity());

		const unsigned int outerBorderColor = 0x000000FF;
		unsigned int innerBorderColor = 0xFFFFFFFF;
		unsigned int externalBorderColor = 0x000000CC;

		auto bordersColor = SystemConf::getInstance()->get("controllers.guns.borderscolor");
		if (bordersColor == "red")   innerBorderColor = 0xFF0000FF;
		if (bordersColor == "green") innerBorderColor = 0x00FF00FF;
		if (bordersColor == "blue")  innerBorderColor = 0x0000FFFF;
		if (bordersColor == "white") innerBorderColor = 0xFFFFFFFF;

		int borderWidth = 0;
		int borderOffset = 0;

		std::string bordersRatio = SystemConf::getInstance()->get("controllers.guns.bordersratio");
		if(bordersRatio == "4:3") {
		  borderWidth = Renderer::getScreenHeight() / 3 * 4;
		  borderOffset = (Renderer::getScreenWidth() - borderWidth) / 2;
		} else {
		  borderWidth = Renderer::getScreenWidth();
		  borderOffset = 0;
		}

		// outer border
		Renderer::drawRect(borderOffset, 0, borderWidth, outerBorderWidth, outerBorderColor);
		Renderer::drawRect(borderOffset + borderWidth - outerBorderWidth, 0, outerBorderWidth, Renderer::getScreenHeight(), outerBorderColor);
		Renderer::drawRect(borderOffset, Renderer::getScreenHeight() - outerBorderWidth, borderWidth, outerBorderWidth, outerBorderColor);
		Renderer::drawRect(borderOffset, 0, outerBorderWidth, Renderer::getScreenHeight(), outerBorderColor);

		// inner border
		Renderer::drawRect(borderOffset + outerBorderWidth, outerBorderWidth, borderWidth - outerBorderWidth * 2, innerBorderWidth, innerBorderColor);
		Renderer::drawRect(borderOffset + borderWidth - outerBorderWidth - innerBorderWidth, outerBorderWidth, innerBorderWidth, Renderer::getScreenHeight() - outerBorderWidth * 2, innerBorderColor);
		Renderer::drawRect(borderOffset + outerBorderWidth, Renderer::getScreenHeight() - outerBorderWidth - innerBorderWidth, borderWidth - outerBorderWidth * 2, innerBorderWidth, innerBorderColor);
		Renderer::drawRect(borderOffset + outerBorderWidth, outerBorderWidth, innerBorderWidth, Renderer::getScreenHeight() - outerBorderWidth * 2, innerBorderColor);

		Renderer::setScreenMargin(borderOffset + outerBorderWidth + innerBorderWidth, outerBorderWidth + innerBorderWidth);
		Renderer::setMatrix(Transform4x4f::Identity());
	}
	else
		Renderer::setScreenMargin(0, 0);
}

void Window::renderMenuBackgroundShader()
{
	auto menuBackground = ThemeData::getMenuTheme()->Background;

	const Renderer::ShaderInfo& info = menuBackground.shader;
	if (!info.path.empty())
	{
		if (mMenuBackgroundShaderTextureCache == -1)
			Renderer::postProcessShader(info.path, 0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), info.parameters, &mMenuBackgroundShaderTextureCache);

		if (mMenuBackgroundShaderTextureCache != -1)
		{
			Renderer::bindTexture(mMenuBackgroundShaderTextureCache);
			Renderer::setMatrix(Transform4x4f::Identity());

			int w = Renderer::getScreenWidth();
			int h = Renderer::getScreenHeight();

			Renderer::Vertex vertices[4];
			vertices[0] = { { (float)0    , (float)0       }, { 0.0f, 1.0f }, 0xFFFFFFFF };
			vertices[1] = { { (float)0    , (float)0 + h   }, { 0.0f, 0.0f }, 0xFFFFFFFF };
			vertices[2] = { { (float)0 + w, (float)0       }, { 1.0f, 1.0f }, 0xFFFFFFFF };
			vertices[3] = { { (float)0 + w, (float)0 + h   }, { 1.0f, 0.0f }, 0xFFFFFFFF };

			Renderer::drawTriangleStrips(&vertices[0], 4, Renderer::Blend::ONE, Renderer::Blend::ONE);
			Renderer::bindTexture(0);
		}
	}
	else
		resetMenuBackgroundShader();
}

void Window::resetMenuBackgroundShader()
{
	if (mMenuBackgroundShaderTextureCache != -1)
	{
		Renderer::destroyTexture(mMenuBackgroundShaderTextureCache);
		mMenuBackgroundShaderTextureCache = -1;
	}
}

void Window::render()
{
	Transform4x4f transform = Transform4x4f::Identity();

	mRenderedHelpPrompts = false;
	
	// draw only bottom and top of GuiStack (if they are different)
	if (mGuiStack.size())
	{
		auto& bottom = mGuiStack.front();
		auto& top = mGuiStack.back();

		auto menuBackground = ThemeData::getMenuTheme()->Background;

		// Don't render bottom if we have a MenuBackgroundShaderTextureCache
		if (mMenuBackgroundShaderTextureCache == -1)
			bottom->render(transform);

		if (bottom != top)
		{
			if ((top->getTag() == "GuiLoading") && mGuiStack.size() > 2)
			{
				mBackgroundOverlay->render(transform);
				renderMenuBackgroundShader();

				auto& middle = mGuiStack.at(mGuiStack.size() - 2);
				if (middle != bottom)
					middle->render(transform);

				top->render(transform);
			}
			else
			{
				if ((top->isKindOf<GuiMsgBox>() || top->getTag() == "popup") && mGuiStack.size() > 2)
				{
					auto& middle = mGuiStack.at(mGuiStack.size() - 2);
					if (middle != bottom)
						middle->render(transform);
				}

				mBackgroundOverlay->render(transform);
				renderMenuBackgroundShader();

				top->render(transform);
			}
		}
		else  
			resetMenuBackgroundShader();
	}
	else 
		resetMenuBackgroundShader();

	renderSindenBorders();

	if (mGuiStack.size() < 2 || !Renderer::ScreenSettings::fullScreenMenus())
		if (!mRenderedHelpPrompts)
			mHelp->render(transform);

	// FPS overlay
	if (Settings::DrawFramerate() && mFrameDataText)
	{
		Renderer::setMatrix(transform);
		Renderer::drawSolidRectangle(40.f, 45.f, mFrameDataText->metrics.size.x() + 10.f, mFrameDataText->metrics.size.y() + 10.f, 0x00000080, 0xFFFFFF30, 2.0f, 3.0f);

		auto trans = transform;
		trans.translate(1, 1);
		Renderer::setMatrix(trans);

		mFrameDataText->setColor(0x000000FF);
		mDefaultFonts.at(1)->renderTextCache(mFrameDataText.get());
		Renderer::setMatrix(transform);

		mFrameDataText->setColor(0xFFFF40FF);		
		mDefaultFonts.at(1)->renderTextCache(mFrameDataText.get());
	}

	// clock 
	if (Settings::DrawClock() && mClock && (mGuiStack.size() < 2 || !Renderer::ScreenSettings::fullScreenMenus()))
		mClock->render(transform);

	if (Settings::ShowControllerActivity() && mControllerActivity != nullptr && (mGuiStack.size() < 2 || !Renderer::isSmallScreen()))
		mControllerActivity->render(transform);

	if (mBatteryIndicator != nullptr && (mGuiStack.size() < 2 || !Renderer::ScreenSettings::fullScreenMenus()))
		mBatteryIndicator->render(transform);

	Renderer::setMatrix(transform);

	unsigned int screensaverTime = (unsigned int)Settings::ScreenSaverTime();
	if (mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
		startScreenSaver();

	// Render notifications
	if (!mRenderScreenSaver)
	{
		if (!mNoticeStyle.enabled)
			for (auto popup : mNotificationPopups)
				popup->render(transform);

		renderAsyncNotifications(transform);
	}

	// Always call the screensaver render function regardless of whether the screensaver is active
	// or not because it may perform a fade on transition
	renderScreenSaver();

	if (!mRenderScreenSaver)
	{
		// A full-screen menu covers the screen's extras (a status bar); a dialog does not, and a screen shader
		// still applies
		bool menuOver = false;
		if (Renderer::ScreenSettings::fullScreenMenus())
			for (size_t i = 1; i < mGuiStack.size() && !menuOver; i++)
				menuOver = coversScreen(mGuiStack[i]);

		for (auto extra : mScreenExtras)
			if (!menuOver || extra->isKindOf<PostProcessShaderComponent>())
				extra->render(transform);

		// A theme's popups go over the screen's extras (its status line)
		if (mNoticeStyle.enabled)
		{
			int shown = shownNotificationCount();
			for (int i = 0; i < shown; i++)
				mNotificationPopups[i]->render(transform);
		}
	}

	if (mMeterStyle.enabled && mVolumeInfo && mBrightnessInfo)
	{
		// The theme's place for one meter; both showing, side by side around it
		bool both = mVolumeInfo->isShowing() && mBrightnessInfo->isShowing();
		float width = mMeterStyle.size.x();
		float x = mMeterStyle.pos.x() - (both ? 2 * width + mMeterStyle.spacing : width) * mMeterStyle.origin.x();
		float y = mMeterStyle.pos.y() - mMeterStyle.size.y() * mMeterStyle.origin.y();

		if (mVolumeInfo->isShowing())
		{
			mVolumeInfo->setPosition(Math::round(x), Math::round(y), 0);
			if (both)
				x += width + mMeterStyle.spacing;
		}

		if (mBrightnessInfo->isShowing())
			mBrightnessInfo->setPosition(Math::round(x), Math::round(y), 0);
	}

	if (mVolumeInfo && Settings::VolumePopup())
		mVolumeInfo->render(transform);

	if (mBrightnessInfo && Settings::VolumePopup())
		mBrightnessInfo->render(transform);

	if (mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
	{
		if (mAllowSleep && (!mScreenSaver || mScreenSaver->allowSleep()))
		{
			// go to sleep
			if (mSleeping == false) {
				mSleeping = true;
				onSleep();
			}
		}
	}

	// Render calibration dark background & text
	if (mCalibrationText)
	{
		Renderer::setMatrix(transform);
		Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x000000A0);
		mCalibrationText->render(transform);
	}

	// Render guns aims
	auto guns = InputManager::getInstance()->getGuns();
	if (!mRenderScreenSaver && guns.size())
	{
		auto margin = Renderer::setScreenMargin(0, 0);
		Renderer::setMatrix(Transform4x4f::Identity());

		bool hasMousePointer = false;

		if (mGunAimTexture == nullptr)
			mGunAimTexture = TextureResource::get(":/gun.png", false, false, true, false);

		if (mGunAimTexture->bind())
		{
			for (auto gun : guns)
			{
				if (gun->isMouse())
				{
					hasMousePointer = true;
					continue;
				}

				if (gun->isLastTickElapsed())
					continue;

				int pointerSize = (Renderer::isVerticalScreen() ? Renderer::getScreenWidth() : Renderer::getScreenHeight()) / 32;

				Vector2f topLeft = { gun->x() - pointerSize, gun->y() - pointerSize };
				Vector2f bottomRight = { gun->x() + pointerSize, gun->y() + pointerSize };

				auto aimColor = guns.size() == 1 ? 0xFFFFFFFF : _gunAimColors[gun->index() % _gunAimColors.size()];

				if (gun->isLButtonDown() || gun->isRButtonDown())
				{
					auto mixIndex = (gun->index() + 3) % _gunAimColors.size();
					auto invertColor = _gunAimColors[mixIndex];

					aimColor = Renderer::mixColors(aimColor, invertColor, 0.5);
				}

				Renderer::Vertex vertices[4];
				vertices[0] = { { topLeft.x() ,     topLeft.y() }, { 0.0f,          0.0f }, aimColor };
				vertices[1] = { { topLeft.x() ,     bottomRight.y() }, { 0.0f,          1.0f }, aimColor };
				vertices[2] = { { bottomRight.x(), topLeft.y() }, { 1.0f, 0.0f }, aimColor };
				vertices[3] = { { bottomRight.x(), bottomRight.y() }, { 1.0f, 1.0f }, aimColor };

				Renderer::drawTriangleStrips(&vertices[0], 4);
			}
		}	

#if WIN32
		if (hasMousePointer)
		{
			if (mMouseCursorTexture == nullptr)
				mMouseCursorTexture = TextureResource::get(":/cursor.png", false, true, true, false);

			if (mMouseCursorTexture->bind())
			{
				for (auto gun : guns)
				{
					if (!gun->isMouse())
						continue;

					if (gun->isLastTickElapsed())
						break;

					SDL_SysWMinfo wmInfo;
					SDL_VERSION(&wmInfo.version);
					if (SDL_GetWindowWMInfo(Renderer::getSDLWindow(), &wmInfo))
					{
						HWND hWnd = wmInfo.info.win.window;

						POINT cursorPos;
						GetCursorPos(&cursorPos);

						// Convert screen coordinates to client coordinates
						ScreenToClient(hWnd, &cursorPos);

						// Get the client rectangle of the window
						RECT clientRect;
						GetClientRect(hWnd, &clientRect);

						// Check if the cursor is within the client area
						if (!PtInRect(&clientRect, cursorPos))
							continue;
					}					

					int pointerSize = (Renderer::isVerticalScreen() ? Renderer::getScreenWidth() : Renderer::getScreenHeight()) / 38;
					
					Vector2i sz = ImageIO::adjustPictureSize(mMouseCursorTexture->getSize(), Vector2i(pointerSize, pointerSize));

					Vector2f topLeft = { gun->x(), gun->y() };
					Vector2f bottomRight = { gun->x() + sz.x(), gun->y() + sz.y() };

					auto aimColor = 0xFFFFFFFF;
					
					if (gun->isLButtonDown() || gun->isRButtonDown())
						aimColor = 0xC0FAFAFF;

					Renderer::Vertex vertices[4];
					vertices[0] = { { topLeft.x() ,     topLeft.y() }, { 0.0f,          1.0f }, aimColor };
					vertices[1] = { { topLeft.x() ,     bottomRight.y() }, { 0.0f,          0.0f }, aimColor };
					vertices[2] = { { bottomRight.x(), topLeft.y() }, { 1.0f, 1.0f }, aimColor };
					vertices[3] = { { bottomRight.x(), bottomRight.y() }, { 1.0f, 0.0f }, aimColor };

					Renderer::drawTriangleStrips(&vertices[0], 4);
					break;
				}
			}
		}
#endif

		Renderer::setScreenMargin(margin.x(), margin.y());
	}
}

void Window::checkSystemResume()
{
#ifndef WIN32
	struct timespec boot, mono;
	if (clock_gettime(CLOCK_BOOTTIME, &boot) != 0 || clock_gettime(CLOCK_MONOTONIC, &mono) != 0)
		return;

	long long suspended = ((long long)boot.tv_sec - mono.tv_sec) * 1000 + (boot.tv_nsec - mono.tv_nsec) / 1000000;
	bool resumed = mSuspendedMs >= 0 && suspended - mSuspendedMs > 1000;
	mSuspendedMs = suspended;

	if (!resumed)
		return;

	LOG(LogInfo) << "Window: back from a system suspend; the screen saver stops";
	cancelScreenSaver();
	updateIdlePause();
	Renderer::invalidateFrame();
#endif
}

bool Window::drawsOnlyChanges()
{
	return mGuiStack.size() == 1 && !mRenderScreenSaver && !mSleeping && mRenderOnChange;
}

// A setting left at "auto" follows the theme's variable; "true" or "false" holds for every theme
static bool themeOption(const std::shared_ptr<ThemeData>& theme, const std::string& setting, const std::string& variable)
{
	std::string value = Settings::getInstance()->getString(setting);
	if (value == "true" || value == "1")
		return true;

	if (value != "auto" && !value.empty())
		return false;

	if (theme == nullptr)
		return false;

	value = theme->getVariable(variable);
	return value == "true" || value == "1";
}

void Window::applyThemeOptions(const std::shared_ptr<ThemeData>& theme)
{
	mRenderOnChange = themeOption(theme, "RenderOnChange", "esRenderOnChange");
	BindingManager::setBindShownOnly(themeOption(theme, "BindShownOnly", "esBindShownOnly"));

	mIdlePauseSeconds = Settings::getInstance()->getInt("IdlePause");
	if (mIdlePauseSeconds < 0)
		mIdlePauseSeconds = theme == nullptr ? 0 : Math::max(0, atoi(theme->getVariable("esIdlePause").c_str()));

	LOG(LogInfo) << "Window: draws only changes " << (mRenderOnChange ? "yes" : "no") << ", idle pause " << mIdlePauseSeconds << " s, binds only shown containers " << (BindingManager::bindsShownOnly() ? "yes" : "no");
	updateIdlePause();
}

void Window::updateIdlePause()
{
	int seconds = mIdlePauseSeconds;
	bool idle = seconds > 0 && mTimeSinceLastInput >= (unsigned int)seconds * 1000;

	mIdleVideo = idle && Settings::getInstance()->getBool("IdlePauseVideo");
	mIdleSlides = idle && Settings::getInstance()->getBool("IdlePauseSlides");
}

void Window::normalizeNextUpdate()
{
	mNormalizeNextUpdate = true;
}

bool Window::getAllowSleep()
{
	return mAllowSleep;
}

void Window::setAllowSleep(bool sleep)
{
	mAllowSleep = sleep;
}

std::string Window::getCustomSplashScreenImage() {
  std::string alternateSplashScreen = Settings::getInstance()->getString("AlternateSplashScreen");
  if(alternateSplashScreen != "" && Utils::FileSystem::exists(alternateSplashScreen))
    return alternateSplashScreen;
  return DEFAULT_SPLASH_IMAGE;
}

void Window::setCustomSplashScreen(std::string imagePath, std::string customText, IBindable* bindable)
{
	if (Settings::getInstance()->getBool("HideWindow"))
		return;

	if (!Utils::FileSystem::exists(imagePath))
		mSplash = std::make_shared<Splash>(this, getCustomSplashScreenImage(), false, bindable);
	else
		mSplash = std::make_shared<Splash>(this, imagePath, false, bindable);

	mSplash->update(customText);
}

void Window::renderSplashScreen(std::string text, float percent, float opacity)
{
	if (mSplash == NULL)
		mSplash = std::make_shared<Splash>(this, getCustomSplashScreenImage());

	mSplash->update(text, percent);
	mSplash->render(opacity);	
}

void Window::renderSplashScreen(float opacity, bool swapBuffers)
{
	if (mSplash != nullptr)
		mSplash->render(opacity, swapBuffers);
}

void Window::closeSplashScreen()
{
	mSplash = nullptr;
}

void Window::renderHelpPromptsEarly(const Transform4x4f& transform)
{
	mHelp->render(transform);
	mRenderedHelpPrompts = true;
}

void Window::setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style)
{
	// Keep a temporary reference to the previous grid.
	// It avoids unloading/reloading images if they are the same, and avoids flickerings
	auto oldGrid = mHelp->getGrid();

	mHelp->clearPrompts();
	mHelp->setStyle(style);

	mClockElapsed = -1;

	std::vector<HelpPrompt> addPrompts;

	std::map<std::string, bool> inputSeenMap;
	std::map<std::string, int> mappedToSeenMap;
	for(auto it = prompts.cbegin(); it != prompts.cend(); it++)
	{
		// only add it if the same icon hasn't already been added
		if(inputSeenMap.emplace(it->first, true).second)
		{
			// this symbol hasn't been seen yet, what about the action name?
			auto mappedTo = mappedToSeenMap.find(it->second);
			if(mappedTo != mappedToSeenMap.cend())
			{
				// yes, it has!

				// can we combine? (dpad only)
				if((it->first == "up/down" && addPrompts.at(mappedTo->second).first != "left/right") ||
					(it->first == "left/right" && addPrompts.at(mappedTo->second).first != "up/down"))
				{
					// yes!
					addPrompts.at(mappedTo->second).first = "up/down/left/right";
					// don't need to add this to addPrompts since we just merged
				}else{
					// no, we can't combine!
					addPrompts.push_back(*it);
				}
			}else{
				// no, it hasn't!
				mappedToSeenMap.emplace(it->second, (int)addPrompts.size());
				addPrompts.push_back(*it);
			}
		}
	}

	// sort prompts so it goes [dpad_all] [dpad_u/d] [dpad_l/r] [a/b/x/y/l/r] [start/select]
	std::sort(addPrompts.begin(), addPrompts.end(), [](const HelpPrompt& a, const HelpPrompt& b) -> bool {

		static const char* map[] = {
			"up/down/left/right",
			"up/down",
			"left/right",
			BUTTON_OK, "x", "y", "l", "r", BUTTON_BACK,
			"start", "select",
			NULL
		};

		int i = 0;
		int aVal = 0;
		int bVal = 0;
		while(map[i] != NULL)
		{
			if(a.first == map[i])
				aVal = i;
			if(b.first == map[i])
				bVal = i;
			i++;
		}

		return aVal > bVal;
	});

	mHelp->setPrompts(addPrompts);
}


void Window::onSleep()
{
	Scripting::fireEvent("sleep");
}

void Window::onWake()
{
	Scripting::fireEvent("wake");
}

void Window::startScreenSaver()
{
	if (mScreenSaver && !mRenderScreenSaver)
	{
		for (auto extra : mScreenExtras)
			extra->onScreenSaverActivate();

		// Tell the GUI components the screensaver is starting
		for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
			(*i)->onScreenSaverActivate();

		mScreenSaver->startScreenSaver();
		mRenderScreenSaver = true;
	}
}

bool Window::cancelScreenSaver()
{
	bool ret = false;

	mTimeSinceLastInput = 0;

	if (mScreenSaver && mRenderScreenSaver)
	{		
		mScreenSaver->stopScreenSaver();
		mRenderScreenSaver = false;
		mScreenSaver->resetCounts();

		// Tell the GUI components the screensaver has stopped
		for(auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
			(*i)->onScreenSaverDeactivate();

		for (auto extra : mScreenExtras)
			extra->onScreenSaverDeactivate();

		ret = true;
	}

	if (mSleeping)
	{
		mSleeping = false;
		onWake();

		ret = true;
	}

	return ret;
}

void Window::renderScreenSaver()
{
	if (mScreenSaver)
		mScreenSaver->renderScreenSaver();
}

AsyncNotificationComponent* Window::createAsyncNotificationComponent(bool actionLine)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	AsyncNotificationComponent* pc = new AsyncNotificationComponent(this, actionLine);
	mAsyncNotificationComponent.push_back(pc);
	
	if (mAsyncNotificationComponent.size() == 1)
		PowerSaver::pause();

	return pc;
}

void Window::renderAsyncNotifications(const Transform4x4f& trans)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

#define PADDING_H  (Renderer::getScreenWidth()*0.01)

	float posY = Renderer::getScreenHeight() * mNotificationOffsetY;

	bool first = true;
	for (auto child : mAsyncNotificationComponent)
	{		
		float posX = Renderer::getScreenWidth()*0.99f - child->getSize().x();

		float offset = child->getSize().y() + PADDING_H;

		float fadingOut = child->getFading();
		if (fadingOut != 0)
		{
			// cubic ease in
			fadingOut = fadingOut - 1;
			fadingOut = Math::lerp(0, 1, fadingOut*fadingOut*fadingOut + 1);
						
			if (child->isClosing())
			{
				child->setPosition(posX, posY - (offset * fadingOut), 0);
				offset = offset - (offset * fadingOut);

				auto sz = child->getSize();
				Renderer::pushClipRect(Vector2i(
					(int)trans.translation()[0] + posX - PADDING_H, 
					(int)trans.translation()[1] + (first ? 0 : posY)), 
					Vector2i(
					(int)sz.x() + 2 * PADDING_H, 
					(int)sz.y() + (first ? posY : 0)));
			}
			else 
				child->setPosition(posX + (child->getSize().x() * (1.0 - fadingOut)), posY, 0);
		}
		else 
			child->setPosition(posX, posY, 0);

		child->render(trans);

		if (fadingOut != 0 && child->isClosing())
			Renderer::popClipRect();

		posY += offset;
		first = false;
	}
}

void Window::updateAsyncNotifications(int deltaTime)
{
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	bool changed = false;

	for (int i = mAsyncNotificationComponent.size() - 1; i >= 0; i--)
	{
		mAsyncNotificationComponent[i]->update(deltaTime);

		if (!mAsyncNotificationComponent[i]->isRunning())
		{
			delete mAsyncNotificationComponent[i];

			auto it = mAsyncNotificationComponent.begin();
			std::advance(it, i);
			mAsyncNotificationComponent.erase(it);

			changed = true;
		}
	}

	if (changed && mAsyncNotificationComponent.size() == 0)
		PowerSaver::resume();
}

void Window::unregisterPostedFunctions(void* data)
{
	if (data == nullptr)
		return;

	for (auto it = mFunctions.cbegin(); it != mFunctions.cend(); )
	{
		if ((*it).container == data)
			it = mFunctions.erase(it);
		else
			it++;
	}
}

void Window::postToUiThread(const std::function<void()>& func, void* data)
{	
	std::unique_lock<std::mutex> lock(mNotificationMessagesLock);

	PostedFunction pf;
	pf.func = func;
	pf.container = data;
	mFunctions.push_back(pf);
	if (mSleeping || !PowerSaver::getState())
	{
		mSleeping = false;
		mTimeSinceLastInput = 0;
		PowerSaver::pushRefreshEvent();
	}
}

void Window::processPostedFunctions()
{
	std::vector<PostedFunction> functions;

	mNotificationMessagesLock.lock();

	for (auto func : mFunctions)
		functions.push_back(func);

	mFunctions.clear();

	mNotificationMessagesLock.unlock();

	for (auto func : functions)
		TRYCATCH("processPostedFunction", func.func())
}

// A screen view's notification element with a size: the look and place of the popups it styles
static Window::PopupStyle readPopupStyle(const ThemeData::ThemeElement* elem)
{
	Window::PopupStyle style;
	if (elem == nullptr || !elem->has("size"))
		return style;

	Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	Vector2f size = elem->get<Vector2f>("size") * screen;
	if (size.x() <= 0 || size.y() <= 0)
		return style;

	style.enabled = true;
	style.vertical = elem->has("vertical") && elem->get<bool>("vertical");
	style.size = Vector2f(Math::round(size.x()), Math::round(size.y()));
	style.pos = elem->has("pos") ? elem->get<Vector2f>("pos") * screen : Vector2f::Zero();
	style.font = Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_MINI));

	if (elem->has("origin"))
		style.origin = elem->get<Vector2f>("origin");

	if (elem->has("color"))
		style.color = elem->get<unsigned int>("color");

	if (elem->has("backgroundColor"))
		style.backgroundColor = elem->get<unsigned int>("backgroundColor");

	if (elem->has("borderColor"))
		style.borderColor = elem->get<unsigned int>("borderColor");

	if (elem->has("trackColor"))
		style.trackColor = elem->get<unsigned int>("trackColor");

	if (elem->has("fillColor"))
		style.fillColor = elem->get<unsigned int>("fillColor");

	if (elem->has("cornerSize"))
		style.cornerRadius = Math::round(elem->get<float>("cornerSize") * screen.y());

	if (elem->has("iconPath"))
		style.iconPath = elem->get<std::string>("iconPath");

	if (elem->has("iconSize"))
		style.iconSize = Math::round(elem->get<float>("iconSize") * screen.y());

	if (elem->has("spacing"))
		style.spacing = Math::round(elem->get<float>("spacing") * screen.y());

	if (elem->has("maxCount"))
		style.maxCount = std::max(0, (int)elem->get<float>("maxCount"));

	return style;
}

void Window::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	applyThemeOptions(theme);

	for (auto extra : mScreenExtras)
		delete extra;

	mScreenExtras.clear();
	mScreenExtras = ThemeData::makeExtras(theme, "screen", this);
	
	std::stable_sort(mScreenExtras.begin(), mScreenExtras.end(), [](GuiComponent* a, GuiComponent* b) { return b->getZIndex() > a->getZIndex(); });

	if (mBackgroundOverlay)
		mBackgroundOverlay->setImage(ThemeData::getMenuTheme()->Background.fadePath);

	auto elem = theme->getElement("screen", "notification", "notification");
	if (elem && elem->has("y"))
		mNotificationOffsetY = elem->get<float>("y");
	else
	{
		// default value
		mNotificationOffsetY = 0.02f;
	}

	mNoticeStyle = readPopupStyle(elem);
	mMeterStyle = readPopupStyle(theme->getElement("screen", "meters", "notification"));

	if (mClock)
	{
		mClock->setFont(Font::get(FONT_SIZE_SMALL));
		mClock->setColor(0x777777FF);		
		mClock->setOrigin(Vector2f::Zero());
		mClock->setHorizontalAlignment(ALIGN_RIGHT);
		mClock->setVerticalAlignment(ALIGN_TOP);
		
		// if clock element does not exist in screen view -> <view name="screen"><text name="clock"> 
		// skin it from system.helpsystem -> <view name="system"><helpsystem name="help"> )
		if (!theme->getElement("screen", "clock", "text"))
		{
			auto elem = theme->getElement("system", "help", "helpsystem");
			if (elem && elem->has("textColor"))
				mClock->setColor(elem->get<unsigned int>("textColor"));

			if (elem && (elem->has("fontPath") || elem->has("fontSize")))
				mClock->setFont(Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_MEDIUM)));
		}
		
		mClock->setPosition(Renderer::getScreenWidth()*0.94, Renderer::getScreenHeight()*0.9965 - mClock->getFont()->getHeight());
		mClock->setSize(Renderer::getScreenWidth()*0.05, 0);

		mClock->applyTheme(theme, "screen", "clock", ThemeFlags::ALL ^ (ThemeFlags::TEXT));
	}

	if (mControllerActivity)
		mControllerActivity->applyTheme(theme, "screen", "controllerActivity", ThemeFlags::ALL ^ (ThemeFlags::TEXT));

	if (mBatteryIndicator)
		mBatteryIndicator->applyTheme(theme, "screen", "batteryIndicator", ThemeFlags::ALL);
	
	mVolumeInfo = std::make_shared<VolumeInfoComponent>(this);
	mBrightnessInfo = std::make_shared<BrightnessInfoComponent>(this);
}

void Window::setGunCalibrationState(bool isCalibrating)
{
	if (isCalibrating)
	{
		if (mCalibrationText == nullptr)
		{
			mCalibrationText = std::make_shared<TextComponent>(this);
			mCalibrationText->setText(_("CALIBRATING GUN\nFire on targets to calibrate"));
			mCalibrationText->setFont(Font::get(FONT_SIZE_MEDIUM));
			mCalibrationText->setHorizontalAlignment(ALIGN_CENTER);
			mCalibrationText->setVerticalAlignment(ALIGN_CENTER);
			mCalibrationText->setPosition(0.0f, 0.0f);
			mCalibrationText->setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight() / 2.1f);
			mCalibrationText->setColor(0xFFFFFFFF);
			mCalibrationText->setGlowSize(1);
			mCalibrationText->setGlowColor(0x00000040);			
		}
	}
	else
		mCalibrationText = nullptr;	
}

std::vector<GuiComponent*> Window::hitTest(int x, int y)
{
	GuiComponent* gui = peekGui();
	if (!gui)
		return std::vector<GuiComponent*>();

	auto trans = Transform4x4f::Identity();

	std::vector<GuiComponent*> ret;

	gui->hitTest(x, y, trans, &ret);

	if (mClock && mClock->isVisible())
		mClock->hitTest(x, y, trans, &ret);

	if (mHelp && mHelp->isVisible())
		mHelp->hitTest(x, y, trans, &ret);

	return ret;
}

void Window::processMouseWheel(int delta)
{
	GuiComponent* gui = peekGui();
	if (!gui)
		return;

	auto hits = hitTest(mLastMousePoint.x(), mLastMousePoint.y());

	if (mMouseCapture != nullptr)
	{
		mMouseCapture->onMouseWheel(delta);
		return;
	}
	else
	{
		std::reverse(hits.begin(), hits.end());

		for (auto hit : hits)
			if (hit->onMouseWheel(delta))
				break;

		hitTest(mLastMousePoint.x(), mLastMousePoint.y());
	}
}

void Window::processMouseMove(int x, int y, bool touchScreen)
{
	if (!touchScreen && (mLastShowCursor != -2 || x != 0 || y != 0))
	{
#if WIN32
		auto guns = InputManager::getInstance()->getGuns();
		if (guns.size() == 0 || std::find_if(guns.cbegin(), guns.cend(), [](Gun* x) { return x->name() == "Wiimote Gun"; }) == guns.cend())
#endif
		SDL_ShowCursor(1);

		mLastShowCursor = 0;
	}

	auto point = Renderer::physicalScreenToRotatedScreen(x, y);

	mLastMousePoint.x() = point.x(); mLastMousePoint.y() = point.y();

	GuiComponent* gui = peekGui();
	if (!gui)
		return;

	auto hits = hitTest(point.x(), point.y());

	if (mMouseCapture != nullptr)
	{
		mMouseCapture->onMouseMove(point.x(), point.y());
		return;
	}
	else
	{
		std::reverse(hits.begin(), hits.end());

		for(auto hit : hits)
			hit->onMouseMove(point.x(), point.y());
	}
}

bool Window::processMouseButton(int button, bool down, int x, int y)
{
	auto point = Renderer::physicalScreenToRotatedScreen(x, y);

	mLastMousePoint.x() = point.x(); mLastMousePoint.y() = point.y();

	if (mMouseCapture != nullptr)
	{
	//	auto hits = hitTest(x, y);
		mMouseCapture->onMouseClick(button, down, point.x(), point.y());
		return true;
	}

	auto ctrls = hitTest(point.x(), point.y());
	std::reverse(ctrls.begin(), ctrls.end());

	for (auto ctrl : ctrls)		
		if (ctrl->onMouseClick(button, down, point.x(), point.y()))
			return true;

	return false;
}
