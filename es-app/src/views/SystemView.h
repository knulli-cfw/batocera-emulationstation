#pragma once
#ifndef ES_APP_VIEWS_SYSTEM_VIEW_H
#define ES_APP_VIEWS_SYSTEM_VIEW_H

#include "components/IList.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "GuiComponent.h"
#include "MultiStateInput.h"

#include "components/CarouselComponent.h"
#include "components/TextListComponent.h"
#include "components/ImageGridComponent.h"

#include <memory>
#include <functional>
#include <map>
#include <unordered_set>

#include "SystemViewControlWrapper.h"
#include "views/ColumnLayout.h"

class AnimatedImageComponent;
class SystemData;
class VideoVlcComponent;

struct SystemViewData
{
	SystemData* object;
	std::vector<GuiComponent*> backgroundExtras;
};

// A tab of the system grid (theme variable "systemtabs"): its name and the systems it shows, in order
struct SystemViewTab
{
	std::string name;
	std::vector<SystemData*> systems;
};


class SystemView : public GuiComponent
{
public:
	SystemView(Window* window);
	~SystemView();

	void goToSystem(SystemData* system, bool animate);

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);
	void reloadTheme(SystemData* system);

	SystemData* getActiveSystem();

	// GuiComponent overrides
	virtual void onShow() override;
	virtual void onHide() override;
	virtual void onScreenSaverActivate() override;
	virtual void onScreenSaverDeactivate() override;
	virtual void topWindow(bool isTop) override;

	virtual bool input(InputConfig* config, Input input) override;
	virtual void update(int deltaTime) override;
	virtual void render(const Transform4x4f& parentTrans) override;

	// Help
	std::vector<HelpPrompt> getHelpPrompts() override;
	virtual HelpStyle getHelpStyle() override;

	// Mouse support
	virtual bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	virtual void onMouseMove(int x, int y) override;
	virtual bool onMouseWheel(int delta) override;
	virtual bool onMouseClick(int button, bool pressed, int x, int y) override;

	virtual bool onAction(const std::string& action) override;

	int getCursorIndex();
	std::vector<SystemData*> getObjects();

	// The index of the tab that shows a system, in the theme's "systemtabs" order; -1 without tabs
	static int getSystemTab(SystemData* system);

	// Binds the selected system's extras again (its counts or last game changed)
	void refreshBindings() { updateExtraTextBinding(); }

	// Columns (ColumnLayout, a tabbed grid only): level 1, the first column (the tabs) has the focus; level 2,
	// the grid. The view pans between them; a game list opens beside level 2.
	bool	hasColumns();
	int		getColumnLevel() { return mColumnLevel; }
	void	setColumnLevel(int level, bool animate);
	// L1 or R1 from a game list: the previous or next tab
	void	changeTab(int direction) { if (hasTabs()) moveTab(direction); }
	const ColumnLayout& getColumnLayout() { return mColumns; }
	// The opacity of the header and the shared column while a game list slides in or out (1 at rest)
	void	setColumnFade(float fade) { mColumnFade = fade; }
	// The level the theme shows, {global:homeColumn}: 0 without columns
	static int getShownColumn() { return sShownColumn; }

protected:
	//virtual void onCursorChanged(const CursorState& state) override;
	void onCursorChanged(const CursorState& state);
	SystemData* getSelected();

private:
	void	 loadExtras(SystemData* system);
	std::vector<GuiComponent*> makeExtras(SystemData* system, bool preload);
	void	 prepareOtherTabs();
	void	 ensureTexture(GuiComponent* extra, bool reload);

	void	 updateExtraTextBinding();
	void	 showQuickSearch();

	void	 preloadExtraNeighbours(int cursor);
	void	 setExtraRequired(SystemViewData& data, bool required);

	void	 activateExtras(int cursor, bool activate = true);	
	void	 updateExtras(const std::function<void(GuiComponent*)>& func);
	// keepExtras (a tab switch): the systems' extras are kept for their next show (mParkedExtras) instead of
	// made again from the theme
	void	 clearEntries(bool keepExtras = false);
	void	 clearParkedExtras();

	int		 moveCursorFast(bool forward = true);

	void	 showNetplay();
	bool	 showNavigationBar();
	void	 showNavigationBar(const std::string& title, const std::function<std::string(SystemData* system)>& selector);

	void	 populate(bool keepExtras = false);
	void	 getViewElements(const std::shared_ptr<ThemeData>& theme);
	void	 getDefaultElements(void);
	void	 getCarouselFromTheme(const ThemeData::ThemeElement* elem);

	bool	 hasTabs();
	bool	 buildTabs();
	void	 applyTabGrid();
	void	 moveTab(int direction, bool wrap = true);

	void	 showColumnLevel(int level);
	// The extras' videos play at level 2 only: at level 1 the system's column is off the screen
	void	 showColumnVideos();
	void	 renderExtra(GuiComponent* extra, const Transform4x4f& trans);
	void	 renderInPlace(const Transform4x4f& screen, ColumnLayout::Role role);
	ColumnLayout::Role columnRole(GuiComponent* extra);
	bool	 onContinueVideoEnded(SystemData* system);

	void	 renderCarousel(const Transform4x4f& parentTrans);
	void	 renderExtras(const Transform4x4f& parentTrans, float lower, float upper);
	void	 renderInfoBar(const Transform4x4f& trans);
	
	ControlWrapper						mCarousel;

	TextComponent						mSystemInfo;

	std::vector<GuiComponent*>			mStaticBackgrounds;
	std::vector<SystemViewData>			mEntries;

	float			mCamOffset;
	float			mExtrasCamOffset;
	float			mExtrasFadeOpacity;
	float			mExtrasFadeMove;
	int				mExtrasFadeOldCursor;

	bool			mViewNeedsReload;		
	bool			mDisable;
	bool			mScreensaverActive;

	MultiStateInput mYButton;

	int				mLastCursor;

	// Mouse support
	int				mPressedCursor;
	Vector2i		mPressedPoint;
	bool			mLockCamOffsetChanges;
	bool			mLockExtraChanges;


	float			mSystemInfoDelay;
	bool			mSystemInfoCountOnly;

	std::string		mExtraTransitionType;
	float			mExtraTransitionSpeed;
	bool			mExtraTransitionHorizontal;

	// Tabs: the theme's definition, the tabs built from it, the shown one and each tab's last system
	std::shared_ptr<ThemeData>			mGridTheme;
	std::string							mGridElement;
	std::string							mTabDefinition;
	std::vector<SystemViewTab>			mTabs;
	int									mActiveTab;
	std::map<int, std::string>			mTabCursors;
	// The panel's suggestion: the system whose video ended (update moves on), the game shown and since when,
	// and the slides version each card was bound with
	SystemData*							mSuggestionEnded;
	SystemData*							mShownSystem;
	std::string							mShownContinue;
	unsigned int						mContinueShownAt;
	std::map<SystemData*, int>			mSlidesBound;
	std::map<SystemData*, std::vector<GuiComponent*>> mParkedExtras;

	static std::map<std::string, int>	sSystemTabs;

	ColumnLayout						mColumns;
	int									mColumnLevel;
	float								mColumnPan;		// drawn, in pixels
	float								mColumnPanFrom;
	float								mColumnPanTo;
	int									mColumnPanTime;	// ms into the pan, -1 at rest
	float								mColumnFade;
	static int							sShownColumn;
};

#endif // ES_APP_VIEWS_SYSTEM_VIEW_H
