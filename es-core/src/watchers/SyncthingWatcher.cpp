#include "SyncthingWatcher.h"
#include "LocaleES.h"
#include "Log.h"
#include "SystemConf.h"
#include <algorithm>
#include "utils/StringUtil.h"

#define GUIICON _U("\uF07C ")

SyncthingWatcher::SyncthingWatcher(Window* window) : mWindow(window), mSyncthingUtil(SyncthingUtil::getInstance()) {
	// Check if syncthing service is enabled in system configuration
	if (SystemConf::isServiceActive("syncthing") == true) {
		mSyncthingEnabled = true;
	} else {
		mSyncthingEnabled = false;
	}
}

bool SyncthingWatcher::enabled() {
	return mSyncthingEnabled && mSyncthingUtil.isEnabled();
}

bool SyncthingWatcher::check() {

	if (!enabled()) {
		if (wndNotification != nullptr) {
			wndNotification->close();
			wndNotification = nullptr;
		}
		return true;
	}

	// Check if we are scheduled to kill the notification from the last cycle
    if (wndNotification != nullptr && mkillNotificationInNextCycle) {
        wndNotification->close();
        wndNotification = nullptr;
        mSyncedDevices.clear();
        mkillNotificationInNextCycle = false;
    }

	if(!mSyncthingUtil.isConnected()) {
		if (wndNotification != nullptr) {
			wndNotification->updateText(_("No connected devices available."));
			mkillNotificationInNextCycle = true;
		}
		return true;
	}

	SyncthingState state = mSyncthingUtil.getState();

	// Ignore invalid/empty states
    if (state.totalBytesTransferred <= 0) {
        return true;
    }

	// Check if initialization is complete
    if (!isInitialized(state)) {
		return true;
	}

    // Calculate the delta
    int64_t transferredBytesSinceLastCheck = state.totalBytesTransferred - mTotalBytesTransferred;

    mTotalBytesTransferred = state.totalBytesTransferred;
	

	// Check if any devices have become dirty or are no longer dirty
	for (const auto& dev : mDirtyDevices) {
		if (std::find(state.dirtyDevices.begin(), state.dirtyDevices.end(), dev) == state.dirtyDevices.end()) {
			LOG(LogInfo) << "Syncthing: Device " << dev << " is no longer dirty.";
			mSyncedDevices.push_back(dev);
		}
	}
	mDirtyDevices = state.dirtyDevices;
	std::vector<std::string> cleanDevices;
	for (const auto& dev : state.connectedDevices) {
		if (std::find(state.dirtyDevices.begin(), state.dirtyDevices.end(), dev) == state.dirtyDevices.end()) {
			cleanDevices.push_back(dev);
		}
	}

	// Debug logging
	if (transferredBytesSinceLastCheck > 0) {
		LOG(LogDebug) << "Syncthing: Total bytes transferred updated to " << mTotalBytesTransferred;
	}

	// If nothing is syncing and only 1024 bytes or less have been transferred since last check, skip or close notification
	if (!state.isSyncing() && transferredBytesSinceLastCheck <= 1024) {
		if (wndNotification != nullptr)
		{
			// Display a final synced/complete message before closing
			if (mDirtyDevices.size() > 0 && transferredBytesSinceLastCheck == 0) {
				// If devices are dirty but none are connected, it's not "syncing"
				wndNotification->updateText(_("All devices disconnected."));
				wndNotification->updatePercent(0);
				mCurrentTransferNeededFiles = 0;
			} else if (mDirtyDevices.size() == 0) {
				wndNotification->updateText(_("Synchronization complete."));
				wndNotification->updatePercent(100);
			} else {
				wndNotification->updateText(toSyncedDevicesNameString(mSyncedDevices, true));
				wndNotification->updatePercent(100);
			}
			// Schedule notification to be executed in next cycle.
			mkillNotificationInNextCycle = true;
			return true;
		}
	} else {
		// Let's make sure we aren't showing a notification for background noise.
		if (!state.isSyncing() && transferredBytesSinceLastCheck <= 1024) {
            return true; 
        }

        // If we got here, it's a real sync.
        mkillNotificationInNextCycle = false;

		// Start new syncing notification
		LOG(LogDebug) << "Syncthing: Starting new syncing notification at state itemsSynced=" << state.itemsSynced << " itemsTotal=" << state.itemsTotal << " transferSpeed=" << state.transferSpeed << " transferredBytesSinceLastCheck=" << transferredBytesSinceLastCheck << " dirtyDevices=" << mDirtyDevices.size();
		
		// Create notification window if not existing yet
		if (wndNotification == nullptr) {
			if (mWindow != nullptr) {
				wndNotification = mWindow->createAsyncNotificationComponent();
				wndNotification->updateTitle(GUIICON + _("SYNCTHING"));
				// Calculate number of files to be transferred
				mCurrentTransferNeededFiles = state.itemsTotal - state.itemsSynced;
			} else {
				LOG(LogError) << "Syncthing: Cannot create syncing notification window because Window is null.";
				return false;
			}
		}
		// If we know how many files need to be transferred, show progress
		if (mCurrentTransferNeededFiles > 0) {
            int64_t currentTransferTransferredFiles = mCurrentTransferNeededFiles - (state.itemsTotal - state.itemsSynced);
            // Clamp to avoid negative numbers if Syncthing updates totals mid-sync
            if (currentTransferTransferredFiles < 0) currentTransferTransferredFiles = 0;

            std::string idx = std::to_string(currentTransferTransferredFiles) + "/" + std::to_string(mCurrentTransferNeededFiles);
            int percentDone = (int)((currentTransferTransferredFiles * 100) / mCurrentTransferNeededFiles);

			wndNotification->updateText(_("Transferring file") + " " + idx);
			wndNotification->updatePercent(percentDone);
		// If we know which devices are dirty, but not how many files need to be transferred, list dirty devices
		} else if (mDirtyDevices.size() > 0) {
			wndNotification->updateText(toSyncedDevicesNameString(mDirtyDevices, false));
			wndNotification->updatePercent(0);
		// If we don't know which devices are dirty, but bytes have been transferred since last check, show generic syncing message
		} else if (state.isSyncing()) {
			// If the API says we are syncing but the file counts are zero/equal,
			// it's likely just a database scan. Don't let it hang forever.
			if (state.itemsTotal > 0 && state.itemsTotal > state.itemsSynced) {
				wndNotification->updateText(_("Transfer in progress..."));
				wndNotification->updatePercent(state.getPercentDone());
			} else {
				// If it's just a scan with no actual file backlog, treat it as finished
				wndNotification->updateText(_("Synchronization complete."));
				wndNotification->updatePercent(100);
				mkillNotificationInNextCycle = true;
			}
		// If no devices are dirty and no transfer is in progress, but at least one device has finished syncing
		} else if (mSyncedDevices.size() > 0) {
			wndNotification->updateText(toSyncedDevicesNameString(mSyncedDevices, true));
			wndNotification->updatePercent(100);
			mkillNotificationInNextCycle = true;
		// If no devices are dirty and no transfer is in progress, but more than 1024 bytes have been transferred since last check
		} else if (transferredBytesSinceLastCheck > 1024) {
			wndNotification->updateText(toSyncedDevicesNameString(cleanDevices, true));
			wndNotification->updatePercent(100);
			mkillNotificationInNextCycle = true;
		} else if (transferredBytesSinceLastCheck == 0 && (mCurrentTransferNeededFiles > 0 || mDirtyDevices.size() > 0)) {
			// No bytes transferred since last check, but there are still files to transfer or dirty devices
			wndNotification->updateText(_("All devices are disconnected."));
			wndNotification->updatePercent(0);
			mkillNotificationInNextCycle = true;
			mCurrentTransferNeededFiles = 0;
		}

	}
	return true;
}

std::string SyncthingWatcher::toSyncedDevicesNameString(const std::vector<std::string>& deviceNames, bool synced) {

	std::string names;
    bool first = true;

    for (const auto& deviceName : deviceNames) {
        // Skip IDs that haven't been resolved to names yet
        if (deviceName.length() > 20 || deviceName.find("-") != std::string::npos) {
            continue; 
        }

        if (!first) {
            names += ", ";
        }
        names += deviceName;
        first = false;
    }
	if (names.empty()) {
		return synced ?  _("Synchronization complete.") : _("Transfer in progress...");
	} else {
		if (synced) {
			return Utils::String::format(_("Synced with %s.").c_str(), names.c_str());
		} else {
			return Utils::String::format(_("Syncing with %s.").c_str(), names.c_str());
		}
	}
}

// During launch, syncthing creates a lot of background noise as it scans the database and checks file states.
// We need to ignore this noise until the initial sync is complete.
bool SyncthingWatcher::isInitialized(const SyncthingState& state) {

	if (mInitialized) {
		return true;
	}

    // Secure the baseline
    if (mTotalBytesTransferred == 0) {
        mTotalBytesTransferred = state.totalBytesTransferred;
        LOG(LogInfo) << "Syncthing: Initial baseline set to " << mTotalBytesTransferred;
        return false;
    }

	// Syncthing has determined a steady baseline, initialization seems to be complete.
	if (state.totalBytesTransferred <= mTotalBytesTransferred) {
		mTotalBytesTransferred = state.totalBytesTransferred;
		mInitialized = true;
		LOG(LogInfo) << "Syncthing: Initialization complete. Baseline reset to " << mTotalBytesTransferred;
		return false; // Return false this cycle to avoid showing a notification immediately.
	}

	return false;

}

void SyncthingWatcher::handleEvent(const std::string& event, const std::string& value)
{
    if (event == "syncthing") {
		if (value == "enabled") {
			mSyncthingEnabled = true;

		} else if (value == "disabled") {
			mSyncthingEnabled = false;
		}
	}
}