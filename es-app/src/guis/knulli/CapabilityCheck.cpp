#include "guis/knulli/CapabilityCheck.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>
#include <fstream>
#include <string>

const std::string CapabilityCheck::RGB_CAPABILITY = "rgb";
const std::string CapabilityCheck::HDMI_CAPABILITY = "hdmi";
const std::string CapabilityCheck::LID_CAPABILITY = "lid";
const std::string CapabilityCheck::WIFI_CAPABILITY = "wifi";
const std::string CapabilityCheck::BLUETOOTH_CAPABILITY = "bluetooth";
const std::string CapabilityCheck::ANALOG_STICK_CAPABILITY = "analogstick";
const std::string CapabilityCheck::RUMBLE_CAPABILITY = "rumble";
const std::string CapabilityCheck::ADB_CAPABILITY = "adb";
const std::string CapabilityCheck::MTP_CAPABILITY = "mtp";
const std::string CapabilityCheck::BEZEL_PROJECT_CAPABILITY = "bezelproject";

const std::string CAPABILITY_CHECK_COMMAND_NAME = "/usr/bin/knulli-board-capability";
const std::string SEPARATOR = " ";

bool CapabilityCheck::hasCapability(const std::string capability)
{
	int result = system((CAPABILITY_CHECK_COMMAND_NAME
		+ SEPARATOR + capability
		).c_str());
	return WEXITSTATUS(result) == 0;
}