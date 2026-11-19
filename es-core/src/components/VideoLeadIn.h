#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_LEAD_IN_H
#define ES_CORE_COMPONENTS_VIDEO_LEAD_IN_H

#include <string>

// A video's fade from black (its lead-in), measured once off the UI thread and kept by path, size and date (in
// memory and in videoleadin.cache): a video that comes in over its snapshot starts after it, so its first frame
// is lit (VideoLight). Measured with ffmpeg's first 1.5 s of the file, which also gives its streams (the video's
// size, any sound); without ffmpeg nothing is known.
namespace VideoLeadIn
{
	// The lead-in in ms (0: none), or -1 when not measured yet for this file as it is now
	int find(const std::string& path);

	// What the measure read of the file's streams: its video's size and whether it has sound (false when not
	// measured, or measured before the streams were kept). The player then needs no parse of its own.
	bool findStreams(const std::string& path, unsigned int& width, unsigned int& height, bool& audio);

	// Measures the file if not known: first (the video about to play) or after those already asked (the cards
	// shown and the next ones)
	void request(const std::string& path, bool first);

	// No measure starts while paused (a game runs)
	void pause(bool paused);
}

#endif // ES_CORE_COMPONENTS_VIDEO_LEAD_IN_H
