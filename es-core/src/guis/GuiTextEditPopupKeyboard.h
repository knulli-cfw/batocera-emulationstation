#include "GuiComponent.h"

#include "components/NinePatchComponent.h"
#include "components/ButtonComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextEditComponent.h"
#include "components/TextComponent.h"
#include <functional>
#include "utils/WordCompletion.h"

class GuiTextEditPopupKeyboard : public GuiComponent
{
public:
	GuiTextEditPopupKeyboard(Window* window, const std::string& title, const std::string& initValue,
		const std::function<void(const std::string&)>& okCallback, bool multiLine, const std::string acceptBtnText = "OK",
		const std::shared_ptr<ThemeData>& searchTheme = nullptr, const std::string& scope = "");
	static void openSearch(Window* window, const std::shared_ptr<ThemeData>& theme, const std::string& scope,
		const std::string& title, const std::string& value, const std::function<void(const std::string&)>& apply);
	void enableWords(WordCompletion vocabulary);
	void update(int deltaTime) override;

	bool input(InputConfig* config, Input input);
	void onSizeChanged();
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	class KeyboardButton
	{
	public:
		std::shared_ptr<ButtonComponent> button;
		const std::string key;
		const std::string shiftedKey;
		const std::string altedKey;
		const std::string altedShiftedKey;
		KeyboardButton(const std::shared_ptr<ButtonComponent> b, const std::string& k, const std::string& sk, const std::string& ak, const std::string& ask) : button(b), key(k), shiftedKey(sk), altedKey(ak), altedShiftedKey(ask) {};
	};
	
	std::shared_ptr<ButtonComponent> makeButton(const std::string& key, const std::string& shiftedKey, const std::string& altedKey, const std::string& altedShiftedKey);
	std::vector<KeyboardButton> keyboardButtons;
	
	std::shared_ptr<ButtonComponent> mShiftButton;	
	std::shared_ptr<ButtonComponent> mAltButton;

	void styleKey(const std::shared_ptr<ButtonComponent>& button, const std::string& key);
	void setKeyLabel(const std::shared_ptr<ButtonComponent>& button, const std::string& key);

	void toggleKeyState(bool& state, std::shared_ptr<ButtonComponent>& button);
	void updateKeyboardButtons();
	void shiftKeys();
	void altKeys();

	NinePatchComponent mBackground;
	ComponentGrid mGrid;

	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextEditComponent> mText;
	std::shared_ptr<ComponentGrid> mKeyboardGrid;
	
	std::function<void(const std::string&)> mOkCallback;

	std::shared_ptr<ThemeData> mSearchTheme;
	std::shared_ptr<TextComponent> mScope, mHint, mShortcuts;
	static constexpr int suggestionLimit = 3;
	std::unique_ptr<WordCompletion> mVocabulary;
	WordCompletion::Choices mChoices;
	std::shared_ptr<TextComponent> mMatchHeader, mWordLabels[suggestionLimit];
	std::shared_ptr<NinePatchComponent> mWordCards[suggestionLimit];
	std::string mLastQuery;
	size_t mLastCursor = 0;
	int mSelectedMatch = -1;
	bool mPickPressed = false, mQuickPressed = false;
	void acceptWord(size_t index);
	void refreshMatches();
	void styleMatches();
	void focusMatch(int row);
	bool matchInput(InputConfig* config, Input input);
	bool mMultiLine;
	bool mShift = false;	
	bool mAlt = false;
};

