#pragma once

#include <thread>
#include "components/AsyncNotificationComponent.h"
#include "utils/SyncthingUtil.h"

class ThreadedSyncthing
{
public:
	static void start(Window* window);
	static bool isRunning() { return mInstance != nullptr; }

private:
	void run();

	ThreadedSyncthing(Window* window);
	~ThreadedSyncthing();

	Window*						mWindow;

	std::thread*				mHandle;
	static ThreadedSyncthing*	mInstance;

	SyncthingUtil&				mSyncthingUtil = SyncthingUtil::getInstance();
};