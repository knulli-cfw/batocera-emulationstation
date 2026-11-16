#include "VolumeInfoComponent.h"
#include "ThemeData.h"
#include "PowerSaver.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "LocaleES.h"
#include "VolumeControl.h"
#include "Window.h"
#include <cmath>

#define PADDING_PX			(Renderer::getScreenWidth()*0.006)
#define PADDING_BAR			(Renderer::isSmallScreen() ? Renderer::getScreenWidth()*0.02 : Renderer::getScreenWidth()*0.006)

#define VISIBLE_TIME		2650
#define FADE_TIME			250
#define CHECKVOLUMEDELAY	40

// WCAG minimum contrast for user interface parts
#define MIN_BAR_CONTRAST	3.0f

// Relative luminance (WCAG) of a 0xRRGGBBAA color
static float relativeLuminance(unsigned int color)
{
	auto channel = [](unsigned int c)
	{
		float v = c / 255.0f;
		return v <= 0.03928f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
	};

	return 0.2126f * channel((color >> 24) & 0xFF) + 0.7152f * channel((color >> 16) & 0xFF) + 0.0722f * channel((color >> 8) & 0xFF);
}

static float contrastRatio(unsigned int a, unsigned int b)
{
	float la = relativeLuminance(a);
	float lb = relativeLuminance(b);
	return (Math::max(la, lb) + 0.05f) / (Math::min(la, lb) + 0.05f);
}

// Opaque color seen when color is drawn with alpha (0-255) over background
static unsigned int blendColor(unsigned int color, unsigned int background, int alpha)
{
	unsigned int ret = 0xFF;
	for (int shift = 8; shift <= 24; shift += 8)
	{
		int c = (color >> shift) & 0xFF;
		int b = (background >> shift) & 0xFF;
		ret |= (unsigned int)((b + (c - b) * alpha / 255) & 0xFF) << shift;
	}

	return ret;
}

VolumeInfoComponent::VolumeInfoComponent(Window* window, bool actionLine)
	: GuiComponent(window)
{
	mDisplayTime = -1;
	mVolume = -1;
	mCheckTime = 0;

	auto theme = ThemeData::getMenuTheme();	

	auto font = theme->TextSmall.font;
	if (Renderer::isSmallScreen())
		font = theme->Text.font;

	Vector2f fullSize(
		2 * PADDING_PX + font->sizeText("100%").x(),
		2 * PADDING_PX + Renderer::getScreenHeight() * 0.20f);
	
	fullSize.y() = fullSize.x() * 2.5f;

	setSize(fullSize);

	mFrame = new NinePatchComponent(window);
	mFrame->setImagePath(theme->Background.path);
	mFrame->setEdgeColor(theme->Background.color);
	mFrame->setCenterColor(theme->Background.centerColor);
	mFrame->setCornerSize(theme->Background.cornerSize);
	mFrame->setPostProcessShader(theme->Background.menuShader, false);
	mFrame->fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	addChild(mFrame);

	// The bar is a track in the text color at half opacity, filled with the small text color.
	// Some themes make both look the same (e.g. white text, grey small text on a dark menu),
	// so use the text color for the fill, then a fainter track, until the fill stands out.
	mTrackDivider = 2;
	mFillColor = theme->TextSmall.color;

	unsigned int track = blendColor(theme->Text.color, theme->Background.centerColor, 255 / mTrackDivider);
	if (contrastRatio(mFillColor, track) < MIN_BAR_CONTRAST)
	{
		mFillColor = theme->Text.color;
		if (contrastRatio(mFillColor, track) < MIN_BAR_CONTRAST)
			mTrackDivider = 4;
	}

	mLabel = new TextComponent(mWindow, "", font, theme->Text.color, ALIGN_CENTER);
	
	int h = font->sizeText("100%").y() + PADDING_PX;
	mLabel->setPosition(0, fullSize.y() - h);
	mLabel->setSize(fullSize.x(), h);
	addChild(mLabel);

	// FCA TopLeft
	float posX = Renderer::getScreenWidth() * 0.02f;
	float posY = Renderer::getScreenHeight() * 0.04f;

	setPosition(posX, posY, 0);
}

VolumeInfoComponent::~VolumeInfoComponent()
{
	delete mLabel;
	delete mFrame;
}

void VolumeInfoComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mDisplayTime >= 0)
	{
		mDisplayTime += deltaTime;
		if (mDisplayTime > VISIBLE_TIME + FADE_TIME)
		{
			mDisplayTime = -1;

			if (isVisible())
			{
				setVisible(false);
				PowerSaver::resume();
			}
		}
	}

	mCheckTime += deltaTime;
	if (mCheckTime < CHECKVOLUMEDELAY)
		return;

	mCheckTime = 0;

	int volume = VolumeControl::getInstance()->getVolume();
	if (volume != mVolume)
	{
		bool firstTime = (mVolume < 0);

		mVolume = volume;

		if (mVolume == 0)
			mLabel->setText("X");
		else
			mLabel->setText(std::to_string(mVolume) + "%");

		if (!firstTime)
		{
			mDisplayTime = 0;

			if (!isVisible())
			{
				setVisible(true);
				PowerSaver::pause();
			}
		}
	}
}

void VolumeInfoComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible || mDisplayTime < 0)
		return;

	auto theme = ThemeData::getMenuTheme();

	int opacity = 255 - Math::max(0, (mDisplayTime - VISIBLE_TIME) * 255 / FADE_TIME);
	setOpacity(opacity);
	setScale(opacity / 255.0f);

	GuiComponent::render(parentTrans);

	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);

	float x = PADDING_PX + PADDING_BAR;
	float y = PADDING_PX * 2;
	float w = getSize().x() - 2 * PADDING_PX - 2 * PADDING_BAR;
	float h = getSize().y() - mLabel->getSize().y() - PADDING_PX - PADDING_PX;
	
	Renderer::drawRect(x, y, w, h, (theme->Text.color & 0xFFFFFF00) | (opacity / mTrackDivider));

	float px = (h*mVolume) / 100;
	Renderer::drawRect(x, y + h - px, w, px, (mFillColor & 0xFFFFFF00) | opacity);
}
