#pragma once

#include <thread>
#include "components/AsyncNotificationComponent.h"

class ThreadedDiskCheck
{
public:
	static void start(Window* window);
	static bool isRunning() { return mInstance != nullptr; }

private:
	void run();
	void updateNotificationComponentContent(const std::string info);

	ThreadedDiskCheck(Window* window);
	~ThreadedDiskCheck();

	Window*						mWindow;
	AsyncNotificationComponent* mWndNotification;

	std::thread*				mHandle;
	static ThreadedDiskCheck*	mInstance;
};
