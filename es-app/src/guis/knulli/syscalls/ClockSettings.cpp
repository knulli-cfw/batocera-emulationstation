#include "ClockSettings.h"
#include "SysCalls.h"
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>

ClockSettings::Clock ClockSettings::get() {
Clock currentClock;
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);
    
    currentClock.year   = localTime->tm_year + 1900;
    currentClock.month  = localTime->tm_mon + 1;
    currentClock.day    = localTime->tm_mday;
    currentClock.hour   = localTime->tm_hour;
    currentClock.minute = localTime->tm_min;
    
    // Get current timezone
    std::string fullPath = SysCalls::executeAndCatchOutput("readlink /etc/localtime");
    std::string prefix = "/usr/share/zoneinfo/";
    size_t pos = fullPath.find(prefix);
    
    if (pos != std::string::npos) {
        // Clean up the timezone string by removing the prefix
        std::string tz = fullPath.substr(pos + prefix.length());
        
        // Remove any trailing newline or carriage return characters
        if (!tz.empty() && tz.back() == '\n') tz.pop_back();
        if (!tz.empty() && tz.back() == '\r') tz.pop_back();
        
        currentClock.timezone = tz;
    } else {
        // Fallback to UTC if we can't determine the timezone
        currentClock.timezone = "UTC";
    }
    return currentClock;
}

void ClockSettings::set(const Clock& targetClock) {
    // Update timezone
    SysCalls::execute("rm -f /etc/localtime");
    std::string tzCmd = "ln -s /usr/share/zoneinfo/" + targetClock.timezone + " /etc/localtime";
    SysCalls::execute(tzCmd);

    // Update date/time
    std::ostringstream dateStream;
    dateStream << std::setfill('0')
               << std::setw(4) << targetClock.year << "-"
               << std::setw(2) << targetClock.month << "-"
               << std::setw(2) << targetClock.day << " "
               << std::setw(2) << targetClock.hour << ":"
               << std::setw(2) << targetClock.minute << ":00";
    std::string dateCmd = "date -s '" + dateStream.str() + "'";
    
    // Set system time and only if that was successful, sync hardware clock
    if (SysCalls::execute(dateCmd)) {
        SysCalls::execute("hwclock --set --date=\"now\"");
    }
}

bool ClockSettings::synchronize() {
    // Simply restart the NTP service to synchronize time with configured NTP servers
    bool success = SysCalls::execute("/etc/init.d/S49ntp restart");
    // If the restart was successful, also sync the hardware clock
    if (success) {
        SysCalls::execute("hwclock --set --date=\"now\"");
    }
    return success;
}
