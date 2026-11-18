#pragma once
#ifndef ES_CORE_COMPONENTS_SLIDE_LAYOUT_H
#define ES_CORE_COMPONENTS_SLIDE_LAYOUT_H

#include <algorithm>
#include <cmath>

// Where SlideshowComponent draws a capture and which dots it shows. Free of ES types so it can be
// tested alone.
namespace SlideLayout
{
	struct Fit
	{
		float scale;
		float x, y;		// the image's top-left corner in the box
		float w, h;		// the image's drawn size
	};

	// The capture at the smallest of 1/4, 1/2, 1, 2, 3 and 4 at which its region of interest (fractions of
	// the image) covers the box, else at the exact factor that does. The region is centred, the image
	// covers the box, and its corner is on whole pixels; at 2x and up it is on the grid of the capture's
	// pixels from the box's corner, so an LCD grid drawn on that grid lines up with them.
	inline Fit fit(float imageW, float imageH, float roiX, float roiY, float roiW, float roiH, float boxW, float boxH)
	{
		Fit out = { 1.0f, 0.0f, 0.0f, imageW, imageH };
		if (imageW <= 0 || imageH <= 0 || boxW <= 0 || boxH <= 0 || roiW <= 0 || roiH <= 0)
			return out;

		float rw = imageW * roiW;
		float rh = imageH * roiH;
		float need = std::max(boxW / rw, boxH / rh);

		static const float scales[] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };
		float scale = need;
		for (float s : scales)
		{
			if (s >= need - 0.0001f)
			{
				scale = s;
				break;
			}
		}

		out.scale = scale;
		out.w = imageW * scale;
		out.h = imageH * scale;

		float x = boxW / 2.0f - (imageW * roiX + rw / 2.0f) * scale;
		float y = boxH / 2.0f - (imageH * roiY + rh / 2.0f) * scale;

		// Cover the box: no edge of the image inside it
		x = std::min(0.0f, std::max(boxW - out.w, x));
		y = std::min(0.0f, std::max(boxH - out.h, y));

		float step = (scale >= 2.0f && scale == std::floor(scale)) ? scale : 1.0f;
		auto snap = [step](float v, float lowest)
		{
			float s = std::round(v / step) * step;
			if (s > 0.0f)
				s -= step;
			if (s < lowest)
				s += step;
			return (s > 0.0f || s < lowest) ? std::round(v) : s;
		};

		out.x = snap(x, boxW - out.w);
		out.y = snap(y, boxH - out.h);
		return out;
	}

	struct Dots
	{
		int first;			// the first dot's slide
		int count;			// how many dots
		bool moreBefore;	// slides before the first dot (it is drawn smaller)
		bool moreAfter;		// slides after the last one
	};

	// At most maxDots, in a window that keeps the current slide in the middle where it can
	inline Dots dots(int total, int current, int maxDots)
	{
		Dots out = { 0, std::max(0, total), false, false };
		if (maxDots <= 0 || total <= maxDots)
			return out;

		int first = current - maxDots / 2;
		first = std::max(0, std::min(total - maxDots, first));

		out.first = first;
		out.count = maxDots;
		out.moreBefore = first > 0;
		out.moreAfter = first + maxDots < total;
		return out;
	}

	// The slide's ease (in and out)
	inline float ease(float t)
	{
		t = std::max(0.0f, std::min(1.0f, t));
		return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
	}

	// When a slideshow moves on, and what to do about a next capture that does not load (a file that cannot
	// be decoded or is gone, a texture evicted or dropped from the load queue). The next capture is asked for
	// while the current one shows; RETRY_AFTER ms past the interval it is asked for once more, and SKIP_AFTER
	// ms after that the following capture is tried instead, so the slideshow never waits for one forever.
	class Clock
	{
	public:
		enum Request { NONE, LOAD, RELOAD };	// what the caller does with the next capture's texture

		static const int RETRY_AFTER = 1000;
		static const int SKIP_AFTER = 2000;

		Clock() : mInterval(4000), mDuration(600) { rewind(); }

		void setTiming(int interval, int duration) { mInterval = std::max(1, interval); mDuration = std::max(1, duration); }

		// The first capture, waiting a whole interval
		void rewind() { mCurrent = 0; mNext = -1; mElapsed = 0; mWait = 0; mRetried = false; mProgress = -1.0f; }

		bool atRest() const { return mCurrent == 0 && mNext < 0 && mProgress < 0 && mElapsed == 0; }

		int current() const { return mCurrent; }
		int next() const { return mNext; }
		float progress() const { return mProgress; }	// of the slide to the next capture, 0 to 1; -1 while waiting

		// One frame of deltaTime ms over count captures. Only a selected item moves; a deselected one goes back
		// to the first capture; a paused one (screen saver, view hidden) keeps its place without moving.
		Request update(int deltaTime, int count, bool selected, bool paused, bool nextLoaded)
		{
			if (count < 2 || !selected)
			{
				if (!atRest())
					rewind();

				return NONE;
			}

			if (paused)
				return NONE;

			if (mCurrent >= count || mNext >= count)
				rewind();

			if (mProgress >= 0)
			{
				mProgress += (float)deltaTime / (float)mDuration;
				if (mProgress >= 1.0f)
				{
					mCurrent = mNext;
					mNext = -1;
					mProgress = -1.0f;
					mElapsed = 0;
				}

				return NONE;
			}

			if (mNext < 0)
			{
				mNext = (mCurrent + 1) % count;
				mWait = 0;
				mRetried = false;
				return LOAD;
			}

			mElapsed += deltaTime;
			if (mElapsed < mInterval)
				return NONE;

			if (nextLoaded)
			{
				mProgress = 0.0f;
				return NONE;
			}

			mWait += deltaTime;
			if (!mRetried)
			{
				if (mWait < RETRY_AFTER)
					return NONE;

				mRetried = true;
				return RELOAD;
			}

			if (mWait < RETRY_AFTER + SKIP_AFTER)
				return NONE;

			// Skip it; when every other capture fails, wait a whole interval and try again
			int skip = (mNext + 1) % count;
			if (skip == mCurrent)
			{
				mElapsed = 0;
				mWait = 0;
				mRetried = false;
				return RELOAD;
			}

			mNext = skip;
			mWait = 0;
			mRetried = false;
			return LOAD;
		}

	private:
		int mInterval, mDuration;
		int mCurrent, mNext, mElapsed, mWait;
		bool mRetried;
		float mProgress;
	};
}

#endif // ES_CORE_COMPONENTS_SLIDE_LAYOUT_H
