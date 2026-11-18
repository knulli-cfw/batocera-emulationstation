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
}

#endif // ES_CORE_COMPONENTS_SLIDE_LAYOUT_H
