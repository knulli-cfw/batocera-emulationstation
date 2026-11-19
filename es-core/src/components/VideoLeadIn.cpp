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

	// Lines "ms <tab> size date <tab> path"; a later line for a path wins. Under sLock.
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

			sKnown[line.substr(b + 1)] = Known{ line.substr(a + 1, b - a - 1), atoi(line.substr(0, a).c_str()) };
		}
	}

	void keep(const std::string& path, const std::string& stamp, int ms)
	{
		std::ofstream file(cachePath(), std::ios::app);
		if (file)
			file << ms << '\t' << stamp << '\t' << path << '\n';
	}

	std::string quoted(const std::string& s)
	{
		return "'" + Utils::String::replace(s, "'", "'\\''") + "'";
	}

	// The lead-in of a file from ffmpeg's showinfo of its first frames (each frame's time and luma), at low
	// priority; 0 when ffmpeg reads no frame
	int measure(const std::string& path)
	{
		std::string command = std::string("nice -n 10 ") + FFMPEG + " -hide_banner -nostdin -v info -threads 1 -an -sn -dn -t " +
			std::to_string(VideoLight::VIDEO_LEAD_IN_MEASURED_MS / 1000.0f) + " -i " + quoted(path) + " -vf scale=32:24,showinfo -f null - 2>&1";

		FILE* pipe = popen(command.c_str(), "r");
		if (pipe == nullptr)
			return 0;

		std::vector<std::pair<int, int>> frames;
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			const char* time = strstr(buffer, "pts_time:");
			const char* mean = strstr(buffer, "mean:[");
			if (time == nullptr || mean == nullptr)
				continue;

			int ms = (int)(atof(time + 9) * 1000.0 + 0.5);
			frames.push_back(std::make_pair(ms, VideoLight::videoRangeLight((float)atof(mean + 6))));
		}

		pclose(pipe);
		return VideoLight::leadInMs(frames);
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

			int ms = measure(path);
			LOG(LogDebug) << "VideoLeadIn: " << ms << " ms: " << path;

			std::unique_lock<std::mutex> lock(sLock);
			sKnown[path] = Known{ stamp, ms };
			keep(path, stamp, ms);
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
