#pragma once

#include "WatchersManager.h"
#include "utils/Platform.h"
#include "utils/SyncthingUtil.h"
#include "components/AsyncNotificationComponent.h"

class SyncthingWatcher : public IWatcher
{

public:
	SyncthingWatcher(Window* window);

protected:
	bool enabled() override;

	int  initialUpdateTime() override { return 0; }		// Immediate
	int  updateTime() override { return 5 * 1000; }		// 5 seconds

	bool check() override;

private:
	SyncthingUtil& mSyncthingUtil;
	Window* mWindow;
	AsyncNotificationComponent* wndNotification = nullptr;
};
