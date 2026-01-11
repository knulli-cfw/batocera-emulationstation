#include "SyncthingWatcher.h"
#include "LocaleES.h"

#define GUIICON _U("\uF07C ")

SyncthingWatcher::SyncthingWatcher(Window* window) : mWindow(window)
	, mSyncthingUtil(SyncthingUtil::getInstance())
{
	// Do nothing
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
		std::string idx = std::to_string(currentTransferTransferredFiles) + "/" + std::to_string(mCurrentTransferNeededFiles);
		int percentDone = currentTransferTransferredFiles * 100 / mCurrentTransferNeededFiles;
		wndNotification->updateText(_("Transferring file") + " " + idx);
		wndNotification->updatePercent(percentDone);
		return true;
	} else {
		if (wndNotification != nullptr)
		{
			if (mCurrentTransferNeededFiles > 0) {
				wndNotification->updateText(_("Synchronization complete."));
				wndNotification->updatePercent(100);
			} else {
				wndNotification->close();
				wndNotification = nullptr;
			}
		}
		mCurrentTransferNeededFiles = 0;
		return false;
	}

	return false;
}