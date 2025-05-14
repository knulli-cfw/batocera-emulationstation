#include "ThreadedDiskCheck.h"
#include "Window.h"
#include "components/AsyncNotificationComponent.h"
#include "guis/GuiMsgBox.h"
#include "ApiSystem.h"
#include "LocaleES.h"

#define ICONINDEX _U("\uF085 ")

ThreadedDiskCheck* ThreadedDiskCheck::mInstance = nullptr;

ThreadedDiskCheck::ThreadedDiskCheck(Window* window, std::string diskCheckMode)
	: mWindow(window)
{
	mDiskCheckMode = diskCheckMode;
	mWndNotification = mWindow->createAsyncNotificationComponent();
	mWndNotification->updateTitle(ICONINDEX + _("CHECKING YOUR DISK(S)"));	
	mWndNotification->updateText(_("Searching for any issues with your storage..."));

	mHandle = new std::thread(&ThreadedDiskCheck::run, this);
}

ThreadedDiskCheck::~ThreadedDiskCheck()
{
	mWndNotification->close();
	mWndNotification = nullptr;

	ThreadedDiskCheck::mInstance = nullptr;
}

void ThreadedDiskCheck::updateNotificationComponentContent(const std::string info)
{
	if (info.empty())
		return;

	mWndNotification->updatePercent(-1);
	mWndNotification->updateText(info);
}

void ThreadedDiskCheck::run()
{
	ApiSystem::getInstance()->runDiskCheck([this](const std::string info)
	{
		updateNotificationComponentContent(info);
	}, mDiskCheckMode);

	delete this;
	ThreadedDiskCheck::mInstance = nullptr;
}

void ThreadedDiskCheck::start(Window* window, std::string diskCheckMode)
{
	if (ThreadedDiskCheck::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("DISK CHECK IS ALREADY RUNNING.")));
		return;
	}
	
	ThreadedDiskCheck::mInstance = new ThreadedDiskCheck(window, diskCheckMode);
}
