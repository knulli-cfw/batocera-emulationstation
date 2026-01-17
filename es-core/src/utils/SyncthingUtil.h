#pragma once
#include <string>
#include <vector>
#include "Window.h"

struct Device {
	std::string id;
	std::string name;
	bool paused;
	bool connected;
	int64_t completion;
    int64_t needItems;
    int64_t globalItems;
    int64_t needBytes;
    int64_t bytesReceived;
    int64_t bytesSent;
    int64_t transferSpeed;
};

struct Folder {
	std::string id;
	std::string label;
	std::string path;
	bool fsWatcherEnabled;
};

struct SyncthingState {
	int64_t itemsSynced;
    int64_t itemsTotal;
    int64_t transferSpeed;
    int64_t totalBytesTransferred;
	std::vector<std::string> connectedDevices;
	std::vector<std::string> dirtyDevices; // IDs of devices with unsynced changes

	int isSyncing() {
		return transferSpeed > 0 && itemsSynced < itemsTotal;
	}

	int getPercentDone() {
		if (itemsTotal <= 0) return 100;
        if (itemsSynced >= itemsTotal) return 100;
        if (itemsSynced <= 0) return 0;
        return (int)((itemsSynced * 100) / itemsTotal);
	}
};

class SyncthingUtil {
public:
	static SyncthingUtil& getInstance() {
		static SyncthingUtil instance;
		if (!instance.isConnected()) {
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
	SyncthingUtil() = default; // Private constructor for singleton pattern

	// Disable copying and assignment
	SyncthingUtil(const SyncthingUtil&) = delete;
	SyncthingUtil& operator=(const SyncthingUtil&) = delete;

	// Optional: Disable moving as well
	SyncthingUtil(SyncthingUtil&&) = delete;
	SyncthingUtil& operator=(SyncthingUtil&&) = delete;

	bool mConnected = false;
	int mCurrentTransferNeededFiles = 0;

	// Syncthing configuration
	std::string mApiKey;
	std::vector<Device> mDevices;
	std::vector<Folder> mFolders;

	Device self {
		.id = "self",
		.name = "self",
		.paused = false,
		.connected = false,
		.completion = 0,
		.needItems = 0,
		.globalItems = 0,
		.needBytes = 0,
		.bytesReceived = 0,
		.bytesSent = 0,
		.transferSpeed = 0
	};

	std::string getMyId();
	std::vector<std::string> getConnectedDeviceIds();
	void updateDeviceCompletion(Device* device);
	Device *getDeviceById(const std::string& deviceId);
	Folder *getFolderById(const std::string& folderId);
};