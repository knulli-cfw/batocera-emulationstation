#pragma once

#include <thread>
#include "components/AsyncNotificationComponent.h"

class ThreadedDiskCheck
{
public:
	static void start(Window* window, std::string diskCheckMode = "fast");
	static bool isRunning() { return mInstance != nullptr; }

private:
	void run();
	void updateNotificationComponentContent(const std::string info);

	ThreadedDiskCheck(Window* window, std::string diskCheckMode);
	~ThreadedDiskCheck();

	Window*						mWindow;
	AsyncNotificationComponent* mWndNotification;

	std::thread*				mHandle;
	std::string					mDiskCheckMode;
	static ThreadedDiskCheck*	mInstance;
};
