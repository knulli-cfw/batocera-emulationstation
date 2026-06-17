#include "guis/knulli/GuiClockSettings.h"
#include "guis/GuiMsgBox.h"
#include "guis/knulli/ExtendedGuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CapabilityCheck.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "Scripting.h"
#include <array>
#include "utils/StringUtil.h"

GuiClockSettings::GuiClockSettings(Window* window) : ExtendedGuiSettings(window, "DATE/TIME SETTINGS")
{

    mCurrentClock = ClockSettings::get();
    mCurrentTimezone = ClockSettings::getTimezone();

    optionListTimezone = createOptionListTimezone();
    addWithDescription(_("TIME ZONE"), _("Select your local time zone."), optionListTimezone);
    switchClockMode12 = addSwitch(_("SHOW CLOCK IN 12-HOUR FORMAT"), "ClockMode12", true);
    addWithDescription(_("Synchronize now"), _("Attempt to synchronize date and time via internet."), nullptr, [this]
    {
        mWindow->pushGui(new GuiMsgBox(mWindow, _("SYNCHRONIZE DATE AND TIME NOW?"), _("YES"), [this]
            {
                mSynchronized = ClockSettings::synchronize();
                if (!mSynchronized) {
                    mWindow->pushGui(new GuiMsgBox(mWindow, _("SYNCHRONIZATION FAILED. PLEASE CHECK YOUR INTERNET CONNECTION.")));
                }
            },
            _("NO"), nullptr));
    });

    addGroup(_("DATE"));
    optionListYear = createOptionListYear();
    addWithDescription(_("YEAR"), _("Set the current year."), optionListYear);
    optionListMonth = createOptionListMonth();
    addWithDescription(_("MONTH"), _("Set the current month."), optionListMonth);
    optionListDay = createOptionListDay();
    addWithDescription(_("DAY"), _("Set the current day."), optionListDay);

    addGroup(_("TIME"));
    optionListHour = createOptionListHour();
    addWithDescription(_("HOUR"), _("Set the current hour."), optionListHour);
    optionListMinute = createOptionListMinute();
    addWithDescription(_("MINUTE"), _("Set the current minute."), optionListMinute);

addSaveFunc([this] {
        bool systemConfChanged = false;

        // Handle possible timezone change first
        std::string selectedTZ = optionListTimezone->getSelected();
        if (selectedTZ.empty()) {
            selectedTZ = "UTC";
        }

        // Only update timezone if it was actually changed by the user
        if (selectedTZ != mCurrentTimezone) {
            if (SystemConf::getInstance()->set("system.timezone", selectedTZ)) {
                ApiSystem::getInstance()->setTimezone(selectedTZ);
                systemConfChanged = true;
            }
            ClockSettings::setTimezone(selectedTZ);
        }

        // Check if any time value was actually modified by the user
        int selectedYear   = optionListYear->getSelected();
        int selectedMonth  = optionListMonth->getSelected();
        int selectedDay    = optionListDay->getSelected();
        int selectedHour   = optionListHour->getSelected();
        int selectedMinute = optionListMinute->getSelected();

        bool timeChanged = (selectedYear   != mCurrentClock.year  ||
                            selectedMonth  != mCurrentClock.month ||
                            selectedDay    != mCurrentClock.day   ||
                            selectedHour   != mCurrentClock.hour  ||
                            selectedMinute != mCurrentClock.minute);

        // Only set the clock if NOT synchronized via NTP and at least one value was changed
        if (!mSynchronized && timeChanged) {
            ClockSettings::Clock newClock;
            newClock.year = selectedYear;
            newClock.month = selectedMonth;
            newClock.day = selectedDay;
            newClock.hour = selectedHour;
            newClock.minute = selectedMinute;

            // Sanitize the day of month based on the selected month and year
            int maxDays = 31;
            if (newClock.month == 4 || newClock.month == 6 || newClock.month == 9 || newClock.month == 11) {
                maxDays = 30;
            } else if (newClock.month == 2) {
                // Check for leap year
                if ((newClock.year % 4 == 0 && newClock.year % 100 != 0) || (newClock.year % 400 == 0)) {
                    maxDays = 29;
                } else {
                    maxDays = 28;
                }
            }

            // Adjust to valid max of month if needed
            if (newClock.day > maxDays) {
                newClock.day = maxDays;
            }

            ClockSettings::set(newClock);
        }

        // Save system configuration if timezone fields were modified
        if (systemConfChanged) {
            SystemConf::getInstance()->saveSystemConf();
        }
    });
}

std::shared_ptr<OptionListComponent<int>> GuiClockSettings::createOptionListYear()
{
    auto optionList = std::make_shared<OptionListComponent<int>>(mWindow, _("YEAR"), false);

    std::string selectedYear = std::to_string(mCurrentClock.year);
    for (int year = 1970; year <= 2100; year++) {
        optionList->add(std::to_string(year), year, selectedYear == std::to_string(year));
    }
    return optionList;
}

std::shared_ptr<OptionListComponent<int>> GuiClockSettings::createOptionListMonth()
{
    auto optionList = std::make_shared<OptionListComponent<int>>(mWindow, _("MONTH"), false);

    static const std::array<std::string, 12> monthNames = {
        _("JANUARY"), _("FEBRUARY"), _("MARCH"), _("APRIL"), _("MAY"), _("JUNE"),
        _("JULY"), _("AUGUST"), _("SEPTEMBER"), _("OCTOBER"), _("NOVEMBER"), _("DECEMBER")
    };

    for (int month = 1; month <= 12; month++) {
        optionList->add(monthNames[month - 1], month, mCurrentClock.month == month);
    }
    
    return optionList;

}

std::shared_ptr<OptionListComponent<int>> GuiClockSettings::createOptionListDay()
{
    auto optionList = std::make_shared<OptionListComponent<int>>(mWindow, _("DAY"), false);

    // XXX: We add all days until 31 here because we cannot refresh
    // the list of days dynamically based on the selected month and year
    for (int day = 1; day <= 31; day++) {
        std::string label = (day < 10) ? "0" + std::to_string(day) : std::to_string(day);
        optionList->add(label, day, mCurrentClock.day == day);
    }
    
    return optionList;
}

std::shared_ptr<OptionListComponent<int>> GuiClockSettings::createOptionListHour()
{
    auto optionList = std::make_shared<OptionListComponent<int>>(mWindow, _("HOUR"), false);

    for (int hour = 0; hour <= 23; hour++) {
        std::string label = (hour < 10) ? "0" + std::to_string(hour) : std::to_string(hour);
        
        optionList->add(label, hour, mCurrentClock.hour == hour);
    }
    
    return optionList;
}

std::shared_ptr<OptionListComponent<int>> GuiClockSettings::createOptionListMinute()
{
    auto optionList = std::make_shared<OptionListComponent<int>>(mWindow, _("MINUTE"), false);

    for (int minute = 0; minute <= 59; minute++) {
        std::string label = (minute < 10) ? "0" + std::to_string(minute) : std::to_string(minute);
        
        optionList->add(label, minute, mCurrentClock.minute == minute);
    }
    
    return optionList;
}

std::shared_ptr<OptionListComponent<std::string>> GuiClockSettings::createOptionListTimezone()
{
    auto optionList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SELECT YOUR TIME ZONE"), false);
    if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::TIMEZONES))
    {
        VectorEx<std::string> availableTimezones = ApiSystem::getInstance()->getTimezones();
        
        if (availableTimezones.size() > 0)
        {
            std::string currentTZ = mCurrentTimezone;
            if (currentTZ.empty() || !availableTimezones.any([currentTZ](const std::string& tz) { return tz == currentTZ; })) {
                currentTZ = "UTC";
            }

            for (auto tz : availableTimezones) {
                optionList->add(_(Utils::String::toUpper(tz).c_str()), tz, currentTZ == tz);
            }
            return optionList;
        }
    }

    optionList->add("UTC", "UTC", true);
    return optionList;
}

