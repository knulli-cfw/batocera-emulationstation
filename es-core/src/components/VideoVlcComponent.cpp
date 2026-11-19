#include "components/VideoVlcComponent.h"
#include "components/ImageAspectCheck.h"
#include "components/ScreenShape.h"

#include "renderers/Renderer.h"
#include "resources/TextureResource.h"
#include "utils/StringUtil.h"
#include "PowerSaver.h"
#include "Settings.h"
#include "Window.h"
#include <vlc/vlc.h>
#include <SDL_mutex.h>
#include <cmath>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <thread>
#include "SystemConf.h"
#include "ThemeData.h"
#include <SDL_timer.h>
#include "AudioManager.h"

#ifdef WIN32
#include <codecvt>
#endif

#include "ImageIO.h"
#include "Log.h"

#define MATHPI          3.141592653589793238462643383279502884L

libvlc_instance_t* VideoVlcComponent::mVLC = NULL;

// How long VLC may take to read a video's tracks; VLC counts it from when its reader takes the file, which
// may wait behind others
static const int PARSE_TIMEOUT_MS = 2000;
// How long ES waits for an answer, counted from the request (VLC never answering)
static const unsigned int PARSE_WAIT_MS = 10000;
// A file that timed out is not read again for a while, and counts as no video after this many timeouts: a
// timeout may come from a busy system, not from the file
static const unsigned int PARSE_RETRY_MS = 30000;
static const int PARSE_TIMEOUTS = 3;

// VLC prepares to render a video frame.
static void *lock(void *data, void **p_pixels) 
{
	struct VideoContext *c = (struct VideoContext *)data;
	
	int frame = (c->surfaceId ^ 1);
	
	c->mutexes[frame].lock();
	c->hasFrame[frame] = false;
	*p_pixels = c->surfaces[frame];
	return NULL; // Picture identifier, not needed here.
}

// VLC just rendered a video frame.
static void unlock(void *data, void* /*id*/, void *const* /*p_pixels*/) 
{
	struct VideoContext *c = (struct VideoContext *)data;

	int frame = (c->surfaceId ^ 1);	

	c->surfaceId = frame;
	c->hasFrame[frame] = true;
	c->mutexes[frame].unlock();
}

// VLC wants to display a video frame. This runs on VLC's thread: it only tells the UI thread
// (VideoVlcComponent::checkDisplayed), which may be freeing the component's texture.
static void display(void* data, void* id)
{
	if (data == NULL)
		return;

	((struct VideoContext *)data)->displayed = true;
}

// The players start and stop on one worker thread: making a VLC player opens its audio output (ALSA reads its
// configuration and probes every card, hundreds of ms on a handheld) and stopping one waits for its threads,
// so the UI thread only hands them over. A start sets the frame callbacks before it plays; a start its
// component gave up on before its turn (it stopped, or started another video) makes no player, and a player
// whose component stopped while it was made is stopped at once. The context goes with the player until it is
// released. The worker also releases every media: the last release of one VLC still reads waits for it.
struct PlayerStart
{
	libvlc_media_t*					media;
	std::shared_ptr<VideoContext>	context;
	unsigned int					width, height;
	bool							mute;

	std::mutex						lock;
	libvlc_media_player_t*			player;
	bool							ready;
	bool							cancelled;

	PlayerStart() : media(nullptr), width(0), height(0), mute(false), player(nullptr), ready(false), cancelled(false) { }
};

namespace
{
	class PlayerWorker
	{
	public:
		// The players' worker, and the one that drops parses VLC never answered: stopping such a parse waits
		// for VLC's reader, maybe for long, and must not hold the players' starts and stops. Never destroyed:
		// the workers outlive main's statics.
		static PlayerWorker* players()
		{
			static PlayerWorker* worker = new PlayerWorker();
			return worker;
		}

		static PlayerWorker* parses()
		{
			static PlayerWorker* worker = new PlayerWorker();
			return worker;
		}

		// false when the worker already has max tasks waiting (one stuck): the task is not taken
		bool post(const std::function<void()>& task, size_t max = 0)
		{
			std::unique_lock<std::mutex> lock(mLock);
			if (max > 0 && mTasks.size() >= max)
				return false;

			if (!mStarted)
			{
				mStarted = true;
				std::thread([this] { run(); }).detach();
			}

			mTasks.push_back(task);
			mWake.notify_one();
			return true;
		}

		// Waits until the tasks handed over so far ran, at most ms: false when some still run
		bool drain(int ms)
		{
			{
				std::unique_lock<std::mutex> lock(mLock);
				if (!mStarted)
					return true;
			}

			struct Done
			{
				std::mutex lock;
				std::condition_variable wake;
				bool done = false;
			};

			auto done = std::make_shared<Done>();
			post([done]
			{
				std::unique_lock<std::mutex> lock(done->lock);
				done->done = true;
				done->wake.notify_all();
			});

			std::unique_lock<std::mutex> lock(done->lock);
			return done->wake.wait_for(lock, std::chrono::milliseconds(ms), [done] { return done->done; });
		}

	private:
		PlayerWorker() : mStarted(false) { }

		void run()
		{
			for (;;)
			{
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(mLock);
					mWake.wait(lock, [this] { return !mTasks.empty(); });
					task = mTasks.front();
					mTasks.pop_front();
				}

				task();
			}
		}

		std::mutex mLock;
		std::condition_variable mWake;
		std::deque<std::function<void()>> mTasks;
		bool mStarted;
	};

	// The players alive (made and not yet released), in the debug log when it changes (tests)
	std::atomic<int> sPlayers(0);

	void countPlayers(int change)
	{
		int players = (sPlayers += change);
		LOG(LogDebug) << "VideoVlcComponent: " << players << " players";
	}

	void stopOnWorker(libvlc_media_player_t* player, std::shared_ptr<VideoContext> context)
	{
		PlayerWorker::players()->post([player, context]
		{
			libvlc_media_player_stop(player);
			libvlc_media_player_release(player);
			countPlayers(-1);
		});
	}

	void releaseOnWorker(libvlc_media_t* media)
	{
		PlayerWorker::players()->post([media] { libvlc_media_release(media); });
	}

	// Stopping a parse waits for VLC's reader: on the parses' worker. Should VLC never let go of one, at most 16
	// wait behind it; any more are left to VLC as they are (not stopped, not released)
	void stopParseOnWorker(libvlc_media_t* media)
	{
		bool taken = PlayerWorker::parses()->post([media]
		{
			libvlc_media_parse_stop(media);
			libvlc_media_release(media);
		}, 16);

		if (!taken)
			LOG(LogWarning) << "VideoVlcComponent: VLC's reader does not stop; a parse is left to it";
	}

	// Files whose parse timed out: how many times, and when they may be read again (SDL ticks). UI thread.
	std::map<std::string, std::pair<int, unsigned int>> sParseTimeouts;

	bool waitsAfterTimeout(const std::string& path)
	{
		auto it = sParseTimeouts.find(path);
		return it != sParseTimeouts.cend() && (int)(it->second.second - SDL_GetTicks()) > 0;
	}

	void startOnWorker(std::shared_ptr<PlayerStart> start)
	{
		PlayerWorker::players()->post([start]
		{
			{
				std::unique_lock<std::mutex> lock(start->lock);
				if (start->cancelled)
				{
					libvlc_media_release(start->media);
					start->media = nullptr;
					start->context = nullptr;
					return;
				}
			}

			libvlc_media_player_t* player = libvlc_media_player_new_from_media(start->media);
			libvlc_media_release(start->media);
			start->media = nullptr;

			if (player != nullptr)
				countPlayers(1);

			if (player != nullptr)
			{
				if (start->mute)
					libvlc_audio_set_mute(player, 1);

				if (start->width > 1)
				{
					libvlc_video_set_callbacks(player, lock, unlock, display, (void*)start->context.get());
					libvlc_video_set_format(player, "RGBA", (int)start->width, (int)start->height, (int)start->width * 4);
				}

				libvlc_media_player_play(player);
			}

			std::unique_lock<std::mutex> lock(start->lock);
			if (start->cancelled)
			{
				if (player != nullptr)
				{
					libvlc_media_player_stop(player);
					libvlc_media_player_release(player);
					countPlayers(-1);
				}

				start->context = nullptr;
				return;
			}

			start->player = player;
			start->ready = true;
		});
	}
}

VideoVlcComponent::VideoVlcComponent(Window* window) : VideoComponent(window), 
	mMediaPlayer(nullptr), mMedia(nullptr), mParsing(false), mParseEnd(0),
	mTopLeftCrop(0.0f, 0.0f), mBottomRightCrop(1.0f, 1.0f)
{
	mSaturation = 1.0f;
	mElapsed = 0;
	mColorShift = 0xFFFFFFFF;
	mLinearSmooth = false;

	mLoops = -1;
	mCurrentLoop = 0;

	// Get an empty texture for rendering the video
	mTexture = nullptr;// TextureResource::get("");
	mEffect = VideoVlcFlags::VideoVlcEffect::BUMP;

	// Make sure VLC has been initialised
	init();
}

// The videos showing their frames (main thread): a screen shows one at most, so a second one is logged
static std::set<VideoVlcComponent*> sShownVideos;

static void videoShown(VideoVlcComponent* video, bool shown)
{
	if (!shown)
	{
		sShownVideos.erase(video);
		return;
	}

	sShownVideos.insert(video);
	if (sShownVideos.size() < 2)
		return;

	std::string paths;
	for (auto v : sShownVideos)
		paths += (paths.empty() ? "" : ", ") + v->getVideoPath();

	LOG(LogWarning) << "VideoCheck: " << sShownVideos.size() << " videos play at once: " << paths;
}

VideoVlcComponent::~VideoVlcComponent()
{
	stopVideo();
	videoShown(this, false);
}

Vector2f VideoVlcComponent::getSize() const
{
	if (mTargetIsMax && mPadding != Vector4f::Zero())
	{
		auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();

		if (mSize.x() == targetSize.x())
			return Vector2f(mSize.x() + mPadding.x() + mPadding.z(), mSize.y());
		else if (mSize.y() == targetSize.y())
			return Vector2f(mSize.x(), mSize.y() + mPadding.y() + mPadding.w());
	}

	return GuiComponent::getSize() * (mBottomRightCrop - mTopLeftCrop);
}

void VideoVlcComponent::setResize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && !mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mSize = mTargetSize;
	mTargetIsMax = false;
	mTargetIsMin = false;

	// the snapshot takes the same shape as the video: the size's, turned to its own orientation
	if (mKeepOrientation)
	{
		mStaticImage.setKeepOrientation(true);
		mStaticImage.setResize(width, height);
	}
	else
		mStaticImage.setMaxSize(width, height);

	resize();
}

void VideoVlcComponent::setMaxSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mSize = mTargetSize;
	mTargetIsMax = true;
	mTargetIsMin = false;
	mStaticImage.setMaxSize(width, height);
	resize();
}

void VideoVlcComponent::setMinSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMin && !mTargetIsMax && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mSize = mTargetSize;
	mTargetIsMax = false;
	mTargetIsMin = true;
	mStaticImage.setMaxSize(width, height);
	resize();
}

void VideoVlcComponent::onVideoStarted()
{
	VideoComponent::onVideoStarted();
	resize();
	videoShown(this, true);
}

void VideoVlcComponent::crop(float left, float top, float right, float bot)
{
	mTopLeftCrop.x() = Math::clamp(left, 0.0f, 1.0f);
	mTopLeftCrop.y() = Math::clamp(top, 0.0f, 1.0f);
	mBottomRightCrop.x() = 1.0f - Math::clamp(right, 0.0f, 1.0f);
	mBottomRightCrop.y() = 1.0f - Math::clamp(bot, 0.0f, 1.0f);
}

void VideoVlcComponent::resize()
{
	if (!mTexture)
		return;

	const Vector2f textureSize((float)mVideoWidth, (float)mVideoHeight);

	if (textureSize == Vector2f::Zero())
		return;

	auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();

	if (mTargetIsMax)
	{
		crop(0, 0, 0, 0);
		mSize = textureSize;

		Vector2f resizeScale((targetSize.x() / mSize.x()), (targetSize.y() / mSize.y()));

		if (resizeScale.x() < resizeScale.y())
		{
			mSize[0] *= resizeScale.x(); // this will be mTargetSize.x(). We can't exceed it, nor be lower than it.
			// we need to make sure we're not creating an image larger than max size
			//mSize[1] = Math::min(Math::round(mSize[1] *= resizeScale.x()), mTargetSize.y());
			mSize[1] = Math::min(mSize[1] *= resizeScale.x(), targetSize.y());
		}
		else
		{
			//mSize[1] = Math::round(mSize[1] * resizeScale.y()); // this will be mTargetSize.y(). We can't exceed it.
			mSize[1] = mSize[1] * resizeScale.y(); // this will be mTargetSize.y(). We can't exceed it.

			// for SVG rasterization, always calculate width from rounded height (see comment above)
			// we need to make sure we're not creating an image larger than max size
			mSize[0] = Math::min((mSize[1] / textureSize.y()) * textureSize.x(), targetSize.x());
		}
	}
	else if (mTargetIsMin)
	{
		// mSize = ImageIO::getPictureMinSize(textureSize, mTargetSize);			
		mSize = textureSize;

		Vector2f resizeScale((targetSize.x() / mSize.x()), (targetSize.y() / mSize.y()));

		if (resizeScale.x() > resizeScale.y())
		{
			mSize[0] *= resizeScale.x();
			mSize[1] *= resizeScale.x();

			float cropPercent = (mSize.y() - targetSize.y()) / (mSize.y() * 2);
			crop(0, cropPercent, 0, cropPercent);
		}
		else
		{
			mSize[0] *= resizeScale.y();
			mSize[1] *= resizeScale.y();

			float cropPercent = (mSize.x() - targetSize.x()) / (mSize.x() * 2);
			crop(cropPercent, 0, cropPercent, 0);
		}

		// for SVG rasterization, always calculate width from rounded height (see comment above)
		// we need to make sure we're not creating an image smaller than min size
		// mSize[1] = Math::max(Math::round(mSize[1]), mTargetSize.y());
		// mSize[0] = Math::max((mSize[1] / textureSize.y()) * textureSize.x(), mTargetSize.x());
	}
	else
	{
		crop(0, 0, 0, 0);
		// if both components are set, we just stretch
		// if no components are set, we don't resize at all
		mSize = targetSize == Vector2f::Zero() ? textureSize : targetSize;

		if (mKeepOrientation && targetSize.x() > 0 && targetSize.y() > 0)
			mSize = ScreenShape::fit(targetSize, textureSize);

		// if only one component is set, we resize in a way that maintains aspect ratio
		// for SVG rasterization, we always calculate width from rounded height (see comment above)
		if (!targetSize.x() && targetSize.y())
		{
			//mSize[1] = Math::round(mTargetSize.y());
			mSize[1] = targetSize.y();
			mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
		}
		else if (targetSize.x() && !targetSize.y())
		{
			//mSize[1] = Math::round((mTargetSize.x() / textureSize.x()) * textureSize.y());
			mSize[1] = (targetSize.x() / textureSize.x()) * textureSize.y();
			mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
		}
	}

	mTexture->rasterizeAt((size_t)Math::round(mSize.x()), (size_t)Math::round(mSize.y()));
	onSizeChanged();
}

void VideoVlcComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	updateVertices();
}

void VideoVlcComponent::onPaddingChanged()
{
	GuiComponent::onPaddingChanged();
	resize();
	updateVertices();
}

void VideoVlcComponent::setColorShift(unsigned int color)
{
	mColorShift = color;
}

void VideoVlcComponent::updateVertices()
{
	if (!mTexture)
		return;

	auto textureSize = mTexture->getSize();

	Vector2f     topLeft = mSize * mTopLeftCrop;
	Vector2f     bottomRight = mSize * mBottomRightCrop;

	Vector2f paddingOffset;

	if (mPadding != Vector4f::Zero())
	{
		paddingOffset = mPadding.xy() - (mPadding.xy() + mPadding.zw()) * mOrigin;
		topLeft += paddingOffset;
		bottomRight += paddingOffset;
	}

	const float        px = mTexture->isTiled() ? mSize.x() / textureSize.x() : 1.0f;
	const float        py = mTexture->isTiled() ? mSize.y() / textureSize.y() : 1.0f;

	const unsigned int color = Renderer::convertColor(mColorShift);

	mVertices[0] = {
		{ topLeft.x(),					topLeft.y()	 },
		{ mTopLeftCrop.x(),				1.0f - mBottomRightCrop.y()    },
		color };

	mVertices[1] = {
		{ topLeft.x(),					bottomRight.y() },
		{ mTopLeftCrop.x(),				py - mTopLeftCrop.y() },
		color };

	mVertices[2] = {
		{ bottomRight.x(),				topLeft.y()	},
		{ mBottomRightCrop.x() * px,	1.0f - mBottomRightCrop.y()     },
		color };

	mVertices[3] = {
		{ bottomRight.x(),				bottomRight.y() },
		{ mBottomRightCrop.x() * px,    py - mTopLeftCrop.y() },
		color };

	// Fix vertices for min Target
	if (mTargetIsMin)
	{		
		auto targetSize = mTargetSize - mPadding.xy() - mPadding.zw();
		Vector2f targetSizePos = (mSize - targetSize) * mOrigin + paddingOffset;

		float x = targetSizePos.x();
		float y = targetSizePos.y();
		float r = x + targetSize.x();
		float b = y + targetSize.y();

		mVertices[0].pos[0] = x;
		mVertices[0].pos[1] = y;

		mVertices[1].pos[0] = x;
		mVertices[1].pos[1] = b;

		mVertices[2].pos[0] = r;
		mVertices[2].pos[1] = y;

		mVertices[3].pos[0] = r;
		mVertices[3].pos[1] = b;
	}

	/*
	// round vertices
	for (int i = 0; i < 4; ++i)
		mVertices[i].pos.round();
	*/
	/*
	if (mFlipX)
	{
		for (int i = 0; i < 4; ++i)
			mVertices[i].tex[0] = px - mVertices[i].tex[0];
	}

	if (mFlipY)
	{
		for (int i = 0; i < 4; ++i)
			mVertices[i].tex[1] = py - mVertices[i].tex[1];
	}
	*/
	updateColors();
	updateRoundCorners();	
}

void VideoVlcComponent::updateColors()
{
	float t = mFadeIn;
	if (mFadeIn < 1.0)
	{
		t = 1.0 - mFadeIn;
		t -= 1; // cubic ease in
		t = Math::lerp(0, 1, t * t * t + 1);
		t = 1.0 - t;
	}

	float opacity = (getOpacity() / 255.0f) * t;

	if (hasStoryBoard() && currentStoryBoardHasProperty("opacity") && isStoryBoardRunning())
		opacity = (getOpacity() / 255.0f);

	unsigned int color = Renderer::convertColor(mColorShift & 0xFFFFFF00 | (unsigned char)((mColorShift & 0xFF) * opacity));

	mVertices[0].col = color;
	mVertices[1].col = color;
	mVertices[2].col = color;
	mVertices[3].col = color;
}

void VideoVlcComponent::setRoundCorners(float value)
{
	if (mRoundCorners == value)
		return;

	VideoComponent::setRoundCorners(value);
	updateRoundCorners();
}

void VideoVlcComponent::updateRoundCorners()
{
	if (mRoundCorners <= 0 || Renderer::shaderSupportsCornerSize(mCustomShader.path))
	{
		mRoundCornerStencil.clear();
		return;
	}

	float x = 0;
	float y = 0;
	float size_x = mSize.x();
	float size_y = mSize.y();

	if (mTargetIsMin)
	{
		Vector2f targetSizePos = (mTargetSize - mSize) * mOrigin * -1;

		x = targetSizePos.x();
		y = targetSizePos.y();
		size_x = mTargetSize.x();
		size_y = mTargetSize.y();
	}

	float radius = mRoundCorners < 1 ? Math::max(size_x, size_y) * mRoundCorners : mRoundCorners;
	mRoundCornerStencil = Renderer::createRoundRect(x, y, size_x, size_y, radius);
}

void VideoVlcComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	// Out of view but drawn (a view sliding away or in): its snapshot, not the stopped player
	if (!isShowing())
	{
		VideoComponent::render(parentTrans);
		return;
	}

	VideoComponent::render(parentTrans);

	bool initFromPixels = true;

	if (!mIsPlaying || mContext == nullptr || !mContext->valid)
	{
		// If video is still attached to the path & texture is initialized, we suppose it had just been stopped (onhide, ondisable, screensaver...)
		// still render the last frame
		if (mTexture != nullptr && !mVideoPath.empty() && mPlayingVideoPath == mVideoPath && mTexture->isLoaded())
			initFromPixels = false;
		else
			return;
	}

	float t = mFadeIn;
	if (mFadeIn < 1.0)
	{
		t = 1.0 - mFadeIn;
		t -= 1; // cubic ease in
		t = Math::lerp(0, 1, t*t*t + 1);
		t = 1.0 - t;
	}

	if (t == 0.0)
		return;
		
	Transform4x4f trans = parentTrans * getTransform();
	
	if (mRotation == 0 && !mTargetIsMin)
	{
		auto rect = Renderer::getScreenRect(trans, mSize);
		if (!Renderer::isVisibleOnScreen(rect))
			return;
	}

	// Build a texture for the video frame
	if (initFromPixels)
	{
#ifdef _RPI_
		// Rpi : A lot of videos are encoded in 60fps on screenscraper
		// Try to limit transfert to opengl textures to 30fps to save CPU
		if (!Settings::getInstance()->getBool("OptimizeVideo") || mElapsed >= 40) // 40ms = 25fps, 33.33 = 30 fps
#endif
		{
			bool created = (mTexture == nullptr);
			if (uploadFrame())
			{
				if (created)
					trans = parentTrans * getTransform();

				mElapsed = 0;
			}
		}
	}

	if (mTexture == nullptr)
		return;

	updateColors();

	bool isDefaultEffectDisabled = hasStoryBoard() && currentStoryBoardHasProperty("scale") && isStoryBoardRunning();

	/*if (mEffect == VideoVlcFlags::VideoVlcEffect::SLIDERIGHT && mFadeIn > 0.0 && mFadeIn < 1.0 && mConfig.startDelay > 0 && !isDefaultEffectDisabled)
	{
		float t = 1.0 - mFadeIn;
		t -= 1;
		t = Math::lerp(0, 1, t*t*t + 1);

		vertices[0] = { { 0.0f     , 0.0f      }, { t, 0.0f }, color };
		vertices[1] = { { 0.0f     , mSize.y() }, { t, 1.0f }, color };
		vertices[2] = { { mSize.x(), 0.0f      }, { t + 1.0f, 0.0f }, color };
		vertices[3] = { { mSize.x(), mSize.y() }, { t + 1.0f, 1.0f }, color };
	}
	else*/
	if (mEffect == VideoVlcFlags::VideoVlcEffect::SIZE && mFadeIn > 0.0 && mFadeIn < 1.0 && mConfig.startDelay > 0 && !isDefaultEffectDisabled)
	{		
		float bump = Math::easeOutCubic(mFadeIn);

		auto scale = mScale;
		mScale = mScale * bump;
		mTransformDirty = true;
		trans = parentTrans * getTransform();
		mScale = scale;
		mTransformDirty = true;
	}
	else if (mEffect == VideoVlcFlags::VideoVlcEffect::BUMP && mFadeIn > 0.0 && mFadeIn < 1.0 && mConfig.startDelay > 0 && !isDefaultEffectDisabled)
	{
		float bump = sin((MATHPI / 2.0) * mFadeIn) + sin(MATHPI * mFadeIn) / 2.0;

		auto scale = mScale;
		mScale = mScale * bump;
		mTransformDirty = true;
		trans = parentTrans * getTransform();
		mScale = scale;
		mTransformDirty = true;
	}

	// round vertices
	// for (int i = 0; i < 4; ++i)
	//	vertices[i].pos.round();
	
	if (mTexture->bind())
	{
		ImageAspectCheck::check(this, mVideoPath, mVertices, mTexture->getSize(), trans);
		Renderer::setMatrix(trans);

		beginCustomClipRect();

		Vector2f targetSizePos = (mTargetSize - mSize) * mOrigin * -1;
		
		// Render it
		mVertices->saturation = mSaturation;
		mVertices->customShader = mCustomShader.path.empty() ? nullptr : &mCustomShader;
	
		if (mRoundCorners > 0 && mRoundCornerStencil.size() > 0)
		{
			Renderer::setStencil(mRoundCornerStencil.data(), mRoundCornerStencil.size());
			Renderer::drawTriangleStrips(&mVertices[0], 4);
			Renderer::disableStencil();
		}
		else
		{
			mVertices->cornerRadius = mRoundCorners < 1 ? Math::max(mSize.x(), mSize.y()) * mRoundCorners : mRoundCorners;
			Renderer::drawTriangleStrips(&mVertices[0], 4);
		}

		endCustomClipRect();

		Renderer::bindTexture(0);
	}
}

void VideoVlcComponent::setupContext()
{
	// The frames' buffers are made for one size
	if (mContext != nullptr && mContext->valid && mContext->width == mVideoWidth && mContext->height == mVideoHeight)
		return;

	freeContext();

	// Create an RGBA surface to render the video into
	mContext = std::make_shared<VideoContext>();
	mContext->surfaces[0] = new unsigned char[mVideoWidth * mVideoHeight * 4];
	mContext->surfaces[1] = new unsigned char[mVideoWidth * mVideoHeight * 4];
	mContext->width = mVideoWidth;
	mContext->height = mVideoHeight;
	mContext->valid = true;
	resize();	
}

void VideoVlcComponent::freeContext()
{
	if (mContext == nullptr || !mContext->valid)
		return;

	if (mIsTopWindow)
	{
		// Release texture memory -> except if mDisable by topWindow ( ex: menu was poped )
		mTexture = nullptr;
	}

	// The player may still draw into its buffers until the worker stopped it: they go with the last of them
	mContext->valid = false;
	mContext = nullptr;
}

#if WIN32
#include <Windows.h>
#pragma comment(lib, "Version.lib")

// If Vlc2 dlls have been upgraded with vlc3 dlls, libqt4_plugin.dll is not compatible, so check if libvlc is 3.x then delete obsolete libqt4_plugin.dll
void _checkUpgradedVlcVersion()
{
	char str[1024] = { 0 };
	if (GetModuleFileNameA(NULL, str, 1024) == 0)
		return;

	auto dir = Utils::FileSystem::getParent(str);
	auto path = Utils::FileSystem::getPreferredPath(Utils::FileSystem::combine(dir, "libvlc.dll"));
	if (Utils::FileSystem::exists(path))
	{
		// Get the version information for the file requested
		DWORD dwSize = GetFileVersionInfoSize(path.c_str(), NULL);
		if (dwSize == 0)
		{
			printf("Error in GetFileVersionInfoSize: %d\n", GetLastError());
			return;
		}

		BYTE                *pbVersionInfo = NULL;
		VS_FIXEDFILEINFO    *pFileInfo = NULL;
		UINT                puLenFileInfo = 0;

		pbVersionInfo = new BYTE[dwSize];

		if (!GetFileVersionInfo(path.c_str(), 0, dwSize, pbVersionInfo))
		{
			printf("Error in GetFileVersionInfo: %d\n", GetLastError());
			delete[] pbVersionInfo;
			return;
		}

		if (!VerQueryValue(pbVersionInfo, TEXT("\\"), (LPVOID*)&pFileInfo, &puLenFileInfo))
		{
			printf("Error in VerQueryValue: %d\n", GetLastError());
			delete[] pbVersionInfo;
			return;
		}

		// FileVersion for libvlc.dll is >= 3.x.x.x ???
		if (HIWORD(pFileInfo->dwFileVersionMS) >= 3)
		{
			auto badV2PluginPath = Utils::FileSystem::getPreferredPath(Utils::FileSystem::combine(dir, "plugins/gui/libqt4_plugin.dll"));
			if (Utils::FileSystem::exists(badV2PluginPath))
				Utils::FileSystem::removeFile(badV2PluginPath);
		}
	}
}
#endif

void VideoVlcComponent::init()
{
	if (mVLC != nullptr)
		return;

	std::vector<std::string> cmdline;
	cmdline.push_back("--quiet");
	cmdline.push_back("--no-video-title-show");

	std::string commandLine = SystemConf::getInstance()->get("vlc.commandline");
	if (!commandLine.empty())
	{
		std::vector<std::string> tokens = Utils::String::split(commandLine, ' ');
		for (auto token : tokens)
			cmdline.push_back(token);
	}

	const char* *theArgs = new const char*[cmdline.size()];

	for (int i = 0 ; i < cmdline.size() ; i++)
		theArgs[i] = cmdline[i].c_str();

#if WIN32
	_checkUpgradedVlcVersion();
#endif

	mVLC = libvlc_new(cmdline.size(), theArgs);

	delete[] theArgs;
}

bool VideoVlcComponent::waitForPlayers(int ms)
{
	int start = SDL_GetTicks();
	bool done = PlayerWorker::players()->drain(ms);
	if (done)
		LOG(LogDebug) << "VideoVlcComponent: players stopped in " << (SDL_GetTicks() - start) << " ms";
	else
		LOG(LogWarning) << "VideoVlcComponent: players still stopping after " << ms << " ms";

	return done;
}

// Copies the newest whole frame into the texture, under that frame's lock: the player writes into a buffer only
// while it holds its lock, so the texture never gets a frame half written. False when there was no new frame.
bool VideoVlcComponent::uploadFrame()
{
	if (mContext == nullptr || !mContext->valid)
		return false;

	int frame = mContext->surfaceId;
	if (!mContext->hasFrame[frame])
		return false;

	std::unique_lock<std::mutex> lock(mContext->mutexes[frame]);
	if (!mContext->hasFrame[frame])
		return false;

	if (mTexture == nullptr)
	{
		mTexture = TextureResource::get("", false, mLinearSmooth);
		resize();
	}

	mTexture->updateFromExternalPixels(mContext->surfaces[frame], mVideoWidth, mVideoHeight);
	mContext->hasFrame[frame] = false;
	return true;
}

// The first frame VLC displayed: the video shows from now on, once that frame is whole in the texture (until
// then the snapshot, or what is under the video, stays)
void VideoVlcComponent::checkDisplayed()
{
	if (mContext == nullptr || !mContext->valid || !mContext->displayed || mIsPlaying || !mIsWaitingForVideoToStart)
		return;

	if (!uploadFrame() && (mTexture == nullptr || !mTexture->isLoaded()))
		return;

	onVideoStarted();
}

void VideoVlcComponent::handleLooping()
{
	checkParse();
	checkStart();
	checkDisplayed();

	if (mIsPlaying && mMediaPlayer)
	{
		libvlc_state_t state = libvlc_media_player_get_state(mMediaPlayer);
		if (state == libvlc_Ended)
		{
			if (mLoops >= 0)
			{
				mCurrentLoop++;
				if (mCurrentLoop > mLoops)
				{
					stopVideo();

					mFadeIn = 0.0;
					mPlayingVideoPath = "";
					mVideoPath = "";
					return;
				}
			}

			// The idle pause: a view's video ends its loop on its capture, and the panel moves on to no other
			if (mIdlePause && !mScreensaverMode && mWindow->isIdlePaused(Window::IDLE_VIDEO))
			{
				stopVideo();
				mFadeIn = 0.0;
				mIdleStopped = true;
				return;
			}

			if (mPlaylist != nullptr)
			{
				auto nextVideo = mPlaylist->getNextItem();
				if (!nextVideo.empty())
				{
					stopVideo();
					setVideo(nextVideo);
					return;
				}
				else
					mPlaylist = nullptr;
			}
			
			if (mVideoEnded != nullptr)
			{
				bool cont = mVideoEnded();
				if (!cont)
				{
					stopVideo();
					return;
				}
			}

			if (!getPlayAudio() || (!mScreensaverMode && !Settings::getInstance()->getBool("VideoAudio")) || (Settings::getInstance()->getBool("ScreenSaverVideoMute") && mScreensaverMode))
				libvlc_audio_set_mute(mMediaPlayer, 1);

			//libvlc_media_player_set_position(mMediaPlayer, 0.0f);
			if (mMedia)
				libvlc_media_player_set_media(mMediaPlayer, mMedia);

			libvlc_media_player_play(mMediaPlayer);
		}
	}
}

void VideoVlcComponent::startVideo()
{
	// Not again while VLC reads the file (checkParse goes on)
	if (mIsPlaying || mParsing)
		return;

	// A start before this one may still hold a player being made, a player, its media or its frames (the
	// component was asked to start again before the first one showed): they go first, so no player is left
	// without its component and no buffer is reused for another size
	if (mStart != nullptr || mMediaPlayer != nullptr || mMedia != nullptr || mContext != nullptr)
		releasePlayback();

	if (hasStoryBoard("", true) && mConfig.startDelay > 0)
		startStoryboard();

	mTexture = nullptr;
	mCurrentLoop = 0;
	mVideoWidth = 0;
	mVideoHeight = 0;

#ifdef WIN32
	std::string path(Utils::String::replace(mVideoPath, "/", "\\"));
#else
	std::string path(mVideoPath);
#endif
	// Make sure we have a video path, and not one VLC could not read before
	if (mVLC && (path.size() > 0) && !isUnplayable(mVideoPath) && !waitsAfterTimeout(mVideoPath))
	{
		// Set the video that we are going to be playing so we don't attempt to restart it
		mPlayingVideoPath = mVideoPath;

		// Open the media
		mMedia = libvlc_media_new_path(mVLC, path.c_str());
		if (mMedia)
		{			
			// use : vlc –long-help
			// WIN32 ? libvlc_media_add_option(mMedia, ":avcodec-hw=dxva2");
			// RPI/OMX ? libvlc_media_add_option(mMedia, ":codec=mediacodec,iomx,all"); .

			std::string options = SystemConf::getInstance()->get("vlc.options");
			if (!options.empty())
			{
				std::vector<std::string> tokens = Utils::String::split(options, ' ');
				for (auto token : tokens)
					libvlc_media_add_option(mMedia, token.c_str());
			}
			
			// If we have a playlist : most videos have a fader, skip it 1 second
			if (mPlaylist != nullptr && mConfig.startDelay == 0 && !mConfig.showSnapshotDelay && !mConfig.showSnapshotNoVideo)
				libvlc_media_add_option(mMedia, ":start-time=0.7");			

			Probe probe;
			if (findProbe(mVideoPath, probe))
			{
				startPlayback(probe);
				return;
			}

#ifdef WIN32
			// It looks like an older version of the library is being used on Windows.
			libvlc_media_parse(mMedia);
			mParsing = true;
			mParseEnd = SDL_GetTicks();
			checkParse();
#else
			// VLC reads the tracks on its own thread, with a time limit: checkParse looks each frame, and a file
			// VLC cannot read (empty, truncated, not a video) fails, is skipped or times out
			libvlc_media_parse_with_options(mMedia, libvlc_media_parse_local, PARSE_TIMEOUT_MS);
			mParsing = true;
			mParseEnd = SDL_GetTicks() + PARSE_WAIT_MS;
#endif
		}
	}
}

static std::map<std::string, std::pair<std::string, VideoVlcComponent::Probe>> sProbes;	// path: size and date, probe

// The file's size and date, so a replaced file is read again
static std::string fileStamp(const std::string& path)
{
	return std::to_string(Utils::FileSystem::getFileSize(path)) + " " + Utils::FileSystem::getFileModificationDate(path).getIsoString();
}

bool VideoVlcComponent::findProbe(const std::string& path, Probe& probe)
{
	auto it = sProbes.find(path);
	if (it == sProbes.cend() || it->second.first != fileStamp(path))
		return false;

	probe = it->second.second;
	return true;
}

void VideoVlcComponent::keepProbe(const std::string& path, const Probe& probe)
{
	sProbes[path] = std::make_pair(fileStamp(path), probe);
}

void VideoVlcComponent::checkParse()
{
	if (!mParsing || mMedia == nullptr)
		return;

	libvlc_media_parsed_status_t parsed = libvlc_media_get_parsed_status(mMedia);
#ifndef WIN32
	bool answered = parsed == libvlc_media_parsed_status_done || parsed == libvlc_media_parsed_status_failed ||
		parsed == libvlc_media_parsed_status_timeout || parsed == libvlc_media_parsed_status_skipped;

	if (!answered && (int)(mParseEnd - SDL_GetTicks()) > 0)
		return;
#endif

	mParsing = false;

	Probe probe = { false, 0, 0, false };
#ifndef WIN32
	// Anything but a parsed file is no video now: the snapshot shows. A file VLC failed on is not tried again
	// (while it keeps its size), so there is no restart loop; one that timed out waits PARSE_RETRY_MS, and
	// counts as no video only after PARSE_TIMEOUTS timeouts
	if (parsed != libvlc_media_parsed_status_done)
	{
		bool timedOut = !answered || parsed == libvlc_media_parsed_status_timeout;
		LOG(LogWarning) << "VideoVlcComponent: cannot read " << mPlayingVideoPath << " (parse status " << (int)parsed << (answered ? "" : ", no answer") << ")";

		if (timedOut)
		{
			auto& timeouts = sParseTimeouts[mPlayingVideoPath];
			timeouts.first++;
			timeouts.second = SDL_GetTicks() + PARSE_RETRY_MS;
			if (timeouts.first >= PARSE_TIMEOUTS)
				markUnplayable(mPlayingVideoPath);
		}
		else
			markUnplayable(mPlayingVideoPath);

		if (answered)
			releaseOnWorker(mMedia);
		else
			stopParseOnWorker(mMedia);

		mMedia = NULL;
		return;
	}

	sParseTimeouts.erase(mPlayingVideoPath);
#endif

	probe.readable = true;

	libvlc_media_track_t** tracks;
	unsigned track_count = libvlc_media_tracks_get(mMedia, &tracks);
	for (unsigned track = 0; track < track_count; ++track)
	{
		if (tracks[track]->i_type == libvlc_track_audio)
			probe.audio = true;
		else if (tracks[track]->i_type == libvlc_track_video && probe.width == 0)
		{
			probe.width = tracks[track]->video->i_width;
			probe.height = tracks[track]->video->i_height;
		}
	}
	libvlc_media_tracks_release(tracks, track_count);

	keepProbe(mPlayingVideoPath, probe);
	startPlayback(probe);
}

void VideoVlcComponent::startPlayback(const Probe& probe)
{
	std::string path(mPlayingVideoPath);
	bool hasAudioTrack = probe.audio;
	mVideoWidth = probe.width;
	mVideoHeight = probe.height;

	if (mVideoWidth == 0 && mVideoHeight == 0 && Utils::FileSystem::isAudio(path))
	{
		if (getPlayAudio() && !mScreensaverMode && Settings::getInstance()->getBool("VideoAudio"))
		{
			// Make fake dimension to play audio files
			mVideoWidth = 1;
			mVideoHeight = 1;
		}
	}

	// Make sure we found a valid video track
	if ((mVideoWidth > 0) && (mVideoHeight > 0))
	{			
		if (mVideoWidth > 1 && Settings::getInstance()->getBool("OptimizeVideo"))
		{
			// Avoid videos bigger than resolution
			Vector2f maxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
								
#ifdef _RPI_
			// Temporary -> RPI -> Try to limit videos to 400x300 for performance benchmark
			if (!Renderer::isSmallScreen())
				maxSize = Vector2f(400, 300);
#endif

			if (!mTargetSize.empty() && (mTargetSize.x() < maxSize.x() || mTargetSize.y() < maxSize.y()))
				maxSize = mTargetSize;

			// If video is bigger than display, ask VLC for a smaller image
			auto sz = ImageIO::adjustPictureSize(Vector2i(mVideoWidth, mVideoHeight), Vector2i(maxSize.x(), maxSize.y()), mTargetIsMin);
			if (sz.x() < mVideoWidth || sz.y() < mVideoHeight)
			{
				mVideoWidth = sz.x();
				mVideoHeight = sz.y();
			}
		}

		PowerSaver::pause();
		setupContext();

		// The player is made on the worker (checkStart takes it): the UI thread does not wait for VLC
		auto start = std::make_shared<PlayerStart>();
		libvlc_media_retain(mMedia);
		start->media = mMedia;
		start->context = mContext;
		start->width = mVideoWidth;
		start->height = mVideoHeight;

		if (hasAudioTrack)
		{
			if (!getPlayAudio() || (!mScreensaverMode && !Settings::getInstance()->getBool("VideoAudio")) || (Settings::getInstance()->getBool("ScreenSaverVideoMute") && mScreensaverMode))
				start->mute = true;
			else
				AudioManager::setVideoPlaying(true);
		}

		mStart = start;
		startOnWorker(start);
	}
}

void VideoVlcComponent::checkStart()
{
	if (mStart == nullptr)
		return;

	std::unique_lock<std::mutex> lock(mStart->lock);
	if (!mStart->ready)
		return;

	mMediaPlayer = mStart->player;
	lock.unlock();
	mStart = nullptr;
}

void VideoVlcComponent::stopVideo()
{
	videoShown(this, false);

	mIsPlaying = false;
	mIsWaitingForVideoToStart = false;
	mStartDelayed = false;

	releasePlayback();
	PowerSaver::resume();
	AudioManager::setVideoPlaying(false);
}

// Lets go of what a start made: its parse, its player (being made or made), its media and its frames
void VideoVlcComponent::releasePlayback()
{
	// A parse under way is dropped with the media (on the worker: stopping it waits for VLC's reader)
	if (mParsing && mMedia)
	{
		stopParseOnWorker(mMedia);
		mMedia = NULL;
	}

	mParsing = false;

	// A player still being made is stopped by the worker when it is; one made is taken to stop it too
	if (mStart != nullptr)
	{
		std::unique_lock<std::mutex> lock(mStart->lock);
		if (mStart->ready)
			mMediaPlayer = mStart->player;
		else
			mStart->cancelled = true;

		lock.unlock();
		mStart = nullptr;
	}

	// The worker stops and releases the player (it waits for VLC's threads); the frames' buffers go with it
	if (mMediaPlayer)
	{
		stopOnWorker(mMediaPlayer, mContext);
		mMediaPlayer = NULL;
	}

	// Release the media
	if (mMedia)
	{
		releaseOnWorker(mMedia);
		mMedia = NULL;
	}

	freeContext();
}

void VideoVlcComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "video");
	if (!elem)
		return;

	if (elem && elem->has("effect"))
	{
		if (!(elem->get<std::string>("effect").compare("slideRight")))
			mEffect = VideoVlcFlags::VideoVlcEffect::SLIDERIGHT;
		else if (!(elem->get<std::string>("effect").compare("size")))
			mEffect = VideoVlcFlags::VideoVlcEffect::SIZE;
		else if (!(elem->get<std::string>("effect").compare("bump")))
			mEffect = VideoVlcFlags::VideoVlcEffect::BUMP;
		else
			mEffect = VideoVlcFlags::VideoVlcEffect::NONE;

		mConfig.scaleSnapshot = (mEffect != VideoVlcFlags::VideoVlcEffect::NONE);
	}

	if (elem && elem->has("roundCorners"))
		setRoundCorners(elem->get<float>("roundCorners"));
	
	if (properties & COLOR)
	{
		if (elem && elem->has("color"))
			setColorShift(elem->get<unsigned int>("color"));

		if (elem->has("linearSmooth"))
			mLinearSmooth = elem->get<bool>("linearSmooth");

		if (elem->has("saturation"))
			setSaturation(Math::clamp(elem->get<float>("saturation"), 0.0f, 1.0f));

		if (ThemeData::parseCustomShader(elem, &mCustomShader))
			updateRoundCorners();

		mStaticImage.setCustomShader(mCustomShader);
	}

	if (elem && elem->has("loops"))
		mLoops = (int)elem->get<float>("loops");
	else
		mLoops = -1;

	VideoComponent::applyTheme(theme, view, element, properties);
}

void VideoVlcComponent::update(int deltaTime)
{
	mElapsed += deltaTime;

	if (mConfig.showSnapshotNoVideo || mConfig.showSnapshotDelay)
		mStaticImage.update(deltaTime);

	checkDisplayed();
	VideoComponent::update(deltaTime);
}

void VideoVlcComponent::onShow()
{
	VideoComponent::onShow();
	mStaticImage.onShow();

	if (hasStoryBoard("", true) && mConfig.startDelay > 0)
		pauseStoryboard();
}

ThemeData::ThemeElement::Property VideoVlcComponent::getProperty(const std::string name)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	
	if (Utils::String::startsWith(name, "shader."))
	{
		auto prop = name.substr(7);

		auto it = mCustomShader.parameters.find(prop);
		if (it != mCustomShader.parameters.cend())
			return Utils::String::toFloat(it->second);

		return 0.0f;
	}

	if (name == "size" || name == "maxSize" || name == "minSize")
		return mSize / scale;

	if (name == "color")
		return mColorShift;

	if (name == "roundCorners")
		return mRoundCorners;

	if (name == "saturation")
		return mSaturation;

	return VideoComponent::getProperty(name);
}

void VideoVlcComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	
	if (value.type == ThemeData::ThemeElement::Property::PropertyType::Pair && (name == "maxSize" || name == "minSize"))
	{
		mSourceBounds.zw() = value.v;
		mTargetSize = Vector2f(value.v.x() * scale.x(), value.v.y() * scale.y());
		resize();
	}
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "color")
		setColorShift(value.i);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "roundCorners")
		setRoundCorners(value.f);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "saturation")
		setSaturation(value.f);
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && Utils::String::startsWith(name, "shader."))
	{
		auto prop = name.substr(7);

		auto it = mCustomShader.parameters.find(prop);
		if (it != mCustomShader.parameters.cend())
			mCustomShader.parameters[prop] = std::to_string(value.f);
	}
	else 
		VideoComponent::setProperty(name, value);
}

void VideoVlcComponent::pauseVideo()
{
	if (!mIsPlaying && !mIsWaitingForVideoToStart && !mStartDelayed)
		return;

	mIsPlaying = false;
	mIsWaitingForVideoToStart = false;
	mStartDelayed = false;
	videoShown(this, false);

	if (mMediaPlayer == NULL)
		stopVideo();
	else
	{
		libvlc_media_player_pause(mMediaPlayer);
		
		PowerSaver::resume();
		AudioManager::setVideoPlaying(false);
	}
}

void VideoVlcComponent::resumeVideo()
{
	if (mIsPlaying)
		return;

	if (mMediaPlayer == NULL)
	{
		startVideoWithDelay();
		return;
	}

	mIsPlaying = true;
	libvlc_media_player_play(mMediaPlayer);
	PowerSaver::pause();
	AudioManager::setVideoPlaying(true);
}

bool VideoVlcComponent::isPaused()
{
	return !mIsPlaying && !mIsWaitingForVideoToStart && !mStartDelayed && mMediaPlayer != NULL;
}

void VideoVlcComponent::setSaturation(float saturation)
{
	mSaturation = saturation;
}
