#pragma once
#include <string>
#include <vector>
#include "HttpReq.h"
#include "Window.h"
#include "watchers/WatchersManager.h"
#include <map>
#include <memory>
#include <mutex>
#include <atomic>

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
	bool paused;
	bool shared;
};

struct SyncthingState {
	int64_t itemsSynced;
    int64_t itemsTotal;
    int64_t transferSpeed;
    int64_t totalBytesTransferred;
	std::vector<std::string> connectedDevices;
	std::vector<std::string> dirtyDevices; // IDs of devices with unsynced changes

	bool isSyncing() {
		return transferSpeed > 0 && itemsSynced < itemsTotal;
	}

	int getPercentDone() {
		if (itemsTotal <= 0) return 100;
        if (itemsSynced >= itemsTotal) return 100;
        if (itemsSynced <= 0) return 0;
        return (int)((itemsSynced * 100) / itemsTotal);
	}
};

class SyncthingUtil : public IWatcherNotify {
public:
	static SyncthingUtil& getInstance() {
		static SyncthingUtil instance;
		std::call_once(mOnceFlag, []() {
			instance.init();
			if (!instance.isConnected()) {
				instance.connect();
			}
		});
		return instance;
	}

	void init();
	bool reloadConfig();
	void scan(Window* window, std::string const* folderId = nullptr);
	SyncthingState getState();
	static bool isEnabled();
	bool isConnected() { return mConnected && mWifiConnected; }
	bool connect();
	void disconnect();
	bool reconnect();
	void OnWatcherChanged(IWatcher* component) override;

private:
	SyncthingUtil() = default; // Private constructor for singleton pattern

	// Disable copying and assignment
	SyncthingUtil(const SyncthingUtil&) = delete;
	SyncthingUtil& operator=(const SyncthingUtil&) = delete;

	// Optional: Disable moving as well
	SyncthingUtil(SyncthingUtil&&) = delete;
	SyncthingUtil& operator=(SyncthingUtil&&) = delete;

	static bool mEnabled;

	// Access control
	static std::once_flag mOnceFlag;
	std::atomic<bool> mApiBusy{false};
	std::mutex mConnectMutex;
	std::mutex mDataMutex;

	bool mInitialized = false;
	bool mConnected = false;
	bool mWifiConnected = false;

	// Syncthing configuration
	std::string mApiKey;
	std::map<std::string, std::shared_ptr<Device>> mDevicesMap;
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

	SyncthingState mLastState;

	void executeScan(Window* window, std::string const* folderId = nullptr);
	SyncthingState getStateFromApi();
	std::string getMyId();
	std::vector<std::string> getConnectedDeviceIds();
	bool parseConfig();
	void updateDeviceCompletion(Device* device);
	long getCurrentTimeMillis();
};
