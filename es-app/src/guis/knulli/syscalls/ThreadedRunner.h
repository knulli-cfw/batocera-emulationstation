#pragma once

#include <thread>
#include "components/AsyncNotificationComponent.h"

class ThreadedRunner
{
public:
	static void start(Window* window, std::string title, std::string text, std::string command);
	static bool isRunning() { return mInstance != nullptr; }

private:
	void run();

	ThreadedRunner(Window* window);
	~ThreadedRunner();

	Window*						mWindow;
	AsyncNotificationComponent* mWndNotification;
	std::string					mCommand;

	std::thread*				mHandle;
	static ThreadedRunner*	mInstance;
};
