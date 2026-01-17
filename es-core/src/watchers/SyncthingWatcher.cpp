#include "SyncthingWatcher.h"
#include "LocaleES.h"
#include "Log.h"
#include <algorithm>

#define GUIICON _U("\uF07C ")

SyncthingWatcher::SyncthingWatcher(Window* window) : mWindow(window), mSyncthingUtil(SyncthingUtil::getInstance()) {
}

bool SyncthingWatcher::enabled() {
	return mSyncthingUtil.isEnabled();
}

bool SyncthingWatcher::check() {

	if (!SyncthingUtil::isEnabled() || !mSyncthingUtil.isConnected()) {
		if (wndNotification != nullptr) {
			wndNotification->close();
			wndNotification = nullptr;
		}
		return true;
	}

	SyncthingState state = mSyncthingUtil.getState();
	std::vector<std::string> syncedDevices;
	// Check if any devices have become dirty or are no longer dirty
	for (const auto& dev : mDirtyDevices) {
		if (std::find(state.dirtyDevices.begin(), state.dirtyDevices.end(), dev) == state.dirtyDevices.end()) {
			LOG(LogInfo) << "Syncthing: Device " << dev << " is no longer dirty.";
			syncedDevices.push_back(dev);
		}
	}
	mDirtyDevices = state.dirtyDevices;
	std::vector<std::string> cleanDevices;
	for (const auto& dev : state.connectedDevices) {
		if (std::find(state.dirtyDevices.begin(), state.dirtyDevices.end(), dev) == state.dirtyDevices.end()) {
			cleanDevices.push_back(dev);
		}
	}

	// Calculate number of bytes transferred since last check
	int64_t transferredBytesSinceLastCheck = state.totalBytesTransferred - mTotalBytesTransferred;

	// Update total bytes transferred
	mTotalBytesTransferred = state.totalBytesTransferred;

	// Debug logging
	if (transferredBytesSinceLastCheck > 0) {
		LOG(LogInfo) << "Syncthing: Total bytes transferred updated to " << mTotalBytesTransferred;
	}

	// If nothing is syncing and no devices are dirty, and only 128 bytes or less have been transferred since last check, skip or close notification
	if (!state.isSyncing() && transferredBytesSinceLastCheck <= 128) {
		if (wndNotification != nullptr)
		{
			// If previous cycle already showed finished message, close notification
			if (mkillNotificationInNextCycle) {
				wndNotification->close();
				wndNotification = nullptr;
				mkillNotificationInNextCycle = false;
			// Show finished message
			} else {
				if (syncedDevices.size() == 0 && mDirtyDevices.size() > 0) {
					wndNotification->updateText(_("No device available for sync."));
					wndNotification->updatePercent(0);
				} else if (syncedDevices.size() == 0 && mDirtyDevices.size() == 0) {
					wndNotification->updateText(_("Synchronization complete."));
					wndNotification->updatePercent(100);
				} else {
					wndNotification->updateText(_("Synced with") + " " + toSyncedDevicesNameString(syncedDevices) + ".");
					wndNotification->updatePercent(100);
				}
				mCurrentTransferNeededFiles = 0;
				mkillNotificationInNextCycle = true;
			}
		}
	} else {
		// Start new syncing notification
		LOG(LogInfo) << "Syncthing: Starting new syncing notification at state itemsSynced=" << state.itemsSynced << " itemsTotal=" << state.itemsTotal << " transferSpeed=" << state.transferSpeed << " transferredBytesSinceLastCheck=" << transferredBytesSinceLastCheck << " dirtyDevices=" << mDirtyDevices.size();
		
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
			wndNotification->updateText(_("Syncing with") + " " + toSyncedDevicesNameString(mDirtyDevices) + ".");
			wndNotification->updatePercent(0);
		// If we don't know which devices are dirty, but bytes have been transferred since last check, show generic syncing message
		} else if (state.isSyncing()) {
			wndNotification->updateText(_("Transfer in progress."));
			wndNotification->updatePercent(0);
		// If no devices are dirty and no transfer is in progress, but at least one device has finished syncing
		} else if (syncedDevices.size() > 0) {
			wndNotification->updateText(_("Synced with") + " " + toSyncedDevicesNameString(syncedDevices) + ".");
			wndNotification->updatePercent(100);
			mkillNotificationInNextCycle = true;
		// If no devices are dirty and no transfer is in progress, but more than 128 bytes have been transferred since last check
		} else if (transferredBytesSinceLastCheck > 128) {
			wndNotification->updateText(_("Synced with") + " " + toSyncedDevicesNameString(cleanDevices) + ".");
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

std::string SyncthingWatcher::toSyncedDevicesNameString(const std::vector<std::string>& deviceNames) {
	std::string result;
	for (size_t i = 0; i < deviceNames.size(); i++) {
		result += deviceNames[i];
		if (i < deviceNames.size() - 1) {
			result += ", ";
		}
	}
	return result;
}
