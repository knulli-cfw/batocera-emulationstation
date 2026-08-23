#include "utils/SyncthingUtil.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>
#include <pugixml/src/pugixml.hpp>
#include <chrono>
#include <thread>
#include <memory>
#include "Window.h"
#include "guis/GuiMsgBox.h"
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>
#include <rapidjson/document.h>
#include "FileSystemUtil.h"
#include "LocaleES.h"
#include "components/AsyncNotificationComponent.h"
#include "watchers/NetworkStateWatcher.h"

#define GUIICON _U("\uF07C ")

bool SyncthingUtil::mEnabled = false;
const std::string SYNCTHING_CONFIG_XML = "/userdata/system/configs/syncthing/config.xml";
std::once_flag SyncthingUtil::mOnceFlag;

const std::string SYNCTHING_DEVICE_ID_COMMAND = "/usr/bin/syncthing --device-id --home /userdata/system/configs/syncthing/ 2>/dev/null";

// Initial check if network connection is present
void SyncthingUtil::init() {
	if (mInitialized) {
		return;
	}
	NetworkStateWatcher* watcher = WatchersManager::GetComponent<NetworkStateWatcher>();
	if (watcher != nullptr) {
		mWifiConnected = watcher->isConnected();
	}
	WatchersManager::getInstance()->RegisterNotify(this);
	mInitialized = true;
}

// Returns true if syncthing is enabled and reachable and the respective config file exists.
bool SyncthingUtil::isEnabled() {

	// If API is already enabled, just check if config file exists.
	if (mEnabled) {
        return Utils::FileSystem::exists(SYNCTHING_CONFIG_XML);
    }

	// Check if syncthing API is up
	std::unique_ptr<HttpReq> req(new HttpReq("http://127.0.0.1:8384"));
	if (!req->wait()) {
		return false;
	}
	mEnabled = true;

	// Check if config file exists
	if (!mEnabled || !Utils::FileSystem::exists(SYNCTHING_CONFIG_XML)) {
        return false;
    }

    return true;
}

// Establishes a connection to the syncthing API by verifying the syncthing API
// is reachable and loading all relevant devices and folders from the config XML.
bool SyncthingUtil::connect() {

	if (mConnected)
		return true;

	if (!SyncthingUtil::isEnabled()) {
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(mConnectMutex);
		
        if (mConnected) 
            return true;

		if (!parseConfig()) {
			LOG(LogError) << "Syncthing: Failed to parse config XML, cannot connect.";
			return false;
		}

		mConnected = true;
	}

	return true;
}

// Disconnects from the syncthing API by clearing all configuration data.
void SyncthingUtil::disconnect() {
	std::lock_guard<std::mutex> lock(mDataMutex);
	mConnected = false;

	// Reset status of own device
	self.connected = false;
	self.paused = false;
	self.completion = 0;
	self.needItems = 0;
	self.globalItems = 0;
	self.needBytes = 0;
	self.bytesReceived = 0;
	self.bytesSent = 0;
	self.transferSpeed = 0;

	// Clear existing devices and folders
	mDevicesMap.clear();
	mFolders.clear();
}

// Reconnects to the syncthing API by first disconnecting and then connecting again.
bool SyncthingUtil::reconnect() {
	disconnect();
	return connect();
}

bool SyncthingUtil::reloadConfig() {
	if (parseConfig()) {
		LOG(LogInfo) << "Syncthing: Successfully reloaded config.";
		return true;
	}
	LOG(LogError) << "Syncthing: Failed to reload config.";
	return false;
}

void SyncthingUtil::scan(Window* window, std::string const* folderId) {
    // Try to set busy. If already busy, skip the scan.
    if (mApiBusy.exchange(true)) {
        LOG(LogDebug) << "Syncthing: API is busy with another request, skipping scan.";
        return;
    }

    struct Guard { 
        std::atomic<bool>& flag; 
        ~Guard() { flag.store(false); } 
    } apiGuard{mApiBusy};

	executeScan(window, folderId);
}

// Starts a rescan of all folders or of a specific folder if folderId is provided.
void SyncthingUtil::executeScan(Window* window, std::string const* folderId) {
	if (!reconnect()) {
		LOG(LogError) << "Syncthing: Unable to (re-)connect, cannot start scan";
		return;
	}

	LOG(LogDebug) << "Syncthing: Starting scan";
	AsyncNotificationComponent* wndNotification = window->createAsyncNotificationComponent();
	LOG(LogDebug) << "Syncthing: Created notification window";
	wndNotification->updateTitle(GUIICON + _("SYNCTHING"));

	std::string targetFolderId = "";
    std::string targetFolderLabel = "";
	bool allSharedFoldersPaused = true;
	{
		std::lock_guard<std::mutex> lock(mDataMutex);
		for (const auto& folder : mFolders) {
			if (folder.shared && !folder.paused) {
				allSharedFoldersPaused = false;
			}

			// If we are looking for a specific folder, copy its data now
			if (folderId && folder.id == *folderId) {
				targetFolderId = folder.id;
				targetFolderLabel = folder.label;
			}
		}
	}

	if (self.paused || allSharedFoldersPaused) {
		LOG(LogError) << "Syncthing: Cannot start scan because synchronization is paused on this device or all folders are paused.";
		wndNotification->updateText(_("Synchronization is paused."));
		std::this_thread::sleep_for(std::chrono::seconds(5));
		wndNotification->close();
		return;
	}

	HttpReqOptions options;
	options.customHeaders.push_back("X-Api-Key: " + mApiKey);
	options.dataToPost = "scan"; // TODO: Make sure this works

	std::unique_ptr<HttpReq> req;

	LOG(LogDebug) << "Syncthing: Starting scan request";
	long startTime = getCurrentTimeMillis();

	if (!targetFolderId.empty()) {
		wndNotification->updateText("Scanning folder: " + targetFolderLabel);
		req.reset(new HttpReq("http://127.0.0.1:8384/rest/db/scan?folder=" + targetFolderId, &options));
	} else {
		wndNotification->updateText("Scanning all folders...");
		req.reset(new HttpReq("http://127.0.0.1:8384/rest/db/scan", &options));
	}

	LOG(LogDebug) << "Syncthing: Scan request sent";
	if (req->wait()) {
		long endTime = getCurrentTimeMillis();
		long duration = endTime - startTime;
		LOG(LogDebug) << "Syncthing: Scan completed in " << duration << " milliseconds";
		if (duration < 1000) {
			wndNotification->updateText(_("Scan completed in ") + Utils::String::format("%d ms.", duration));
		} else {
			wndNotification->updateText(_("Scan completed in ") + Utils::String::format("%d seconds.", duration / 1000));
		}
		std::this_thread::sleep_for(std::chrono::seconds(5));
		wndNotification->close();
		wndNotification = nullptr;
	} else {
		wndNotification->updateText("Error starting scan: " + req->getErrorMsg());
		std::this_thread::sleep_for(std::chrono::seconds(5));
		wndNotification->close();
		wndNotification = nullptr;
		disconnect();
		LOG(LogError) << "Syncthing: Scan request failed - " << req->getErrorMsg();
	}
}

SyncthingState SyncthingUtil::getState() {    
    // Try to set busy. If already busy, return empty/last state immediately.
    if (mApiBusy.exchange(true)) {
        LOG(LogDebug) << "Syncthing: API is busy with another request, skipping getState.";
		SyncthingState busyState = mLastState;
		if (busyState.itemsSynced < busyState.itemsTotal) {
			// The last known state indicates syncing is in progress, but
			// since we are currently busy, let's assume there is some activity
			// and set some non-zero values to avoid that the notification closes.
			busyState.transferSpeed = 1;
			busyState.totalBytesTransferred = 2048;
		}
        return busyState;
    }

    struct Guard { 
        std::atomic<bool>& flag; 
        ~Guard() { flag.store(false); } 
    } apiGuard{mApiBusy};

    mLastState = getStateFromApi();
    return mLastState;
}

// Retrieves the current synchronization state from the syncthing API.
SyncthingState SyncthingUtil::getStateFromApi() {
	SyncthingState state;
	state.itemsSynced = 0;
	state.itemsTotal = 0;
	state.transferSpeed = 0;

	if (!mWifiConnected) {
		LOG(LogInfo) << "Syncthing: Not connected to WiFi, cannot get state";
		return state;
	}

	if (!mConnected && !connect()) {
		LOG(LogError) << "Syncthing: Not connected, cannot get state";
		return state;
	}

	std::vector<std::string> deviceIds = getConnectedDeviceIds();
	if (deviceIds.empty()) {
		LOG(LogDebug) << "Syncthing: No connected devices found when getting current state.";
		return state;
	}

	int globalItems = 0;
	int needItems = 0;
	int totalSpeed = 0;

	state.connectedDevices.clear();
	
	{
		std::lock_guard<std::mutex> lock(mDataMutex);

		updateDeviceCompletion(&self);
		globalItems += self.globalItems;
		needItems += self.needItems;
		totalSpeed += self.transferSpeed;

		for (auto& deviceId : deviceIds) {

			auto it = mDevicesMap.find(deviceId);
            if (it == mDevicesMap.end()) continue;
            
            std::shared_ptr<Device> device = it->second;

			updateDeviceCompletion(device.get());
			if (!device->connected || device->paused) continue;
			state.connectedDevices.push_back(device->name);
			globalItems += device->globalItems;
			needItems += device->needItems;
			totalSpeed += device->transferSpeed;
			if (device->needItems > 0) {
				state.dirtyDevices.push_back(device->name);
			} else {
				// Remove device from dirty list if it has no more unsynced items
				auto it = std::find(state.dirtyDevices.begin(), state.dirtyDevices.end(), device->name);
				if (it != state.dirtyDevices.end()) {
					state.dirtyDevices.erase(it);
				}
			}
		}
	}
	int syncedItems = globalItems - needItems;
	state.itemsSynced = syncedItems;
	state.itemsTotal = globalItems;
	state.transferSpeed = totalSpeed;
	state.totalBytesTransferred = self.bytesReceived + self.bytesSent;
	if (totalSpeed == 0) {
		LOG(LogDebug) << "Syncthing: No transfer speed detected, assuming sync is complete even though " << needItems << " files have not been synced yet.";
	}
	return state;
}

// Retrieves own device ID from the syncthing API.
std::string SyncthingUtil::getMyId() {
    std::string cmd = SYNCTHING_DEVICE_ID_COMMAND;
    
    std::vector<std::string> myIds;
    char buffer[128];

    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        LOG(LogError) << "Syncthing: Failed to open pipe for device ID command.";
        return "";
    }

    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        std::string line = buffer;
        // Trim trailing newlines or spaces
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (!line.empty()) {
            myIds.push_back(line);
        }
    }

    if (myIds.empty()) {
        LOG(LogWarning) << "Syncthing: No device ID returned from command.";
        return ""; 
    }

    if (myIds.size() > 1) {
        LOG(LogError) << "Syncthing: Unexpectedly detected " << myIds.size() << " IDs! Using: " << myIds[0];
    }

    return myIds[0];
}

// Retrieves a list of IDs of currently connected devices from the syncthing API.
std::vector<std::string> SyncthingUtil::getConnectedDeviceIds() {
	std::vector<std::string> deviceIds;

	HttpReqOptions options;
	options.customHeaders.push_back("X-Api-Key: " + mApiKey);

	std::unique_ptr<HttpReq> req(new HttpReq("http://127.0.0.1:8384/rest/system/connections", &options));

	if (req->wait()) {
		rapidjson::Document doc;
		doc.Parse(req->getContent().c_str());
		if (doc.HasParseError())
			return deviceIds;

		if (doc.IsObject() == false)
			return deviceIds;

		std::lock_guard<std::mutex> lock(mDataMutex);

		if (doc.HasMember("total")) {
			self.bytesReceived = doc["total"].GetObject()["inBytesTotal"].GetInt64();
            self.bytesSent = doc["total"].GetObject()["outBytesTotal"].GetInt64();
		}
		if (doc.HasMember("connections")) {
			for (auto& member : doc["connections"].GetObject()) {
				// Exclude self and paused devices from list of connected devices
				if (member.name.IsString() == false || std::string(member.name.GetString()) == self.id)
					continue;

				std::shared_ptr<Device> device = nullptr;
				auto it = mDevicesMap.find(member.name.GetString());
				if (it != mDevicesMap.end()) {
					device = it->second;
				}
			
				// Handle unknown device TODO: Create new device instead of skipping?
				if (device == nullptr)
					continue;

				// Skip invalid entires
				if (member.value.IsObject() == false)
					continue;

				// Update device metadata
				if (member.value.HasMember("paused") == true)
					device->paused = member.value["paused"].GetBool();
				if (member.value.HasMember("connected") == true)
					device->connected = member.value["connected"].GetBool();
				if (member.value.HasMember("inBytesTotal") == true && member.value["inBytesTotal"].IsInt64())
					device->bytesReceived = member.value["inBytesTotal"].GetInt64();
				if (member.value.HasMember("outBytesTotal") == true && member.value["outBytesTotal"].IsInt64())
					device->bytesSent = member.value["outBytesTotal"].GetInt64();
				if (!device->connected || device->paused)
					continue;
				deviceIds.push_back(member.name.GetString());
			}
		}
	}
	return deviceIds;
}

// Updates the status of a specific device by querying the syncthing API.
void SyncthingUtil::updateDeviceCompletion(Device* device) {
	if (!device) return;

	HttpReqOptions options;
	options.customHeaders.push_back("X-Api-Key: " + mApiKey);

	std::unique_ptr<HttpReq> req(new HttpReq("http://127.0.0.1:8384/rest/db/completion?device=" + device->id, &options));

	if (!req->wait()) {
		return;
	}
	rapidjson::Document doc;
	doc.Parse(req->getContent().c_str());
	if (doc.HasParseError())
		return;
	if (doc.IsObject() == false)
		return;
	if (doc.GetObject().HasMember("completion") && doc.GetObject()["completion"].IsInt64())
		device->completion = doc.GetObject()["completion"].GetInt64();
	if (doc.GetObject().HasMember("needItems") && doc.GetObject()["needItems"].IsInt64())
		device->needItems = doc.GetObject()["needItems"].GetInt64();
	if (doc.GetObject().HasMember("globalItems") && doc.GetObject()["globalItems"].IsInt64())
		device->globalItems = doc.GetObject()["globalItems"].GetInt64();
	if (doc.GetObject().HasMember("paused") && doc.GetObject()["paused"].IsBool())
		device->paused = doc.GetObject()["paused"].GetBool();
	if (doc.GetObject().HasMember("needBytes") && doc.GetObject()["needBytes"].GetInt64()) {
		int64_t currentNeedBytes = doc.GetObject()["needBytes"].GetInt64();

		// Only record speed if the change is significant (e.g., > 1KB)
		// and if we actually have items left to sync
		if (currentNeedBytes < device->needBytes && (device->needItems > 0)) {
			device->transferSpeed = device->needBytes - currentNeedBytes;
		} else {
			device->transferSpeed = 0; // Explicitly reset to zero
		}
		device->needBytes = currentNeedBytes;
	}
}

bool SyncthingUtil::parseConfig() {
	// Load syncthing config XML
	pugi::xml_document document;
	pugi::xml_parse_result result = document.load_file(SYNCTHING_CONFIG_XML.c_str());
	if (!result) {
		LOG(LogError) << "Unable to parse packages";
		return false;
	}

	pugi::xml_node configurationNode = document.child("configuration");

	mApiKey = configurationNode.child("gui").child("apikey").text().get();
	LOG(LogDebug) << "Syncthing: API key is " << mApiKey;

	// Determine own device ID
	const std::string myId = getMyId();
	if (myId.empty() || myId == "OWN_ID_UNKNOWN") {
		return false;
	}
	self.id = myId;
	LOG(LogInfo) << "Syncthing: Own device ID is " << self.id;

	// Clear map before determining devices.
	std::lock_guard<std::mutex> lock(mDataMutex);
	mDevicesMap.clear();
	mFolders.clear();

	// Load devices
	for (pugi::xml_node deviceNode = configurationNode.child("device"); deviceNode; deviceNode = deviceNode.next_sibling("device")) {
		if (self.id == deviceNode.attribute("id").as_string()) {
			self.name = deviceNode.attribute("name").as_string();
			self.paused = deviceNode.child("paused").text().as_bool();
			LOG(LogInfo) << "Syncthing: Determined own device name " << self.name;
			continue; // Don't add self to device list
		}
		auto device = std::make_shared<Device>();
		device->id = deviceNode.attribute("id").as_string();
		device->name = deviceNode.attribute("name").as_string();
		device->paused = deviceNode.child("paused").text().as_bool();
		LOG(LogInfo) << "Syncthing: Added device with name " << device->name;
		mDevicesMap[device->id] = device;
	}
	// Load folders
	for (pugi::xml_node folderNode = configurationNode.child("folder"); folderNode; folderNode = folderNode.next_sibling("folder")) {
		Folder folder;
		folder.id = folderNode.attribute("id").as_string();
		folder.label = folderNode.attribute("label").as_string();
		folder.path = folderNode.attribute("path").as_string();
		folder.fsWatcherEnabled = folderNode.attribute("fsWatcherEnabled").as_bool();
		if (folderNode.child("paused")) {
			folder.paused = folderNode.child("paused").text().as_bool();
		} else {
			folder.paused = false;
		}
		folder.shared = false;
		// Look at every <device> tag inside this specific <folder>
    	for (pugi::xml_node deviceNode = folderNode.child("device"); deviceNode; deviceNode = deviceNode.next_sibling("device")) {
        	std::string deviceId = deviceNode.attribute("id").as_string();
        
			// If we find a device ID that isn't ours, it's shared with someone else
			if (deviceId != self.id) {
				folder.shared = true;
				break; // No need to keep looking once we find one partner
			}
		}
		LOG(LogInfo) << "Syncthing: Added folder with label " << folder.label;
		mFolders.push_back(folder);
	}

	return true;
}

// Handles WiFi connection status changes.
void SyncthingUtil::OnWatcherChanged(IWatcher* component)
{
	NetworkStateWatcher* watcher = dynamic_cast<NetworkStateWatcher*>(component);
	if (watcher != nullptr)
	{
		mWifiConnected = watcher->isConnected();
	}
}

long SyncthingUtil::getCurrentTimeMillis() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   std::chrono::system_clock::now().time_since_epoch())
		.count();
}
