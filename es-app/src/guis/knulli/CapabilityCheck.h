#pragma once
#include "utils/StringUtil.h"

class CapabilityCheck
{
public:
        static const std::string RGB_CAPABILITY;
        static const std::string HDMI_CAPABILITY;
        static const std::string LID_CAPABILITY;
        static const std::string WIFI_CAPABILITY;
        static const std::string BLUETOOTH_CAPABILITY;
        static const std::string ANALOG_STICK_CAPABILITY;
        static const std::string RUMBLE_CAPABILITY;
        static const std::string ADB_CAPABILITY;
        static const std::string MTP_CAPABILITY;
        static const std::string BEZEL_PROJECT_CAPABILITY;

        static bool hasCapability(const std::string capability);

};
