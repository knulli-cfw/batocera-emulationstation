#include "ThreadedSyncthing.h"
#include "Window.h"
#include "components/AsyncNotificationComponent.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"

ThreadedSyncthing* ThreadedSyncthing::mInstance = nullptr;

ThreadedSyncthing::ThreadedSyncthing(Window* window)
	: mWindow(window)
{
	mHandle = new std::thread(&ThreadedSyncthing::run, this);
}

ThreadedSyncthing::~ThreadedSyncthing()
{
	ThreadedSyncthing::mInstance = nullptr;
}

void ThreadedSyncthing::run()
{
	mSyncthingUtil.scan(mWindow);
	delete this;
	ThreadedSyncthing::mInstance = nullptr;
}

void ThreadedSyncthing::start(Window* window)
{
	if (ThreadedSyncthing::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("SYNCHRONIZATION ALREADY RUNNING.")));
		return;
	}

	ThreadedSyncthing::mInstance = new ThreadedSyncthing(window);
}