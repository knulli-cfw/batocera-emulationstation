#pragma once
#include <string>
#include <vector>
#include "Window.h"

struct Device
{
	std::string id;
	std::string name;
	bool paused;
	int completion;
	int needItems;
	int globalItems;
	int needBytes;
	int transferSpeed;
};

struct Folder 
{
	std::string id;
	std::string label;
	std::string path;
	bool fsWatcherEnabled;
};

struct SyncthingState
{
	int itemsSynced;
	int itemsTotal;

	int isSyncing() {
		return itemsSynced < itemsTotal;
	}

	int getPercentDone() {
		if (itemsTotal == 0) return 100;
		return (itemsSynced * 100) / itemsTotal;
	}
};

class SyncthingUtil
{
public:
		static SyncthingUtil& getInstance()
		{
			static SyncthingUtil instance;
			if (!instance.isConnected())
			{
				instance.connect();
			}
			return instance;
		}

		void scan(Window* window, std::string const* folderId = nullptr);
		SyncthingState getState();
		static bool isEnabled();
		bool isConnected() { return mConnected; }
		bool connect();
		void disconnect();
		bool reconnect();

private:
		SyncthingUtil() {} // Purposely hidden, use getInstance() instead!
		SyncthingUtil(const SyncthingUtil&);  // Purposely hidden, don't implement!
		void operator=(const SyncthingUtil&); // Purposely hidden, don't implement!

		bool mConnected = false;

		// Syncthing configuration
		std::string mApiKey;
		std::vector<Device> mDevices;
		std::vector<Folder> mFolders;

		Device self {
				.id = "self",
				.name = "self",
				.paused = false,
				.completion = 0,
				.needItems = 0,
				.globalItems = 0,
				.needBytes = 0,
				.transferSpeed = 0
		};

		std::string getMyId();
		std::vector<std::string> getConnectedDeviceIds();
		void updateDevice(Device* device);
		Device *getDeviceById(const std::string& deviceId);
		Folder *getFolderById(const std::string& folderId);
};