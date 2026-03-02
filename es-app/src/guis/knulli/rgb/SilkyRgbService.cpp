#include "guis/knulli/rgb/SilkyRgbService.h"
#include "utils/Platform.h"
#include "utils/StringUtil.h"
#include "Paths.h"
#include "HttpReq.h"
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>
#include <rapidjson/document.h>
#include "guis/knulli/syscalls/SysCalls.h"
#include <algorithm>
#include <string>
#include <cctype>
#include "Log.h"


const std::string DAEMON_NAME = "/etc/init.d/S25silky-rgb";

const std::string API_BASE_PATH = "http://localhost:1235/";
const std::string API_GET_SETTINGS = "get-settings";
const std::string API_GET_MODES = "get-modes";
const std::string API_GET_PALETTES = "get-palettes";
const std::string API_GET_COLORS = "get-colors";
const std::string API_RELOAD_CONFIG = "reload-config";
const std::string API_SET_CONFIG = "set-config";
const std::string API_UPDATE_SCREEN_STATE = "update-screen-state";

void SilkyRgbService::start()
{
	SysCalls::execute(DAEMON_NAME + " start");
}

void SilkyRgbService::stop()
{
	SysCalls::execute(DAEMON_NAME + " stop");
}

void SilkyRgbService::reloadConfig()
{
	HttpReq req(API_BASE_PATH + API_RELOAD_CONFIG);

	if (req.wait())
	{
		// TODO: Handle response if needed
	}
}

std::vector<std::string> SilkyRgbService::requiredSettings()
{
	std::vector<std::string> settings;

	HttpReq req(API_BASE_PATH + API_GET_SETTINGS);
	if (req.wait())
	{
		rapidjson::Document doc;
		doc.Parse(req.getContent().c_str());
		if (doc.HasParseError())
			return settings;

		if (doc.IsObject() == false)
			return settings;

		for (auto& member : doc.GetObject())
		{
			if (member.name.IsString())
			{
				settings.push_back(std::string(member.name.GetString()));
			}
		}
	}
	return settings;
}

std::vector<ModeInfo> SilkyRgbService::getAvailableModes()
{
	std::vector<ModeInfo> modes;
	HttpReq req(API_BASE_PATH + API_GET_MODES);
	if (req.wait())
	{
		rapidjson::Document doc;
		doc.Parse(req.getContent().c_str());
		if (doc.HasParseError())
			return modes;
		
		
		if (doc.IsObject() == false)
			return modes;

		for (auto& member : doc.GetObject())
		{

			if (member.name.IsString())
			{
				ModeInfo mode;
				mode.id = std::string(member.name.GetString());
				if (member.value.IsObject() && member.value.HasMember("name") && member.value["name"].IsString()) {
					mode.name = member.value["name"].GetString();
					modes.push_back(mode);
				}
			}
		}
		
	}
	return modes;
}

std::vector<ColorInfo> SilkyRgbService::getAvailableColors()
{
	std::vector<ColorInfo> colors;
	HttpReq req(API_BASE_PATH + API_GET_COLORS);
	if (req.wait())
	{
		rapidjson::Document doc;
		doc.Parse(req.getContent().c_str());
		if (doc.HasParseError())
			return colors;
		
		
		if (doc.IsObject() == false)
			return colors;

		for (auto& member : doc.GetObject())
		{

			if (member.name.IsString())
			{
				ColorInfo color;
				color.id = std::string(member.name.GetString());
				if (member.value.IsObject() && member.value.HasMember("name") && member.value["name"].IsString()) {
					color.name = member.value["name"].GetString();
					colors.push_back(color);
				}
			}
		}
		
	}
	return colors;
}

std::vector<PaletteInfo> SilkyRgbService::getAvailablePalettes()
{
	std::vector<PaletteInfo> palettes;
	HttpReq req(API_BASE_PATH + API_GET_PALETTES);
	if (req.wait())
	{
		rapidjson::Document doc;
		doc.Parse(req.getContent().c_str());
		if (doc.HasParseError())
			return palettes;
		
		
		if (doc.IsObject() == false)
			return palettes;

		for (auto& member : doc.GetObject())
		{

			if (member.name.IsString())
			{
				PaletteInfo palette;
				palette.id = std::string(member.name.GetString());
				if (member.value.IsObject() && member.value.HasMember("name") && member.value["name"].IsString()) {
					palette.name = member.value["name"].GetString();
					if (member.value.HasMember("color.primary") && member.value["color.primary"].IsString()) {
						palette.colorPrimary = member.value["color.primary"].GetString();
					}
					if (member.value.HasMember("color.secondary") && member.value["color.secondary"].IsString()) {
						palette.colorSecondary = member.value["color.secondary"].GetString();
					}
					palettes.push_back(palette);
				}
			}
		}
		
	}
	return palettes;
}

void SilkyRgbService::applyValue(std::string key, std::string value)
{
	HttpReqOptions options;
	options.dataToPost = key + " " + value;
	HttpReq req(API_BASE_PATH + API_SET_CONFIG, &options);

	if (req.wait())
	{
		// TODO: Handle response if needed
	}
}

void SilkyRgbService::updateScreenBrightness()
{
	std::string brightness = SysCalls::executeAndCatchOutput("knulli-brightness");
	brightness = Utils::String::trim(brightness);

	if (brightness.empty() || !SilkyRgbService::isNumeric(brightness) || std::stof(brightness) < 0 || std::stof(brightness) > 100)
	{
		LOG(LogError) << "SilkyRgbService: Failed to get screen brightness from knulli-brightness.";
		return;
	}

	HttpReqOptions options;
	options.dataToPost = brightness;
	HttpReq req(API_BASE_PATH + API_UPDATE_SCREEN_STATE, &options);

	if (req.wait())
	{
		// TODO: Handle response if needed
	}
}

bool SilkyRgbService::isNumeric(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isdigit(c);
    });
}