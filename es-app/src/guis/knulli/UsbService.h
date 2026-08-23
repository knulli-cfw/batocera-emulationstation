#pragma once
#include "utils/StringUtil.h"

class UsbService
{
public:
        static bool hasService();
        static void start();
        static void stop();
        static void restart();

private:
        static void call(std::string argument);
};
