#include "ThreadedRunner.h"
#include "Window.h"
#include "Log.h"
#include "components/AsyncNotificationComponent.h"
#include "guis/GuiMsgBox.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include <chrono>
#include <thread>

#define ICONINDEX _U("\uF085 ")

ThreadedRunner* ThreadedRunner::mInstance = nullptr;

ThreadedRunner::ThreadedRunner(Window* window, std::string title, std::string text, std::string command)
	: mWindow(window), mCommand(command)
{
	if (mWindow != nullptr)
	{
		mWndNotification = mWindow->createAsyncNotificationComponent();
		if (title.empty())
			title = _("WORK IN PROGRESS");
		if (text.empty())
			text = _("Please wait...");
		mWndNotification->updateTitle(ICONINDEX + title);
		mWndNotification->updateText(text);
	}

	std::thread(&ThreadedRunner::run, this).detach();
}

ThreadedRunner::~ThreadedRunner()
{
	// Kill notification if still open
	if (mWndNotification != nullptr)
	{
		mWndNotification->close();
		mWndNotification = nullptr;
	}
	// Set instance to null so that new processes can be started
	ThreadedRunner::mInstance = nullptr;
}

void ThreadedRunner::run()
{
	LOG(LogInfo) << "Running " << mCommand;
	int result = system((mCommand + " 2>&1 >/dev/null").c_str());
	if (result == 0 && mWndNotification != nullptr)
	{
		mWndNotification->updatePercent(100);
		mWndNotification->updateText(_("Operation completed."));
	}
	else
	{
		LOG(LogError) << "Error executing " << mCommand;
		if (mWndNotification != nullptr)
			mWndNotification->updateText(_("Operation failed."));
	}
	std::this_thread::sleep_for(std::chrono::seconds(5));
	delete this;
}

void ThreadedRunner::start(Window* window, std::string title, std::string text, std::string command)
{
	if (ThreadedRunner::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("PLEASE WAIT. ANOTHER PROCESS IS ALREADY RUNNING.")));
		return;
	}
	
	ThreadedRunner::mInstance = new ThreadedRunner(window, title, text, command);
}
