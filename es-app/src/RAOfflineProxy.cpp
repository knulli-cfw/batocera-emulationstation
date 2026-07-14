#include "RAOfflineProxy.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "SystemConf.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <sys/stat.h>
#include <sys/wait.h>

#define RAOP_LAUNCHER "/userdata/system/raofflineproxy/bin/raofflineproxy"
#define RAOP_CACHED_IDS_FILE "/userdata/system/.config/raofflineproxy/cached_game_ids.txt"

namespace RAOfflineProxy
{
	static std::mutex sMutex;
	static std::set<int> sCachedIds;
	static long long sLoadedStamp = -1;
	static bool sLoadedOnce = false;

	static long long fileStamp(const char* path)
	{
		struct stat st;
		if (stat(path, &st) != 0)
			return 0;

		return (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec + st.st_size;
	}

	bool isInstalled()
	{
		return Utils::FileSystem::exists(RAOP_LAUNCHER);
	}

	static const char* CONF_FILES[] = { "/userdata/system/batocera.conf", "/userdata/system/knulli.conf" };

	static bool readServiceEnabled()
	{
		for (const char* confPath : CONF_FILES)
		{
			std::ifstream file(confPath);
			std::string line;
			while (std::getline(file, line))
			{
				if (line.rfind("system.services=", 0) != 0)
					continue;

				for (auto& service : Utils::String::split(line.substr(16), ' '))
					if (service == "raofflineproxy")
						return true;
			}
		}

		return false;
	}

	bool isServiceEnabled()
	{
		static std::mutex confMutex;
		static bool cachedEnabled = false;
		static long long cachedStamps[2] = { -1, -1 };

		std::unique_lock<std::mutex> lock(confMutex);

		long long stamps[2] = { fileStamp(CONF_FILES[0]), fileStamp(CONF_FILES[1]) };

		if (stamps[0] != cachedStamps[0] || stamps[1] != cachedStamps[1])
		{
			cachedEnabled = readServiceEnabled();
			cachedStamps[0] = stamps[0];
			cachedStamps[1] = stamps[1];
		}

		return cachedEnabled;
	}

	bool isProxyToggleEnabled()
	{
		return SystemConf::getInstance()->getBool("global.retroachievements.proxy", true);
	}

	bool isServiceActive()
	{
		return isInstalled() && isServiceEnabled();
	}

	bool isActive()
	{
		return isServiceActive() && isProxyToggleEnabled();
	}

	std::string launcherPath()
	{
		return RAOP_LAUNCHER;
	}

	static void reloadIfChanged()
	{
		long long stamp = fileStamp(RAOP_CACHED_IDS_FILE);
		if (sLoadedOnce && stamp == sLoadedStamp)
			return;

		sCachedIds.clear();

		std::ifstream file(RAOP_CACHED_IDS_FILE);
		std::string line;
		while (std::getline(file, line))
		{
			int id = Utils::String::toInteger(line);
			if (id > 0)
				sCachedIds.insert(id);
		}

		sLoadedStamp = stamp;
		sLoadedOnce = true;
	}

	bool isGameCached(int cheevosId)
	{
		if (cheevosId <= 0 || !isActive())
			return false;

		std::unique_lock<std::mutex> lock(sMutex);
		reloadIfChanged();
		return sCachedIds.find(cheevosId) != sCachedIds.cend();
	}

	void invalidateCachedIds()
	{
		std::unique_lock<std::mutex> lock(sMutex);
		sLoadedOnce = false;
	}

	std::string quoteShellArgument(const std::string& value)
	{
		std::string quoted = "'";
		for (char c : value)
		{
			if (c == '\'')
				quoted += "'\\''";
			else
				quoted += c;
		}
		quoted += "'";
		return quoted;
	}

	int runCommand(const std::string& command, const std::function<void(const std::string&)>& onLine)
	{
		FILE* pipe = popen(command.c_str(), "r");
		if (pipe == nullptr)
			return -1;

		char line[1024];
		while (fgets(line, sizeof(line), pipe))
		{
			strtok(line, "\n");
			if (onLine != nullptr)
				onLine(std::string(line));
		}

		int status = pclose(pipe);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);

		return -1;
	}
}
