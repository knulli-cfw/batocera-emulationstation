#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_LIGHT_H
#define ES_CORE_COMPONENTS_VIDEO_LIGHT_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>
#include <vector>

// When a video that replaces its snapshot may show. Most videos open on a fade from black (picture and sound), and a
// crossfade into that dips the picture to black: the snapshot stays until the video's frames are lit and the fade is
// over (a frame no brighter than the one before), or until VIDEO_LIGHT_WAIT_MS after the first frame.
namespace VideoLight
{
	// A frame this dark (0-255) is still the fade from black
	static const int VIDEO_LIT = 10;
	// The longest the snapshot waits for the video's light, from its first frame
	static const unsigned int VIDEO_LIGHT_WAIT_MS = 3000;

	// The average light (luma, 0-255) of an RGBA frame of width x height (rows of width * 4 bytes), from a grid of
	// 16 x 16 points
	inline int frameLight(const unsigned char* pixels, int width, int height)
	{
		if (pixels == nullptr || width <= 0 || height <= 0)
			return 0;

		long sum = 0;
		int count = 0;
		for (int j = 0; j < 16; j++)
		{
			int y = (2 * j + 1) * height / 32;
			for (int i = 0; i < 16; i++)
			{
				int x = (2 * i + 1) * width / 32;
				const unsigned char* p = pixels + ((long)y * width + x) * 4;
				sum += (p[0] * 54 + p[1] * 183 + p[2] * 19) >> 8;
				count++;
			}
		}

		return (int)(sum / count);
	}

	// Whether the video may show, given its newest frame's light, the one before (-1: none yet) and the time since
	// its first frame. A first frame lit already may be in the middle of the fade: the next one tells.
	inline bool mayShow(int light, int lastLight, unsigned int sinceFirstMs)
	{
		if (sinceFirstMs >= VIDEO_LIGHT_WAIT_MS)
			return true;

		if (light < VIDEO_LIT || lastLight < 0)
			return false;

		// still coming in from black: brighter than the frame before
		return light <= lastLight + 1;
	}

	// How much of a video's first frames is measured for its fade from black (VideoLeadIn)
	static const int VIDEO_LEAD_IN_MEASURED_MS = 1500;

	// The light (0-255) of a frame whose luma is in video range (black 16, white 235)
	inline int videoRangeLight(float y)
	{
		int light = (int)((y - 16.0f) * 255.0f / 219.0f + 0.5f);
		return std::max(0, std::min(255, light));
	}

	// "..., 320x240 [SAR 1:1 DAR 4:3], ..." in ffmpeg's line for a video stream: its size
	inline bool streamSize(const char* line, unsigned int& width, unsigned int& height)
	{
		for (const char* p = line; *p; p++)
		{
			if ((*p != ' ' && *p != ',') || !isdigit((unsigned char)p[1]))
				continue;

			unsigned int w = 0, h = 0;
			int used = 0;
			if (sscanf(p + 1, "%ux%u%n", &w, &h, &used) == 2 && w >= 16 && h >= 16 && w <= 8192 && h <= 8192)
			{
				char after = p[1 + used];
				if (after == ' ' || after == ',' || after == '\0' || after == '\n')
				{
					width = w;
					height = h;
					return true;
				}
			}
		}

		return false;
	}

	// Where a video's picture is in after its fade from black, from its first frames (time in ms, light): the
	// first frame lit to 80 % of the light after the fade (the median of the frames from 1 s on, else of the last
	// third), and at least VIDEO_LIT. 0 when the video starts lit, stays dark, or darkens on the way (not a fade:
	// then up to its first lit frame only).
	inline int leadInMs(const std::vector<std::pair<int, int>>& frames)
	{
		if (frames.empty())
			return 0;

		std::vector<int> after;
		for (auto& frame : frames)
			if (frame.first >= 1000)
				after.push_back(frame.second);

		if (after.size() < 3)
		{
			after.clear();
			for (size_t i = frames.size() - frames.size() / 3 - 1; i < frames.size(); i++)
				after.push_back(frames[i].second);
		}

		std::sort(after.begin(), after.end());
		int reference = after[after.size() / 2];
		if (reference < VIDEO_LIT)
			return 0;

		int target = std::max(VIDEO_LIT, reference * 8 / 10);
		int brightest = 0, firstLit = -1;
		for (size_t i = 0; i < frames.size(); i++)
		{
			int light = frames[i].second;
			if (light >= target)
				return i == 0 ? 0 : frames[i].first;

			if (firstLit < 0 && light >= VIDEO_LIT)
				firstLit = (int)i;

			// darker than before by more than noise: a scene, not a fade
			if (light + 8 < brightest)
				return firstLit <= 0 ? 0 : frames[firstLit].first;

			brightest = std::max(brightest, light);
		}

		return 0;
	}
}

#endif // ES_CORE_COMPONENTS_VIDEO_LIGHT_H
