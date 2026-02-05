#include "guis/knulli/rgb/SilkyRgbService.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "HttpReq.h"
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>
#include <rapidjson/document.h>
#include "guis/knulli/syscalls/SysCalls.h"

#include "Log.h"

const std::string API_BASE_PATH = "http://localhost:1235/";
const std::string API_GET_SETTINGS = "get-settings";
const std::string API_GET_MODES = "get-modes";
const std::string API_GET_PALETTES = "get-palettes";
const std::string API_RELOAD_CONFIG = "reload-config";
const std::string API_SET_CONFIG = "set-config";
const std::string API_UPDATE_SCREEN_STATE = "update-screen-state";

bool SilkyRgbService::isInstalled() {
	HttpReq* req = new HttpReq(API_BASE_PATH + API_GET_SETTINGS);
	return req->wait();
}

void SilkyRgbService::reloadConfig()
{
	HttpReq* req = new HttpReq(API_BASE_PATH + API_RELOAD_CONFIG);

	if (req->wait())
	{
		// TODO: Handle response if needed
	}
}

std::vector<std::string> SilkyRgbService::requiredSettings()
{
	std::vector<std::string> settings;

	HttpReq* req = new HttpReq(API_BASE_PATH + API_GET_SETTINGS);
	if (req->wait())
	{
		rapidjson::Document doc;
		doc.Parse(req->getContent().c_str());
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
	HttpReq* req = new HttpReq(API_BASE_PATH + API_GET_MODES);
	if (req->wait())
	{
		rapidjson::Document doc;
		doc.Parse(req->getContent().c_str());
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

std::vector<PaletteInfo> SilkyRgbService::getAvailablePalettes()
{
	std::vector<PaletteInfo> palettes;
	HttpReq* req = new HttpReq(API_BASE_PATH + API_GET_PALETTES);
	if (req->wait())
	{
		rapidjson::Document doc;
		doc.Parse(req->getContent().c_str());
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
	HttpReq* req = new HttpReq(API_BASE_PATH + API_SET_CONFIG, &options);

	if (req->wait())
	{
		// TODO: Handle response if needed
	}
}

void SilkyRgbService::updateScreenBrightness()
{

	std::string brightness = SysCalls::executeAndCatchOutput("knulli-brightness");

	if (brightness.empty() || !Utils::String::isNumeric(brightness) || std::stof(brightness) < 0 || std::stof(brightness) > 100)
	{
		LOG(LogError) << "SilkyRgbService: Failed to get screen brightness from knulli-brightness.";
		return;
	}

	HttpReqOptions options;
	options.dataToPost = brightness;
	HttpReq* req = new HttpReq(API_BASE_PATH + API_UPDATE_SCREEN_STATE, &options);

	if (req->wait())
	{
		// TODO: Handle response if needed
	}
}

