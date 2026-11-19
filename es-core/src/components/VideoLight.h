#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_LIGHT_H
#define ES_CORE_COMPONENTS_VIDEO_LIGHT_H

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
}

#endif // ES_CORE_COMPONENTS_VIDEO_LIGHT_H
