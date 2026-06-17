#pragma once
#include <string>

class ClockSettings {
public:
    struct Clock {
        int year;
        int month;
        int day;
        int hour;
        int minute;
    };

    static Clock get();
    static void set(const Clock& targetClock);
    static std::string getTimezone();
    static std::string setTimezone(const std::string& timezone);
    static bool synchronize();
};