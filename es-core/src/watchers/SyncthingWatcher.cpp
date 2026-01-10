#include "SyncthingWatcher.h"
#include "LocaleES.h"

#define GUIICON _U("\uF03E ")

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
			wndNotification = mWindow->createAsyncNotificationComponent();
			wndNotification->updateTitle(GUIICON + _("SYNCTHING"));
		}
		std::string idx = std::to_string(state.itemsSynced) + "/" + std::to_string(state.itemsTotal);
		wndNotification->updateText(_("Synchronizing file") + " " + idx);
		wndNotification->updatePercent(state.getPercentDone());
		return true;
	} else{
		if (wndNotification != nullptr)
		{
			wndNotification->close();
			wndNotification = nullptr;
		}
		return false;
	}

	return false;
}