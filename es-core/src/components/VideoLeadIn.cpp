#include "components/VideoLeadIn.h"
#include "components/VideoLight.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Paths.h"

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace
{
	const char* FFMPEG = "/usr/bin/ffmpeg";
	// the videos waiting for a measure: past this, the oldest of those asked after the others are dropped
	const size_t MAX_WAITING = 64;

	struct Known
	{
		std::string stamp;	// the file's size and date when measured
		int ms;
		unsigned int width = 0, height = 0;	// the video stream's size (0: not read)
		bool audio = false;
	};

	std::mutex sLock;
	std::condition_variable sWake;
	std::map<std::string, Known> sKnown;
	std::deque<std::string> sWaiting;
	std::set<std::string> sWaitingSet;
	bool sLoaded = false;
	bool sStarted = false;
	bool sPaused = false;
	int sAvailable = -1;

	std::string cachePath()
	{
		return Paths::getUserEmulationStationPath() + "/videoleadin.cache";
	}

	// The file's size and date, so a replaced file is measured again
	std::string stampOf(const std::string& path)
	{
		return std::to_string(Utils::FileSystem::getFileSize(path)) + " " + Utils::FileSystem::getFileModificationDate(path).getIsoString();
	}

	// Lines "ms [width height audio] <tab> size date <tab> path"; a later line for a path wins. Under sLock.
	void load()
	{
		if (sLoaded)
			return;

		sLoaded = true;
		std::ifstream file(cachePath());
		std::string line;
		while (std::getline(file, line))
		{
			size_t a = line.find('\t');
			size_t b = a == std::string::npos ? a : line.find('\t', a + 1);
			if (b == std::string::npos || b + 1 >= line.size())
				continue;

			Known known;
			known.stamp = line.substr(a + 1, b - a - 1);
			int audio = 0;
			if (sscanf(line.substr(0, a).c_str(), "%d %u %u %d", &known.ms, &known.width, &known.height, &audio) < 4)
				known.width = known.height = 0;

			known.audio = audio != 0;
			sKnown[line.substr(b + 1)] = known;
		}
	}

	void keep(const std::string& path, const Known& known)
	{
		std::ofstream file(cachePath(), std::ios::app);
		if (file)
			file << known.ms << ' ' << known.width << ' ' << known.height << ' ' << (known.audio ? 1 : 0) << '\t' << known.stamp << '\t' << path << '\n';
	}


	std::string quoted(const std::string& s)
	{
		return "'" + Utils::String::replace(s, "'", "'\\''") + "'";
	}

	// The lead-in of a file from ffmpeg's showinfo of its first frames (each frame's time and luma), at low
	// priority, with its first video stream's size and whether it has sound; 0 when ffmpeg reads no frame
	void measure(const std::string& path, Known& known)
	{
		std::string command = std::string("nice -n 10 ") + FFMPEG + " -hide_banner -nostdin -v info -threads 1 -an -sn -dn -t " +
			std::to_string(VideoLight::VIDEO_LEAD_IN_MEASURED_MS / 1000.0f) + " -i " + quoted(path) + " -vf scale=32:24,showinfo -f null - 2>&1";

		known.ms = 0;
		FILE* pipe = popen(command.c_str(), "r");
		if (pipe == nullptr)
			return;

		std::vector<std::pair<int, int>> frames;
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			// the file's streams, listed before the frames ("Stream #0:0...: Video: h264 ..., 320x240 [SAR...")
			const char* stream = strstr(buffer, "Stream #0:");
			if (stream != nullptr && strstr(stream, ": Audio:") != nullptr)
				known.audio = true;
			if (stream != nullptr && known.width == 0 && strstr(stream, ": Video:") != nullptr)
				VideoLight::streamSize(strstr(stream, ": Video:"), known.width, known.height);

			const char* time = strstr(buffer, "pts_time:");
			const char* mean = strstr(buffer, "mean:[");
			if (time == nullptr || mean == nullptr)
				continue;

			int ms = (int)(atof(time + 9) * 1000.0 + 0.5);
			frames.push_back(std::make_pair(ms, VideoLight::videoRangeLight((float)atof(mean + 6))));
		}

		pclose(pipe);
		known.ms = VideoLight::leadInMs(frames);
		if (frames.empty())
			known.width = known.height = 0;	// ffmpeg read no frame: the player parses the file itself
	}

	void run()
	{
		for (;;)
		{
			std::string path;
			{
				std::unique_lock<std::mutex> lock(sLock);
				sWake.wait(lock, [] { return !sPaused && !sWaiting.empty(); });
				path = sWaiting.front();
				sWaiting.pop_front();
				sWaitingSet.erase(path);
			}

			std::string stamp = stampOf(path);
			{
				std::unique_lock<std::mutex> lock(sLock);
				auto it = sKnown.find(path);
				if (it != sKnown.cend() && it->second.stamp == stamp)
					continue;
			}

			Known known;
			known.stamp = stamp;
			measure(path, known);
			LOG(LogDebug) << "VideoLeadIn: " << known.ms << " ms, " << known.width << "x" << known.height << (known.audio ? " with sound: " : ": ") << path;

			std::unique_lock<std::mutex> lock(sLock);
			sKnown[path] = known;
			keep(path, known);
		}
	}
}

int VideoLeadIn::find(const std::string& path)
{
	if (path.empty())
		return -1;

	std::string stamp = stampOf(path);

	std::unique_lock<std::mutex> lock(sLock);
	load();

	auto it = sKnown.find(path);
	return it != sKnown.cend() && it->second.stamp == stamp ? it->second.ms : -1;
}

bool VideoLeadIn::findStreams(const std::string& path, unsigned int& width, unsigned int& height, bool& audio)
{
	if (path.empty())
		return false;

	std::string stamp = stampOf(path);

	std::unique_lock<std::mutex> lock(sLock);
	load();

	auto it = sKnown.find(path);
	if (it == sKnown.cend() || it->second.stamp != stamp || it->second.width == 0)
		return false;

	width = it->second.width;
	height = it->second.height;
	audio = it->second.audio;
	return true;
}

void VideoLeadIn::request(const std::string& path, bool first)
{
	if (path.empty())
		return;

	std::unique_lock<std::mutex> lock(sLock);
	if (sAvailable < 0)
		sAvailable = Utils::FileSystem::exists(FFMPEG) ? 1 : 0;

	if (sAvailable == 0)
		return;

	load();

	// known (the worker looks at the file's stamp before it measures): nothing to do
	if (sKnown.find(path) != sKnown.cend() && !first)
		return;

	if (sWaitingSet.find(path) != sWaitingSet.cend())
	{
		if (!first)
			return;

		for (auto it = sWaiting.begin(); it != sWaiting.end(); ++it)
			if (*it == path)
			{
				sWaiting.erase(it);
				break;
			}
	}
	else
		sWaitingSet.insert(path);

	if (first)
		sWaiting.push_front(path);
	else
		sWaiting.push_back(path);

	while (sWaiting.size() > MAX_WAITING)
	{
		sWaitingSet.erase(sWaiting.back());
		sWaiting.pop_back();
	}

	if (!sStarted)
	{
		sStarted = true;
		std::thread(run).detach();
	}

	sWake.notify_one();
}

void VideoLeadIn::pause(bool paused)
{
	std::unique_lock<std::mutex> lock(sLock);
	sPaused = paused;
	sWake.notify_one();
}
