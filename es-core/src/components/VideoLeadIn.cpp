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
#include <chrono>
#include <sstream>
#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace
{
	const char* FFMPEG = "/usr/bin/ffmpeg";
	// the videos waiting for a measure: past this, the oldest of those asked after the others are dropped
	const size_t MAX_WAITING = 64;

	struct Known
	{
		std::string stamp;	// the file's size and date when measured
		int ms = 0;
		unsigned int width = 0, height = 0;	// the video stream's size (0: not read)
		bool audio = false;
	};

	struct State
	{
		std::mutex lock;
		std::condition_variable wake;
		std::map<std::string, Known> known;
		std::deque<std::string> waiting;
		std::set<std::string> waitingSet;
		bool loaded = false;
		bool started = false;
		bool paused = false;
		bool measuring = false;
		int available = -1;
	};

	// Never destroyed: the worker waits on it until the process ends (a condition variable destroyed at exit with
	// a thread waiting on it hangs the exit, and ES would not quit)
	State& state()
	{
		static State* s = new State();
		return *s;
	}

	std::mutex& sLock = state().lock;
	std::condition_variable& sWake = state().wake;
	std::map<std::string, Known>& sKnown = state().known;
	std::deque<std::string>& sWaiting = state().waiting;
	std::set<std::string>& sWaitingSet = state().waitingSet;
	bool& sLoaded = state().loaded;
	bool& sStarted = state().started;
	bool& sPaused = state().paused;
	int& sAvailable = state().available;

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
			std::istringstream fields(line.substr(0, a));
			if (!(fields >> known.ms) || known.ms < 0 || known.ms > VideoLight::VIDEO_LEAD_IN_MEASURED_MS)
				continue;
			fields >> std::ws;
			if (!fields.eof())
			{
				int audio;
				if (!(fields >> known.width >> known.height >> audio) || (audio != 0 && audio != 1) ||
					!((known.width == 0 && known.height == 0) || (known.width >= 16 && known.width <= 8192 &&
					known.height >= 16 && known.height <= 8192)))
					continue;
				fields >> std::ws;
				if (!fields.eof())
					continue;
				known.audio = audio == 1;
			}
			sKnown[line.substr(b + 1)] = known;
		}
	}

	void keep(const std::string& path, const Known& known)
	{
		std::ofstream file(cachePath(), std::ios::app);
		if (file)
			file << known.ms << ' ' << known.width << ' ' << known.height << ' ' << (known.audio ? 1 : 0) << '\t' << known.stamp << '\t' << path << '\n';
	}


	// Bound the lifetime of the local probe. Its anonymous log is removed on close.
	bool measure(const std::string& path, Known& known)
	{
#ifdef _WIN32
		return false;
#else
		struct stat info;
		if (path.empty() || path[0] != '/' || stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
			return false;
		FILE* output = tmpfile();
		if (!output)
			return false;
		std::string duration = std::to_string(VideoLight::VIDEO_LEAD_IN_MEASURED_MS / 1000.0f);
		const char* args[] = { FFMPEG, "-hide_banner", "-nostdin", "-v", "info", "-threads", "1",
			"-an", "-sn", "-dn", "-t", duration.c_str(), "-protocol_whitelist", "file",
			"-i", path.c_str(), "-vf", "scale=32:24,showinfo", "-f", "null", "-", nullptr };
		posix_spawn_file_actions_t actions;
		if (posix_spawn_file_actions_init(&actions) != 0)
		{
			fclose(output);
			return false;
		}
		pid_t child;
		int err = posix_spawn_file_actions_adddup2(&actions, fileno(output), STDERR_FILENO);
		if (!err)
			err = posix_spawn(&child, FFMPEG, &actions, nullptr, const_cast<char* const*>(args), environ);
		posix_spawn_file_actions_destroy(&actions);
		if (err)
		{
			fclose(output);
			return false;
		}
		setpriority(PRIO_PROCESS, child, 10);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		int status = 0;
		for (;;)
		{
			pid_t result = waitpid(child, &status, WNOHANG);
			if (result == child)
				break;
			if (result < 0 && errno != EINTR)
			{
				fclose(output);
				return false;
			}
			std::unique_lock<std::mutex> lock(sLock);
			if (sWake.wait_for(lock, std::chrono::milliseconds(20), [] { return sPaused; }) ||
				std::chrono::steady_clock::now() >= deadline)
			{
				lock.unlock();
				kill(child, SIGKILL);
				while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
				fclose(output);
				return false;
			}
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		{
			fclose(output);
			return false;
		}
		rewind(output);
		std::vector<std::pair<int, int>> frames;
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), output) != nullptr)
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

		fclose(output);
		known.ms = VideoLight::leadInMs(frames);
		if (frames.empty())
			known.width = known.height = 0;	// ffmpeg read no frame: the player parses the file itself
		return !frames.empty();
#endif
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
				if (sPaused || (it != sKnown.cend() && it->second.stamp == stamp))
					continue;
				state().measuring = true;
			}

			Known known;
			known.stamp = stamp;
			bool ok = measure(path, known);
			LOG(LogDebug) << "VideoLeadIn: " << known.ms << " ms, " << known.width << "x" << known.height << (known.audio ? " with sound: " : ": ") << path;

			std::unique_lock<std::mutex> lock(sLock);
			state().measuring = false;
			sWake.notify_all();
			if (!ok || sPaused)
				continue;
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

bool VideoLeadIn::pause(bool paused)
{
	std::unique_lock<std::mutex> lock(sLock);
	sPaused = paused;
	sWake.notify_all();
	return !paused || sWake.wait_for(lock, std::chrono::milliseconds(1000), [] { return !state().measuring; });
}
