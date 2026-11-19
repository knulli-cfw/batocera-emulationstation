#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
#define ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H

#include "VideoComponent.h"
#include "ThemeData.h"
#include "renderers/Renderer.h"
#include <atomic>
#include <mutex>

struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;

// The frames VLC draws for one playback. Shared by the component and the player's worker (VideoVlcComponent's
// PlayerWorker), so the buffers outlive a player that is still being stopped after the component let go.
struct VideoContext 
{
	VideoContext()
	{
		surfaces[0] = nullptr;
		surfaces[1] = nullptr;
		valid = false;
		displayed = false;
		hasFrame[0] = false;
		hasFrame[1] = false;
		surfaceId = 0;
	}

	~VideoContext()
	{
		delete[] surfaces[0];
		delete[] surfaces[1];
	}

	int					surfaceId;
	unsigned char*		surfaces[2];	
	std::mutex			mutexes[2];
	bool				hasFrame[2];

	bool				valid;
	// Set by VLC's display callback; the UI thread starts showing the video (VideoVlcComponent::checkDisplayed)
	std::atomic<bool>	displayed;
};

struct PlayerStart;


namespace VideoVlcFlags
{
	enum VideoVlcEffect
	{
		NONE,
		BUMP,
		SIZE,
		SLIDERIGHT
	};
}

class VideoVlcComponent : public VideoComponent
{
	// Structure that groups together the configuration of the video component
	struct Configuration
	{
		unsigned						startDelay;
		bool							showSnapshotNoVideo;
		bool							showSnapshotDelay;
		std::string						defaultVideoPath;
	};

public:
	static void init();

	VideoVlcComponent(Window* window);
	virtual ~VideoVlcComponent();

	void render(const Transform4x4f& parentTrans) override;

	// Resize the video to fit this size. If one axis is zero, scale that axis to maintain aspect ratio.
	// If both are non-zero, potentially break the aspect ratio.  If both are zero, no resizing.
	// Can be set before or after a video is loaded.
	// setMaxSize() and setResize() are mutually exclusive.
	void setResize(float width, float height);

	// Resize the video to be as large as possible but fit within a box of this size.
	// Can be set before or after a video is loaded.
	// Never breaks the aspect ratio. setMaxSize() and setResize() are mutually exclusive.
	void setMaxSize(float width, float height);
	void setMinSize(float width, float height);

	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties);
	virtual void update(int deltaTime);

	void	setColorShift(unsigned int color);

	virtual void onShow() override;

	ThemeData::ThemeElement::Property getProperty(const std::string name) override;
	void setProperty(const std::string name, const ThemeData::ThemeElement::Property& value) override;

	void setEffect(VideoVlcFlags::VideoVlcEffect effect) { mEffect = effect; }

	bool getLinearSmooth() { return mLinearSmooth; }
	void setLinearSmooth(bool value = true) { mLinearSmooth = value; }

	void setSaturation(float saturation);

	void setRoundCorners(float value) override;
	void onSizeChanged() override;
	void onPaddingChanged() override;
	
	Vector2f getSize() const override;

	// What VLC read of a video's file: its first video track's size and whether it has sound
	struct Probe
	{
		bool         readable;
		unsigned int width, height;
		bool         audio;
	};

private:
	// Calculates the correct mSize from our resizing information (set by setResize/setMaxSize).
	// Used internally whenever the resizing parameters or texture change.
	void resize();
	// Start the video Immediately
	virtual void startVideo();
	// Stop the video
	virtual void stopVideo();

	virtual void pauseVideo();
	virtual void resumeVideo();
	virtual bool isPaused();

	// Handle looping the video. Must be called periodically
	virtual void handleLooping();

	virtual void onVideoStarted();

	void setupContext();
	void freeContext();

	// startVideo asks VLC to read the file's tracks on its own thread; each frame checkParse looks whether it
	// has, then playback starts (startPlayback). Until then the snapshot or what is under the video shows and
	// the UI thread does not wait. A file read once is not parsed again while its size and date stay the same.
	void checkParse();
	void startPlayback(const Probe& probe);
	void checkStart();
	void checkDisplayed();
	static bool findProbe(const std::string& path, Probe& probe);
	static void keepProbe(const std::string& path, const Probe& probe);

private:
	void crop(float left, float top, float right, float bot);

	static libvlc_instance_t*		mVLC;
	libvlc_media_t*					mMedia;
	libvlc_media_player_t*			mMediaPlayer;
	bool							mParsing;
	unsigned int					mParseEnd;	// SDL ticks
	std::shared_ptr<PlayerStart>	mStart;		// a player being made on the worker
	std::shared_ptr<VideoContext>	mContext;
	std::shared_ptr<TextureResource> mTexture;

	std::string					    mSubtitlePath;
	std::string					    mSubtitleTmpFile;
	Renderer::ShaderInfo			mCustomShader;

	VideoVlcFlags::VideoVlcEffect	mEffect;

	unsigned int					mColorShift;
	int								mElapsed;

	int								mCurrentLoop;
	int								mLoops;

	bool							mLinearSmooth;
	float							mSaturation;

	void updateVertices();
	void updateColors();
	void updateRoundCorners();
	
	Renderer::Vertex				mVertices[4];
	std::vector<Renderer::Vertex>	mRoundCornerStencil;

	Vector2f mTopLeftCrop;
	Vector2f mBottomRightCrop;
};

#endif // ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
