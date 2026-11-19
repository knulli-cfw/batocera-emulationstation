#include "VolumeInfoComponent.h"
#include "resources/Font.h"
#include "renderers/Renderer.h"
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
	: GuiComponent(window), mFrame(nullptr), mLabel(nullptr), mThemed(false), mBackgroundColor(0), mBorderColor(0),
	mTrackColor(0), mCornerRadius(0), mGlyph(nullptr), mBarX(0), mBarWidth(0)
{
	mDisplayTime = -1;
	mVolume = -1;
	mCheckTime = 0;

	const Window::PopupStyle& style = window->getMeterStyle();
	if (style.enabled && style.font)
	{
		// The theme's pill: the glyph (volume or brightness), a horizontal bar and the level
		mThemed = true;
		mBackgroundColor = style.backgroundColor;
		mBorderColor = style.borderColor;
		mTrackColor = style.trackColor;
		mFillColor = style.fillColor;
		mTrackDivider = 1;

		float width = style.size.x();
		float height = style.size.y();
		mCornerRadius = style.cornerRadius > 0 ? style.cornerRadius : height / 2.0f;

		float padding = Math::round(height * 0.43f);
		float gap = Math::round(height * 0.3f);
		float glyphSize = style.iconSize > 0 ? style.iconSize : Math::round(height * 0.5f);
		float labelWidth = Math::round(style.font->sizeText("100 %").x());

		mBarX = padding + glyphSize + gap;
		mBarWidth = Math::max(8.0f, width - mBarX - gap - labelWidth - padding);
		setSize(width, height);

		// the glyphs come from the icon font ES falls back to, at the theme's icon size
		float scale = Renderer::ScreenSettings::fontScale();
		int glyphFontSize = Math::max(1, (int)Math::round(glyphSize / (scale > 0 ? scale : 1.0f)));
		mGlyph = new TextComponent(mWindow, "", Font::get(glyphFontSize, style.font->getPath()), style.color, ALIGN_CENTER);
		mGlyph->setPosition(padding, 0);
		mGlyph->setSize(glyphSize, height);
		addChild(mGlyph);

		mLabel = new TextComponent(mWindow, "", style.font, style.color, ALIGN_RIGHT);
		mLabel->setPosition(width - padding - labelWidth, 0);
		mLabel->setSize(labelWidth, height);
		addChild(mLabel);
		return;
	}

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

	moveToSlot(0);
}

void VolumeInfoComponent::moveToSlot(int slot)
{
	if (mThemed)
		return;

	// FCA TopLeft
	float posX = Renderer::getScreenWidth() * 0.02f + slot * (mSize.x() + Renderer::getScreenWidth() * 0.01f);
	float posY = Renderer::getScreenHeight() * 0.04f;

	setPosition(posX, posY, 0);
}

int VolumeInfoComponent::readLevel()
{
	return VolumeControl::getInstance()->getVolume();
}

std::string VolumeInfoComponent::glyph(int level)
{
	return level == 0 ? _U("\uF026") : _U("\uF028"); // a speaker, muted or not
}

VolumeInfoComponent::~VolumeInfoComponent()
{
	delete mGlyph;
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

	int volume = readLevel();
	if (volume != mVolume)
	{
		bool firstTime = (mVolume < 0) || !showChange();

		mVolume = volume;

		if (mThemed)
		{
			mGlyph->setText(glyph(mVolume));
			mLabel->setText(std::to_string(mVolume) + " %");
		}
		else if (mVolume == 0)
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

	if (mThemed)
	{
		// fades in place: the pill, the bar's track and fill, the glyph and the level
		setOpacity(opacity);

		auto withOpacity = [opacity](unsigned int color) { return (color & 0xFFFFFF00) | ((color & 0xFF) * opacity / 255); };

		Transform4x4f trans = parentTrans * getTransform();
		Renderer::setMatrix(trans);
		Renderer::drawSolidRectangle(0, 0, mSize.x(), mSize.y(), withOpacity(mBackgroundColor), withOpacity(mBorderColor),
			(mBorderColor & 0xFF) ? 1.0f : 0.0f, mCornerRadius);

		float h = Math::max(2.0f, Math::round(mSize.y() * 0.143f));
		float y = Math::round((mSize.y() - h) / 2.0f);
		Renderer::drawSolidRectangle(mBarX, y, mBarWidth, h, withOpacity(mTrackColor), 0, 0, h / 2.0f);

		float fill = Math::round(mBarWidth * Math::max(0, Math::min(100, mVolume)) / 100.0f);
		if (fill > 0)
			Renderer::drawSolidRectangle(mBarX, y, fill, h, withOpacity(mFillColor), 0, 0, h / 2.0f);

		GuiComponent::renderChildren(trans);
		return;
	}
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
