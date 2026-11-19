#include "views/gamelist/IGameListView.h"

#include "guis/GuiGamelistOptions.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "Sound.h"
#include "Window.h"

void IGameListView::setTheme(const std::shared_ptr<ThemeData>& theme)
{
	mTheme = theme;
	mColumns.read(theme);
	onThemeChanged(theme);
}

HelpStyle IGameListView::getHelpStyle()
{
	HelpStyle style;
	style.applyTheme(mTheme, getName());
	return style;
}

void IGameListView::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	// Columns: while the view slides beside the home, its background and header stay in place and fade with its
	// first column (the shared one); the rest slides
	if (!mColumns.hasSlide() || !ViewController::get()->isColumnSliding())
	{
		Renderer::pushClipRect(rect);
		renderChildren(trans);
		Renderer::popClipRect();
		return;
	}

	Transform4x4f screen = ViewController::get()->getScreenTransform();
	float shared = Renderer::getScreenWidth() - mColumns.slide;
	unsigned char faded = ColumnLayout::opacityOf(ColumnLayout::SHARED, mColumnFade, true);

	for (auto child : mChildren)
		if (child->isVisible() && mColumns.roleOf(child, 0, shared) == ColumnLayout::BACKGROUND)
			ColumnLayout::renderAt(child, screen, faded);

	Renderer::pushClipRect(rect);

	for (auto child : mChildren)
	{
		if (!child->isVisible())
			continue;

		auto role = mColumns.roleOf(child, 0, shared);
		if (role == ColumnLayout::SLIDES)
			child->render(trans);
		else if (role == ColumnLayout::SHARED)
			ColumnLayout::renderAt(child, trans, faded);
	}

	Renderer::popClipRect();

	for (auto child : mChildren)
		if (child->isVisible() && mColumns.roleOf(child, 0, shared) == ColumnLayout::HEADER)
			ColumnLayout::renderAt(child, screen, faded);
}

void IGameListView::setThemeName(std::string name)
{
	mCustomThemeName = name;
}

bool IGameListView::hasFileDataEntry(FileData* file)
{
	std::string path = file->getPath();

	for (auto item : getFileDataEntries())
		if (item->getPath() == path)
			return true;

	return false;
}