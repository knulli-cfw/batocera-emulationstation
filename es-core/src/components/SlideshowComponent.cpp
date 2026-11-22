#include "components/SlideshowComponent.h"

#include "components/GridTileComponent.h"
#include "components/SlideLayout.h"
#include "components/ImageAspectCheck.h"
#include "renderers/Renderer.h"
#include "resources/TextureResource.h"
#include "utils/StringUtil.h"
#include "ImageIO.h"
#include "ThemeData.h"
#include "Window.h"

SlideshowComponent::SlideshowComponent(Window* window) : GuiComponent(window),
	mShown(0), mScreenSaverActive(false), mColor(0xFFFFFFFF), mSaturation(1.0f), mRoundCorners(0.0f), mRoi(0, 0, 1, 1),
	mDots(false), mMaxDots(9), mDotSize(4.0f / 480.0f), mDotsOffset(9.0f / 640.0f, 9.0f / 480.0f),
	mDotColor(0xFFFFFF55), mDotSelectedColor(0xFFFFFFD0)
{
}

SlideshowComponent::~SlideshowComponent()
{
}

void SlideshowComponent::setPaths(const std::string& paths)
{
	if (paths == mPathsValue)
		return;

	mPathsValue = paths;
	mPaths.clear();

	for (auto line : Utils::String::split(paths, '\n', true))
	{
		line = Utils::String::trim(line);
		if (!line.empty())
			mPaths.push_back(line);
	}

	mTexture = nullptr;
	mNextTexture = nullptr;
	mClock.rewind();
	mShown = 0;
}

// Moves on the selected item of a grid only; anywhere else, always
bool SlideshowComponent::isActive()
{
	auto tile = dynamic_cast<GridTileComponent*>(getParent());
	return tile == nullptr || tile->isSelected();
}

std::shared_ptr<TextureResource> SlideshowComponent::loadTexture(const std::string& path)
{
	// Smoothed only when the capture is drawn smaller than its pixels
	unsigned int w = 0, h = 0;
	bool linear = false;
	if (ImageIO::loadImageSize(path, &w, &h) && w > 0 && h > 0)
		linear = SlideLayout::fit((float)w, (float)h, mRoi.x(), mRoi.y(), mRoi.z(), mRoi.w(), mSize.x(), mSize.y()).scale < 1.0f;

	return TextureResource::get(path, false, linear);
}

// Ready to slide in: loaded and with pixels (a file that is gone gives an empty texture)
static bool isReady(const std::shared_ptr<TextureResource>& texture)
{
	return texture != nullptr && texture->isLoaded() && texture->getPhysicalSize().x() > 0;
}

void SlideshowComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (!mVisible)
		return;

	// Paused under the screen saver and while the view is hidden (a game list, a game): no capture is loaded
	bool paused = mScreenSaverActive || !isShowing();
	bool sliding = mClock.progress() >= 0;
	int target = mClock.next();
	bool held = mWindow->isIdlePaused(Window::IDLE_SLIDES);
	auto request = mClock.update(deltaTime, (int)mPaths.size(), isActive(), paused, isReady(mNextTexture), held);

	if (mClock.current() != mShown)
	{
		// Slid to the next capture (its texture is the one that slid in), or back to the first one
		bool arrived = sliding && mClock.current() == target;
		mTexture = arrived ? mNextTexture : nullptr;
		mShown = mClock.current();
	}

	if (mClock.next() < 0 || mClock.next() >= (int)mPaths.size())
		mNextTexture = nullptr;
	else if (request == SlideLayout::Clock::LOAD)
	{
		// The next capture loads while this one shows
		mNextTexture = loadTexture(mPaths[mClock.next()]);
		if (mNextTexture != nullptr)
			mNextTexture->reload();
	}
	else if (request == SlideLayout::Clock::RELOAD && mNextTexture != nullptr)
		mNextTexture->reload();
}

void SlideshowComponent::drawSlide(const std::shared_ptr<TextureResource>& texture, float dx, const Transform4x4f& trans)
{
	if (texture == nullptr)
		return;

	auto size = texture->getPhysicalSize();
	if (size.x() <= 0 || size.y() <= 0)
		return;

	if (!texture->bind())
		return;

	auto fit = SlideLayout::fit(size.x(), size.y(), mRoi.x(), mRoi.y(), mRoi.z(), mRoi.w(), mSize.x(), mSize.y());

	float x = fit.x + dx;
	float y = fit.y;
	float r = x + fit.w;
	float b = y + fit.h;

	float opacity = getOpacity() / 255.0f;
	const unsigned int color = Renderer::convertColor((mColor & 0xFFFFFF00) | (unsigned char)((mColor & 0xFF) * opacity));

	Renderer::Vertex vertices[4];
	vertices[0] = { { x, y }, { 0.0f, 1.0f }, color };
	vertices[1] = { { x, b }, { 0.0f, 0.0f }, color };
	vertices[2] = { { r, y }, { 1.0f, 1.0f }, color };
	vertices[3] = { { r, b }, { 1.0f, 0.0f }, color };
	vertices[0].saturation = mSaturation;

	ImageAspectCheck::check(this, "slide", vertices, Vector2i((int)size.x(), (int)size.y()), trans);
	Renderer::setMatrix(trans);
	Renderer::drawTriangleStrips(&vertices[0], 4);
}

void SlideshowComponent::drawDots(int shown, const Transform4x4f& trans)
{
	int total = (int)mPaths.size();
	auto window = SlideLayout::dots(total, shown, mMaxDots);

	float dot = Math::max(1.0f, Math::round(mDotSize * Renderer::getScreenHeight()));
	float gap = Math::round(dot * 0.75f);
	float wide = dot * 2.5f;

	// Right-aligned at the offset from the top-right corner, the current dot wider; at the ends of a
	// window with more slides beyond, the dot is smaller
	float right = mSize.x() - Math::round(mDotsOffset.x() * Renderer::getScreenWidth());
	float top = Math::round(mDotsOffset.y() * Renderer::getScreenHeight());

	float x = right;
	Renderer::setMatrix(trans);

	for (int i = window.first + window.count - 1; i >= window.first; i--)
	{
		bool current = i == shown;
		bool small = !current && ((i == window.first && window.moreBefore) || (i == window.first + window.count - 1 && window.moreAfter));

		float h = small ? Math::max(1.0f, Math::round(dot * 0.6f)) : dot;
		float w = current ? wide : h;
		x -= w;

		unsigned int color = current ? mDotSelectedColor : mDotColor;
		unsigned char alpha = (unsigned char)((color & 0xFF) * (getOpacity() / 255.0f));
		Renderer::drawSolidRectangle(x, top + (dot - h) / 2.0f, w, h, (color & 0xFFFFFF00) | alpha, 0, 0, h / 2.0f);

		x -= gap;
	}
}

void SlideshowComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible || getOpacity() == 0)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	// The children (a screen filter, the card art) go over the captures, the dots over them
	if (mPaths.empty())
	{
		GuiComponent::renderChildren(trans);
		return;
	}

	int current = mClock.current() < (int)mPaths.size() ? mClock.current() : 0;
	if (mTexture == nullptr)
		mTexture = loadTexture(mPaths[current]);

	// Everything inside the box, its corners rounded
	Renderer::setMatrix(trans);
	if (mRoundCorners > 0)
		Renderer::enableRoundCornerStencil(0, 0, mSize.x(), mSize.y(), mRoundCorners < 1 ? Math::max(mSize.x(), mSize.y()) * mRoundCorners : mRoundCorners);
	else
		Renderer::pushClipRect(Vector2i((int)rect.x, (int)rect.y), Vector2i((int)rect.w, (int)rect.h));

	int shown = current;
	if (mClock.progress() >= 0 && mNextTexture != nullptr)
	{
		float t = SlideLayout::ease(mClock.progress());
		float dx = Math::round(t * mSize.x());
		drawSlide(mTexture, -dx, trans);
		drawSlide(mNextTexture, mSize.x() - dx, trans);

		if (t > 0.5f)
			shown = mClock.next();
	}
	else
		drawSlide(mTexture, 0, trans);

	Renderer::bindTexture(0);

	if (mRoundCorners > 0)
		Renderer::disableStencil();
	else
		Renderer::popClipRect();

	GuiComponent::renderChildren(trans);

	if (mDots && mPaths.size() > 1 && isActive())
		drawDots(shown, trans);
}

void SlideshowComponent::onHide()
{
	GuiComponent::onHide();

	// Back on the first capture next time, and nothing kept loaded for a hidden view
	mClock.rewind();
	mShown = 0;
	mTexture = nullptr;
	mNextTexture = nullptr;
}

void SlideshowComponent::onScreenSaverActivate()
{
	GuiComponent::onScreenSaverActivate();
	mScreenSaverActive = true;
}

void SlideshowComponent::onScreenSaverDeactivate()
{
	GuiComponent::onScreenSaverDeactivate();
	mScreenSaverActive = false;
}

void SlideshowComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	GuiComponent::applyTheme(theme, view, element, properties);

	int interval = 4000, duration = 600;
	if (elem->has("interval"))
		interval = (int)(Math::max(0.5f, elem->get<float>("interval")) * 1000.0f);

	if (elem->has("duration"))
		duration = (int)(Math::max(0.0f, elem->get<float>("duration")) * 1000.0f);

	mClock.setTiming(interval, duration);

	if (elem->has("color"))
		mColor = elem->get<unsigned int>("color");

	if (elem->has("saturation"))
		mSaturation = Math::clamp(elem->get<float>("saturation"), 0.0f, 1.0f);

	if (elem->has("roundCorners"))
		mRoundCorners = elem->get<float>("roundCorners");

	if (elem->has("roi"))
	{
		auto roi = elem->get<Vector4f>("roi");
		if (roi.z() > 0 && roi.w() > 0)
			mRoi = roi;
	}

	if (elem->has("dots"))
		mDots = elem->get<bool>("dots");

	if (elem->has("maxDots"))
		mMaxDots = Math::max(1, (int)elem->get<float>("maxDots"));

	if (elem->has("dotSize"))
		mDotSize = elem->get<float>("dotSize");

	if (elem->has("dotsOffset"))
		mDotsOffset = elem->get<Vector2f>("dotsOffset");

	if (elem->has("dotColor"))
		mDotColor = elem->get<unsigned int>("dotColor");

	if (elem->has("dotSelectedColor"))
		mDotSelectedColor = elem->get<unsigned int>("dotSelectedColor");

	if (elem->has("path"))
		setPaths(elem->get<std::string>("path"));
}

ThemeData::ThemeElement::Property SlideshowComponent::getProperty(const std::string name)
{
	if (name == "path")
		return mPathsValue;

	if (name == "saturation")
		return mSaturation;

	return GuiComponent::getProperty(name);
}

void SlideshowComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	if (name == "path" && value.type == ThemeData::ThemeElement::Property::PropertyType::String)
		setPaths(value.s);
	else if (name == "saturation" && value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
		mSaturation = Math::clamp(value.f, 0.0f, 1.0f);
	else
		GuiComponent::setProperty(name, value);
}
