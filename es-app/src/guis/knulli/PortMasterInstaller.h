#pragma once

#include "Window.h"

class PortMasterInstaller
{
public:
        static void install(Window* window);
        static bool isInstalled();
        static bool hasInstaller();

};
