#include "GuiRetroAchievementsSettings.h"
#include "ThreadedHasher.h"
#include "GuiHashStart.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "RetroAchievements.h"

#include "guis/GuiMsgBox.h"
#include "components/SwitchComponent.h"
#include "components/OptionListComponent.h"
#include "components/TextComponent.h"
#include "RAOfflineProxy.h"

GuiRetroAchievementsSettings::GuiRetroAchievementsSettings(Window* window, bool focusProxyRow) : GuiSettings(window, _("RETROACHIEVEMENT SETTINGS").c_str())
{
	addGroup(_("SETTINGS"));

	bool retroachievementsEnabled = SystemConf::getInstance()->getBool("global.retroachievements");
	std::string username = SystemConf::getInstance()->get("global.retroachievements.username");
	std::string password = SystemConf::getInstance()->get("global.retroachievements.password");
	std::string webApiKey = SystemConf::getInstance()->get("global.retroachievements.webapikey");

	// retroachievements_enable
	auto retroachievements_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_enabled->setState(retroachievementsEnabled);
	addWithLabel(_("RETROACHIEVEMENTS"), retroachievements_enabled);
	
	// retroachievements, username, password
	addInputTextRow(_("USERNAME"), "global.retroachievements.username", false);
	addInputTextRow(_("PASSWORD"), "global.retroachievements.password", true);
	addInputTextRow(_("API KEY"), "global.retroachievements.webapikey", true);

	addGroup(_("OPTIONS"));

	bool raopActive = RAOfflineProxy::isServiceActive();
	bool proxyEnabled = SystemConf::getInstance()->getBool("global.retroachievements.proxy", true);
	bool proxyRouting = raopActive && proxyEnabled;

	if (raopActive)
	{
		auto raop_proxy = std::make_shared<SwitchComponent>(mWindow);
		raop_proxy->setState(proxyEnabled);
		addWithDescription(_("USE OFFLINE PROXY"), _("Route RetroAchievements through RAOfflineProxy for offline play and casual unlocks. Turn off to connect directly."), raop_proxy, focusProxyRow);
		addSaveFunc([raop_proxy]
		{
			SystemConf::getInstance()->setBool("global.retroachievements.proxy", raop_proxy->getState());
		});

		// rebuild the screen on toggle so the hardcore row updates instantly;
		// deferred to the next UI tick because close() deletes the switch that
		// is still handling this input event
		raop_proxy->setOnChangedCallback([this, window, comp = raop_proxy.get()]
		{
			SystemConf::getInstance()->setBool("global.retroachievements.proxy", comp->getState());
			window->postToUiThread([this, window]
			{
				window->pushGui(new GuiRetroAchievementsSettings(window, true));
				close();
			});
		});
	}

	if (proxyRouting)
	{
		// The offline proxy is casual-only: hardcore is force-disabled while it routes.
		auto theme = ThemeData::getMenuTheme();
		auto hardcore_locked = std::make_shared<TextComponent>(mWindow, _U("\uF023  ") + _("DISABLED"), theme->Text.font, theme->Text.color);
		hardcore_locked->setOpacity(110);
		addWithDescription(_("HARDCORE MODE"), _("Not available while the offline proxy is active (casual mode only)."), hardcore_locked);
		addSaveFunc([] { SystemConf::getInstance()->set("global.retroachievements.hardcore", "0"); });
	}
	else
		addSwitch(_("HARDCORE MODE"), _("Disable loading states, rewind and cheats for more points."), "global.retroachievements.hardcore", false, nullptr);

	addSwitch(_("LEADERBOARDS"), _("Compete in high-score and best time leaderboards (requires hardcore)."), "global.retroachievements.leaderboards", false, nullptr);
	addSwitch(_("VERBOSE MODE"), _("Show achievement progression on game launch and other notifications."), "global.retroachievements.verbose", false, nullptr);
	addSwitch(_("RICH PRESENCE"), "global.retroachievements.richpresence", false);
	addSwitch(_("ENCORE MODE"), _("Unlocked achievements can be earned again."), "global.retroachievements.encore", false, nullptr);
	addSwitch(_("AUTOMATIC SCREENSHOT"), _("Automatically take a screenshot when an achievement is earned."), "global.retroachievements.screenshot", false, nullptr);
	addSwitch(_("CHALLENGE INDICATORS"), _("Shows icons in the bottom right corner when eligible achievements can be earned."), "global.retroachievements.challenge_indicators", false, nullptr);

	// Unlock sound
	auto installedRSounds = ApiSystem::getInstance()->getRetroachievementsSoundsList();
	if (installedRSounds.size() > 0)
	{
		std::string currentSound = SystemConf::getInstance()->get("global.retroachievements.sound");

		auto rsounds_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("RETROACHIEVEMENT UNLOCK SOUND"), false);
		rsounds_choices->add(_("none"), "none", currentSound.empty() || currentSound == "none");

		for (auto snd : installedRSounds)
			rsounds_choices->add(_(Utils::String::toUpper(snd).c_str()), snd, currentSound == snd);

		if (!rsounds_choices->hasSelection())
			rsounds_choices->selectFirstItem();

		addWithLabel(_("UNLOCK SOUND"), rsounds_choices);
		addSaveFunc([rsounds_choices] { SystemConf::getInstance()->set("global.retroachievements.sound", rsounds_choices->getSelected()); });
	}

	// retroachievements_hardcore_mode
	addSwitch(_("SHOW RETROACHIEVEMENTS ENTRY IN MAIN MENU"), _("View your RetroAchievement stats right from the main menu!"), "RetroachievementsMenuitem", true, nullptr);

	addGroup(_("GAME INDEXES"));
	addSwitch(_("INDEX NEW GAMES AT STARTUP"), "CheevosCheckIndexesAtStart", true);
	addEntry(_("INDEX GAMES"), true, [this]
	{
		if (ThreadedHasher::checkCloseIfRunning(mWindow))
			mWindow->pushGui(new GuiHashStart(mWindow, ThreadedHasher::HASH_CHEEVOS_MD5));
	});

	addSaveFunc([retroachievementsEnabled, retroachievements_enabled, username, password, window]
	{
		bool newState = retroachievements_enabled->getState();
		std::string newUsername = SystemConf::getInstance()->get("global.retroachievements.username");
		std::string newPassword = SystemConf::getInstance()->get("global.retroachievements.password");
		std::string newWebApiKey = SystemConf::getInstance()->get("global.retroachievements.webapikey");
		std::string token = SystemConf::getInstance()->get("global.retroachievements.token");

		if (newState && (!retroachievementsEnabled || username != newUsername || password != newPassword || token.empty()))
		{
			std::string tokenOrError;
			if (RetroAchievements::testAccount(newUsername, newPassword, tokenOrError))
			{
				SystemConf::getInstance()->set("global.retroachievements.token", tokenOrError);
			}
			else
			{
				SystemConf::getInstance()->set("global.retroachievements.token", "");

				window->pushGui(new GuiMsgBox(window, _("UNABLE TO ACTIVATE RETROACHIEVEMENTS:") + "\n" + tokenOrError, _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
				retroachievements_enabled->setState(false);
				newState = false;
			}
		}
		else if (!newState)
			SystemConf::getInstance()->set("global.retroachievements.token", "");

		if (SystemConf::getInstance()->setBool("global.retroachievements", newState))
			if (!ThreadedHasher::isRunning() && newState)
				ThreadedHasher::start(window, ThreadedHasher::HASH_CHEEVOS_MD5, false, true);
	});
}
