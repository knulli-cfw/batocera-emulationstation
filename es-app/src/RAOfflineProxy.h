#pragma once
#ifndef ES_APP_RAOFFLINEPROXY_H
#define ES_APP_RAOFFLINEPROXY_H

#include <functional>
#include <string>

// Integration with the RAOfflineProxy RetroAchievements offline proxy.
// The proxy exports the set of RA game ids it has cached to a plain text
// file (one id per line); ES reads it to flag games whose achievements
// are available offline.
namespace RAOfflineProxy
{
	bool isInstalled();
	bool isServiceEnabled();
	bool isProxyToggleEnabled();
	bool isServiceActive();
	bool isActive();
	std::string launcherPath();
	bool isGameCached(int cheevosId);
	void invalidateCachedIds();
	std::string quoteShellArgument(const std::string& value);
	int runCommand(const std::string& command, const std::function<void(const std::string&)>& onLine);
}

#endif // ES_APP_RAOFFLINEPROXY_H
