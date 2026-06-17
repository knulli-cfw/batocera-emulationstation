#pragma once
#include "guis/knulli/ExtendedGuiSettings.h"
#include "components/OptionListComponent.h"
#include "guis/knulli/syscalls/ClockSettings.h"
#include <memory>
#include <string>

class GuiClockSettings : public ExtendedGuiSettings
{
public:
    GuiClockSettings(Window* window);

private:

    ClockSettings::Clock mCurrentClock;
    std::string mCurrentTimezone;

    bool mSynchronized = false;

    std::shared_ptr<OptionListComponent<int>> optionListYear;
    std::shared_ptr<OptionListComponent<int>> optionListMonth;
    std::shared_ptr<OptionListComponent<int>> optionListDay;
    std::shared_ptr<OptionListComponent<int>> optionListHour;
    std::shared_ptr<OptionListComponent<int>> optionListMinute;
    std::shared_ptr<OptionListComponent<std::string>> optionListTimezone;
    std::shared_ptr<SwitchComponent> switchClockMode12;

    std::shared_ptr<OptionListComponent<int>> createOptionListYear();
    std::shared_ptr<OptionListComponent<int>> createOptionListMonth();
    std::shared_ptr<OptionListComponent<int>> createOptionListDay();
    std::shared_ptr<OptionListComponent<int>> createOptionListHour();
    std::shared_ptr<OptionListComponent<int>> createOptionListMinute();
    std::shared_ptr<OptionListComponent<std::string>> createOptionListTimezone();

};
