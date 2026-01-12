#include "SyncthingWatcher.h"
#include "LocaleES.h"

#define GUIICON _U("\uF07C ")

SyncthingWatcher::SyncthingWatcher(Window* window) : mWindow(window)
	, mSyncthingUtil(SyncthingUtil::getInstance())
{
}

bool SyncthingWatcher::enabled()
{
	return mSyncthingUtil.isEnabled();
}

bool SyncthingWatcher::check()
{
	if (!SyncthingUtil::isEnabled() || !mSyncthingUtil.isConnected())
	{
		if (wndNotification != nullptr) {
			wndNotification->close();
			delete wndNotification; // Fixed: Explicitly free the UI component
			wndNotification = nullptr;
		}
		return false;
	}

	SyncthingState state = mSyncthingUtil.getState();
	if (state.isSyncing())
	{
		if (wndNotification == nullptr)
		{
			mCurrentTransferNeededFiles = state.itemsTotal - state.itemsSynced;
			wndNotification = mWindow->createAsyncNotificationComponent();
			wndNotification->updateTitle(GUIICON + _("SYNCTHING"));
		}
		
		int currentTransferTransferredFiles = mCurrentTransferNeededFiles - (state.itemsTotal - state.itemsSynced);
		
		// Ensure we don't divide by zero if Syncthing reports 0 total items
		if (mCurrentTransferNeededFiles > 0) {
			std::string idx = std::to_string(currentTransferTransferredFiles) + "/" + std::to_string(mCurrentTransferNeededFiles);
			int percentDone = (currentTransferTransferredFiles * 100) / mCurrentTransferNeededFiles;
			wndNotification->updateText(_("Transferring file") + " " + idx);
			wndNotification->updatePercent(percentDone);
		}
		return true;
	} else {
		if (wndNotification != nullptr)
		{
			// If we were just syncing, show completion briefly before deleting
			if (mCurrentTransferNeededFiles > 0) {
				wndNotification->updateText(_("Synchronization complete."));
				wndNotification->updatePercent(100);
				mCurrentTransferNeededFiles = 0; 
			} else {
				wndNotification->close();
				delete wndNotification; // Fixed: Explicitly free the UI component
				wndNotification = nullptr;
			}
		}
		return false;
	}
}