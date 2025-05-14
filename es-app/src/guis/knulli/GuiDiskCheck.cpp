#include "GuiDiskCheck.h"

#include "components/SwitchComponent.h"
#include "components/OptionListComponent.h"
#include "guis/GuiMsgBox.h"
#include "ThreadedDiskCheck.h"

GuiDiskCheck::GuiDiskCheck(Window* window) : GuiSettings(window, _("DISK CHECK").c_str())
{

	diskCheckMode = std::make_shared< OptionListComponent<std::string> >(mWindow, _("DISK_CHECK_MODE"), false);
	diskCheckMode->add(_("FAST"), "fast", true);
	diskCheckMode->add(_("FULL"), "full", false);
	addWithDescription(_("DISK CHECK MODE"),_("Full mode is more thorough but takes longer.") , diskCheckMode);

	mMenu.clearButtons();
	mMenu.addButton(_("RUN DISK CHECK"), _("START"), std::bind(&GuiDiskCheck::pressedStart, this));
	mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });

}

void GuiDiskCheck::pressedStart()
{
	if (ThreadedDiskCheck::isRunning())
		mWindow->pushGui(new GuiMsgBox(mWindow, _("DISK CHECK IS ALREADY RUNNING.")));
	else
    {
	    std::string selectedDiskCheckMode = diskCheckMode->getSelected();
	    ThreadedDiskCheck::start(mWindow, selectedDiskCheckMode);
        close();
    }
}