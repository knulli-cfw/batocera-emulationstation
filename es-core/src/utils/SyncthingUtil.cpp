#include "utils/SyncthingUtil.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "Log.h"
#include "HttpReq.h"
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

#define GUIICON _U("\uF07C ")

const std::string SYNCTHING_CONFIG_XML = "/userdata/system/configs/syncthing/config.xml";

// Returns true if syncthing is enabled and reachable and the respective config file exists.
bool SyncthingUtil::isEnabled()
{

	// Check if syncthing API is up
	std::unique_ptr<HttpReq> req(new HttpReq("http://127.0.0.1:8384"));
	if (!req->wait())
	{
		return false;
	}

	// Check if config file exists
	if (!Utils::FileSystem::exists(SYNCTHING_CONFIG_XML)) {
		return false;
	}
	return true;
}

// Establishes a connection to the syncthing API by verifying the syncthing API
// is reachable and loading all relevant devices and folders from the config XML.
bool SyncthingUtil::connect()
{
	if (mConnected)
		return true;

	if (!SyncthingUtil::isEnabled()) {
		mConnected = false;
		return false;
	}

	// Load syncthing config XML
	pugi::xml_document document;
	pugi::xml_parse_result result = document.load_file(SYNCTHING_CONFIG_XML.c_str());
	if (!result)
	{
		LOG(LogError) << "Unable to parse packages";
		mConnected = false;
		return false;
	}

	pugi::xml_node configurationNode = document.child("configuration");

	mApiKey = configurationNode.child("gui").child("apikey").text().get();
	LOG(LogDebug) << "Syncthing: API key is " << mApiKey;

	// Determine own device ID
	self.id = getMyId();
	LOG(LogInfo) << "Syncthing: Own device ID is " << self.id;

	// Load devices
	for (pugi::xml_node deviceNode = configurationNode.child("device"); deviceNode; deviceNode = deviceNode.next_sibling("device")) {
		if (std::string(deviceNode.attribute("id").as_string()) == self.id) {
			self.name = deviceNode.attribute("name").as_string();
			self.paused = deviceNode.child("paused").text().as_bool();
			LOG(LogInfo) << "Syncthing: Determined own device name " << self.name;
			continue; // Don't add self to device list
		}
		Device device;
		device.id = deviceNode.attribute("id").as_string();
		device.name = deviceNode.attribute("name").as_string();
		device.paused = deviceNode.child("paused").text().as_bool();
		LOG(LogInfo) << "Syncthing: Added device with name " << device.name;
		mDevices.push_back(device);
	}
	// Load folders
	for (pugi::xml_node folderNode = configurationNode.child("folder"); folderNode; folderNode = folderNode.next_sibling("folder")) {
		Folder folder;
		folder.id = folderNode.attribute("id").as_string();
		folder.label = folderNode.attribute("label").as_string();
		folder.path = folderNode.attribute("path").as_string();
		folder.fsWatcherEnabled = folderNode.attribute("fsWatcherEnabled").as_bool();
		LOG(LogInfo) << "Syncthing: Added folder with label " << folder.label;
		mFolders.push_back(folder);
	}

	mConnected = true;
	return true;
}

// Disconnects from the syncthing API by clearing all configuration data.
void SyncthingUtil::disconnect()
{
	mConnected = false;

	// Reset status of own device
	self = Device {
		.id = "self",
		.name = "self",
		.paused = false,
		.completion = 0,
		.needItems = 0,
		.globalItems = 0,
		.needBytes = 0,
		.transferSpeed = 0
	};

	// Clear existing devices and folders
	mDevices.clear();
	mFolders.clear();
}

// Reconnects to the syncthing API by first disconnecting and then connecting again.
bool SyncthingUtil::reconnect()
{
	disconnect();
	return connect();
}

// Starts a rescan of all folders or of a specific folder if folderId is provided.
void SyncthingUtil::scan(Window* window, std::string const* folderId)
{
	if (!reconnect()) {
		LOG(LogError) << "Syncthing: Unable to (re-)connect, cannot start scan";
		return;
	}

	LOG(LogDebug) << "Syncthing: Starting scan";
	AsyncNotificationComponent* wndNotification = window->createAsyncNotificationComponent();
	LOG(LogDebug) << "Syncthing: Created notification window";
	wndNotification->updateTitle(GUIICON + _("SYNCTHING"));

	Folder* folder = nullptr;
	if (folderId) {
		folder = getFolderById(*folderId);
		if (folder) {
			wndNotification->updateText("Scanning folder: " + folder->label);
		} else {
			wndNotification->updateText("Scanning all folders...");
		}
	} else {
		wndNotification->updateText("Scanning all folders...");
	}
	LOG(LogDebug) << "Syncthing: Starting scan request";
	
	HttpReqOptions options;
	options.customHeaders.push_back("X-Api-Key: " + mApiKey);
	options.dataToPost = "scan"; // TODO: Make sure this works

	std::unique_ptr<HttpReq> req;

	if(!folder) {
		// Sync all folders
		req.reset(new HttpReq("http://127.0.0.1:8384/rest/db/scan", &options));
	} else {
		// Sync only specified folder
		req.reset(new HttpReq("http://127.0.0.1:8384/rest/db/scan?folder=" + folder->id, &options));
	}

	LOG(LogDebug) << "Syncthing: Scan request sent";
	if (req->wait())
	{
		wndNotification->close();
		wndNotification = nullptr;
	}
	else
	{
		wndNotification->updateText("Error starting scan: " + req->getErrorMsg());
		std::this_thread::sleep_for(std::chrono::seconds(3));
		wndNotification->close();
		wndNotification = nullptr;
		disconnect();
		LOG(LogError) << "Syncthing: Scan request failed - " << req->getErrorMsg();
	}
}

// Retrieves the current synchronization state from the syncthing API.
SyncthingState SyncthingUtil::getState() {

	SyncthingState state;
	state.itemsSynced = 0;
	state.itemsTotal = 0;
	state.transferSpeed = 0;

	if (!mConnected && !connect()) {
		LOG(LogError) << "Syncthing: Not connected, cannot get state";
		return state;
	}

	// If no devices are connected, there's nothing to wait for
	std::vector<std::string> deviceIds = getConnectedDeviceIds();
	if (deviceIds.size() == 0)
		return state;

	int globalItems = 0;
	int needItems = 0;
	int totalSpeed = 0;

	updateDevice(&self);
	globalItems += self.globalItems;
	needItems += self.needItems;
	totalSpeed += self.transferSpeed;

	for (auto& deviceId : getConnectedDeviceIds()) {
		Device* device = getDeviceById(deviceId);
		if (!device) continue;
		updateDevice(device);
		if (device->paused) continue;
		globalItems += device->globalItems;
		needItems += device->needItems;
		totalSpeed += device->transferSpeed;
	}

	int syncedItems = globalItems - needItems;
	std::string idx = std::to_string(syncedItems) + "/" + std::to_string(globalItems);

	if (needItems == 0) {
		return state;
	}

	if (totalSpeed == 0) {
		LOG(LogDebug) << "Syncthing: No transfer speed detected, assuming sync is complete even though " << needItems << " files have not been synced yet.";
		return state;
	
	}

	state.itemsSynced = syncedItems;
	state.itemsTotal = globalItems;
	state.transferSpeed = totalSpeed;
	return state;

}

// Retrieves a device by its ID, or nullptr if not found.
Device* SyncthingUtil::getDeviceById(const std::string& deviceId) {
	for (auto& device : mDevices) {
		if (device.id == deviceId) {
			return &device;
		}
	}
	return nullptr;
}

// Retrieves a folder by its ID, or nullptr if not found.
Folder* SyncthingUtil::getFolderById(const std::string& folderId) {
	for (auto& folder : mFolders) {
		if (folder.id == folderId) {
			return &folder;
		}
	}
	return nullptr;
}

// Retrieves own device ID from the syncthing API.
std::string SyncthingUtil::getMyId()
{
	HttpReqOptions options;
	options.customHeaders.push_back("X-Api-Key: " + mApiKey);

	std::unique_ptr<HttpReq> req(new HttpReq("http://127.0.0.1:8384/rest/system/status", &options));
	
	if (req->wait())
	{
		rapidjson::Document doc;
		doc.Parse(req->getContent().c_str());
		if (doc.HasParseError())
			return "OWN_ID_UNKNOWN";


		if (doc.GetObject().HasMember("myID") && doc.GetObject()["myID"].IsString())
			return doc.GetObject()["myID"].GetString();

	}
	return "OWN_ID_UNKNOWN";
}

// Retrieves a list of IDs of currently connected devices from the syncthing API.
std::vector<std::string> SyncthingUtil::getConnectedDeviceIds() {
	std::vector<std::string> deviceIds;

	HttpReqOptions options;
	options.customHeaders.push_back("X-Api-Key: " + mApiKey);

	std::unique_ptr<HttpReq> req(new HttpReq("http://127.0.0.1:8384/rest/system/connections", &options));

	if (req->wait())
	{
		rapidjson::Document doc;
		doc.Parse(req->getContent().c_str());
		if (doc.HasParseError())
			return deviceIds;
		
		
		if (doc.IsObject() == false || doc.HasMember("connections") == false)
			return deviceIds;

		for (auto& member : doc["connections"].GetObject())
		{
			// Exclude self and paused devices from list of connected devices
			if (member.name.IsString() == false || std::string(member.name.GetString()) == self.id)
				continue;
			Device* device = getDeviceById(member.name.GetString());
			if (device->paused)	
				continue;
			deviceIds.push_back(member.name.GetString());
		}
		
	}

	return deviceIds;
}

// Updates the status of a specific device by querying the syncthing API.
void SyncthingUtil::updateDevice(Device* device)
{
	if (!device) return;

	HttpReqOptions options;
	options.customHeaders.push_back("X-Api-Key: " + mApiKey);

	std::unique_ptr<HttpReq> req(new HttpReq("http://127.0.0.1:8384/rest/db/completion?device=" + device->id, &options));

	if (req->wait())
	{
		rapidjson::Document doc;
		doc.Parse(req->getContent().c_str());
		if (doc.HasParseError())
			return;

		if (doc.GetObject().HasMember("completion") && doc.GetObject()["completion"].IsInt())
			device->completion = doc.GetObject()["completion"].GetInt();
		if (doc.GetObject().HasMember("needItems") && doc.GetObject()["needItems"].IsInt())
			device->needItems = doc.GetObject()["needItems"].GetInt();
		if (doc.GetObject().HasMember("globalItems") && doc.GetObject()["globalItems"].IsInt())
			device->globalItems = doc.GetObject()["globalItems"].GetInt();
		if (doc.GetObject().HasMember("paused") && doc.GetObject()["paused"].IsBool())
			device->paused = doc.GetObject()["paused"].GetBool();
		if (doc.GetObject().HasMember("needBytes") && doc.GetObject()["needBytes"].IsInt()) {
			int currentNeedBytes = doc.GetObject()["needBytes"].GetInt();
			int distance = 0;
			if (currentNeedBytes > device->needBytes) {
				device->needBytes = currentNeedBytes;
				device->transferSpeed = 1;
			} else {
				distance = device->needBytes - currentNeedBytes;
				device->needBytes = currentNeedBytes;
				device->transferSpeed = distance;
			}
		}
	}

}