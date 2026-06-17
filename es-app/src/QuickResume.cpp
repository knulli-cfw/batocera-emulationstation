#include "QuickResume.h"
#include <SystemConf.h>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/Platform.h"
#include "InputManager.h"
#include "guis/GuiGameSwitcher.h"

#include <rapidjson/document.h>
#include <fstream>

namespace QuickResume
{
    const std::string shutdownFlag = "/var/run/shutdown.flag";

    bool setQuickResume(std::string quickResumeCommand, std::string quickResumePath)
    {
        if (!quickResumeEnabled())
            return false;

        if (quickResumeCommand.empty() || quickResumePath.empty())
            return false;

        SystemConf::getInstance()->set("global.bootgame.path", quickResumePath);
        SystemConf::getInstance()->set("global.bootgame.cmd", quickResumeCommand);
        return SystemConf::getInstance()->saveSystemConf();
    }

    bool clearQuickResume()
    {
        bool configSaved = false;

        if (quickResumeEnabled())
        {
            SystemConf::getInstance()->set("global.bootgame.path", "");
            SystemConf::getInstance()->set("global.bootgame.cmd", "");
            configSaved = SystemConf::getInstance()->saveSystemConf();
        }

        return configSaved;
    }

    bool postLaunchConditionalClean()
    {
        bool configSaved = false;

        if (quickResumeEnabled() && !shutDownInProgress())
        {
            configSaved = clearQuickResume();
        }

        return configSaved;
    }

    bool shutDownInProgress()
    {
        return Utils::FileSystem::exists(shutdownFlag);
    }

    bool quickResumeEnabled()
    {
        return SystemConf::getInstance()->getBool("global.quickresume") == true;
    }

    void launchStartupGame()
    {
        auto gamePath = SystemConf::getInstance()->get("global.bootgame.path");
        if (gamePath.empty() || !Utils::FileSystem::exists(gamePath))
            return;

        auto command = SystemConf::getInstance()->get("global.bootgame.cmd");
        if (!command.empty())
        {
            // Try to get system name from cache for stats tracking
            std::string systemName;
            std::string cachePath = GuiGameSwitcher::getCachePath();
            if (Utils::FileSystem::exists(cachePath))
            {
                std::ifstream file(cachePath);
                if (file.is_open())
                {
                    std::string content((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
                    file.close();

                    rapidjson::Document doc;
                    doc.Parse(content.c_str());
                    if (doc.IsArray())
                    {
                        for (auto& gameObj : doc.GetArray())
                        {
                            if (gameObj.HasMember("gamePath") && gameObj["gamePath"].IsString())
                            {
                                if (std::string(gameObj["gamePath"].GetString()) == gamePath)
                                {
                                    if (gameObj.HasMember("systemName") && gameObj["systemName"].IsString())
                                        systemName = gameObj["systemName"].GetString();
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            InputManager::getInstance()->init();
            command = Utils::String::replace(command, "%CONTROLLERSCONFIG%", InputManager::getInstance()->configureEmulators());

            // Track start time
            time_t startTime = time(NULL);

            Utils::Platform::ProcessStartInfo(command).run();

            // Calculate elapsed time and save pending stats
            time_t endTime = time(NULL);
            int elapsedSeconds = (int)difftime(endTime, startTime);

            if (!systemName.empty())
            {
                GuiGameSwitcher::savePendingStats(gamePath, systemName, elapsedSeconds);
            }

            postLaunchConditionalClean();
        }
    }
}