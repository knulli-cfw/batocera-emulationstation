#pragma once

#include "GuiComponent.h"
#include "MultiStateInput.h"
#include "Window.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include <set>

class FileData;
struct SaveState;

struct SaveStatePreview
{
	std::string screenshotPath;
	std::string label;       // "AUTO SAVE - Jan 15 2026" or "SLOT 2 - Jan 10 2026"
	int slot;                // -1 = auto-save, >= 0 = numbered
	SaveState* saveState;    // Non-null in normal mode, nullptr in cached mode
};

class GuiGameSwitcher : public GuiComponent
{
public:
	GuiGameSwitcher(Window* window, bool fromCache = false);
	~GuiGameSwitcher();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& transform) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

	// Static methods for pending Game Switcher (triggered while game is running)
	static void setPendingGameSwitcher(bool pending);
	static bool hasPendingGameSwitcher();
	static void showPendingGameSwitcher(Window* window);
	static bool isActive();

	// Cached mode (lightweight Game Switcher before full ES loads)
	static bool runCachedMode();

	// Cache management
	static std::string getCachePath();
	static void saveCache(FileData* gameBeingLaunched = nullptr);
	static bool hasCachedData();

	// Exclusion list management
	static std::string getExclusionPath();
	static std::vector<std::string> loadExclusions();
	static void saveExclusions(const std::vector<std::string>& exclusions);
	static void addExclusion(const std::string& gamePath);
	static void clearExclusions();
	static bool isExcluded(const std::string& gamePath);

	// Inclusion list management
	static std::string getInclusionPath();
	static std::vector<std::string> loadInclusions();
	static void saveInclusions(const std::vector<std::string>& inclusions);
	static void addInclusion(const std::string& gamePath);
	static void removeInclusion(const std::string& gamePath);
	static void clearInclusions();
	static bool isIncluded(const std::string& gamePath);

	// Pending stats for games launched without full ES (Quick Resume / cached mode)
	static void savePendingStats(const std::string& gamePath, const std::string& systemName, int elapsedSeconds);
	static void applyPendingStats();  // Called when ES fully loads

	// Settings UI (called from GuiMenu)
	static void openSettings(Window* window, bool selectMarqueeEnable = false, bool selectPlayInfoEnable = false);

private:
	static std::string getPendingStatsPath();
	struct GameItem
	{
		FileData* game;           // nullptr in cached mode
		std::string screenshotPath;

		// Cached data (used when game is nullptr)
		std::string gamePath;
		std::string gameName;
		std::string marqueePath;
		std::string launchCommand;
		std::string systemName;
		int playCount;
		int gameTime;
		bool included;

		// Save state preview data
		std::vector<SaveStatePreview> saveStates;
		int currentSaveStateIndex;  // -1 = default view (no specific save state selected)
	};

	std::vector<GameItem> mGames;
	int mCurrentIndex;
	bool mCachedMode;

	// Current visual components
	ImageComponent* mScreenshot;
	ImageComponent* mMarquee;
	TextComponent*  mGameName;      // Fallback when no marquee
	TextComponent*  mPlayInfo;      // "Played X times | Play time: Xh Xm"

	// Save state label (shown when browsing save states with up/down)
	TextComponent*  mSaveStateLabel;
	TextComponent*  mPrevSaveStateLabel;

	// Save state indicator (◉, top-left — shown when viewing a save state)
	TextComponent*  mSaveStateIndicator;
	TextComponent*  mPrevSaveStateIndicator;

	// Included indicator (star, top-right — shown when game is pinned)
	TextComponent*  mIncludedIndicator;
	TextComponent*  mPrevIncludedIndicator;

	// Previous visual components (for animation)
	ImageComponent* mPrevScreenshot;
	ImageComponent* mPrevMarquee;
	TextComponent*  mPrevGameName;
	TextComponent*  mPrevPlayInfo;

	// Animation state
	bool  mAnimating;
	bool  mAnimatingVertical;    // True = vertical (save state), false = horizontal (game)
	bool  mLaunching;            // Fade-out before launch
	bool  mLaunchAfterNavigation; // Launch game after navigation animation completes
	float mAnimationProgress;    // 0.0 to 1.0
	int   mAnimationDirection;   // -1 = left/up, +1 = right/down
	bool  mFadeIndicator;        // True when save state indicator should fade (default ↔ save state)
	int   mAnimationDuration;    // milliseconds

	void loadRecentlyPlayedGames();
	void loadFromCache();
	static std::string getScreenshotForGame(FileData* game);
	void updateDisplay();
	void updateDisplayForComponents(ImageComponent* screenshot, ImageComponent* marquee,
	                                 TextComponent* gameName, TextComponent* playInfo,
	                                 int gameIndex);
	void navigateTo(int index);
	void navigateToSaveState(int newIndex, int direction);
	void launchCurrentGame();
	void removeCurrentGame();
	void toggleCurrentGameInclusion();

	MultiStateInput mXButton;
	MultiStateInput mYButton;
	MultiStateInput mAButton;

	// Cached screen dimensions (avoid per-frame Renderer calls)
	float mScreenWidth;
	float mScreenHeight;

	// Cached settings (avoid per-frame singleton lookups in render())
	unsigned char mCachedBgAlpha;
	unsigned int mCachedInfoBgColor;
	bool mCachedHelpEnabled;
	bool mCachedMarqueeEnabled;
	bool mCachedMarqueeFallback;
	bool mCachedPlayInfoEnabled;
	bool mCachedLaunchAnimEnabled;

	// Cached help bar background geometry (avoid per-frame getHelpStyle() calls)
	float mHelpBgY;
	float mHelpBgHeight;

	// Cached play info background dimensions (avoid per-frame sizeText() calls)
	float mPlayInfoBgW;
	float mPlayInfoBgH;
	float mPlayInfoBgY;
	float mPrevPlayInfoBgW;
	float mPrevPlayInfoBgH;
	float mPrevPlayInfoBgY;

	// Cached save state label background dimensions
	float mSaveStateLabelBgW;
	float mSaveStateLabelBgH;
	float mSaveStateLabelBgY;
	float mPrevSaveStateLabelBgW;
	float mPrevSaveStateLabelBgH;
	float mPrevSaveStateLabelBgY;

	// Cached exclusion list (avoid repeated file I/O in isExcluded())
	static std::set<std::string> sCachedExclusions;
	static bool sExclusionsLoaded;

	// Cached inclusion list (avoid repeated file I/O in isIncluded())
	static std::set<std::string> sCachedInclusions;
	static bool sInclusionsLoaded;

	static bool sPendingGameSwitcher;
	static GuiGameSwitcher* sActiveInstance;
};
