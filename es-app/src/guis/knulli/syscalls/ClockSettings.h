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
        std::string timezone; // z.B. "Europe/Berlin" oder "UTC"
    };

    static Clock get();
    static void set(const Clock& targetClock);
    static bool synchronize();
};