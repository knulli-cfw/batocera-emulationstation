#pragma once
#ifndef ES_APP_VIEWS_COLUMN_LAYOUT_H
#define ES_APP_VIEWS_COLUMN_LAYOUT_H

#include "GuiComponent.h"
#include "ThemeData.h"
#include "renderers/Renderer.h"
#include "math/Misc.h"
#include "math/Transform4x4f.h"

#include <cstdlib>
#include <memory>
#include <string>

// A theme's column layout for the tabbed system grid and its game lists, from three theme variables, each a
// fraction of the screen (none or 0: no columns):
//   esColumnPan     the home's first column (the tabs, focused at level 1): the system view pans by its width
//                   when the grid takes the focus (level 2), and back
//   esColumnSlide   a game list opens this far to the right of the home at level 2 and both slide by it; what
//                   is left of the screen (the home's last column, the list's first) is shared by the two
//                   views: there they crossfade
//   esColumnHeader  the band at the top of both views that stays in place while they slide (and crossfades)
// An element's role comes from where the theme put it: a full-screen one is the background, one inside the
// header band stays in place, one that starts in the shared column crossfades, the rest slides.
struct ColumnLayout
{
	enum Role { SLIDES, BACKGROUND, HEADER, SHARED };

	float pan;
	float slide;
	float header;

	ColumnLayout() : pan(0), slide(0), header(0) { }

	void read(const std::shared_ptr<ThemeData>& theme)
	{
		pan = slide = header = 0;
		if (theme == nullptr)
			return;

		pan = fraction(theme, "esColumnPan") * Renderer::getScreenWidth();
		slide = fraction(theme, "esColumnSlide") * Renderer::getScreenWidth();
		header = fraction(theme, "esColumnHeader") * Renderer::getScreenHeight();

		if (slide <= 0 || slide >= Renderer::getScreenWidth())
			slide = 0;
	}

	bool hasSlide() const { return slide > 0; }

	// The role of an element of a view whose shared column runs from left to right (view coordinates, pixels)
	Role roleOf(GuiComponent* comp, float left, float right) const
	{
		Vector2f size = comp->getSize();
		Vector3f pos = comp->getPosition();
		Vector2f origin = comp->getOrigin();

		float x = pos.x() - origin.x() * size.x();
		float y = pos.y() - origin.y() * size.y();

		float w = (float)Renderer::getScreenWidth();
		float h = (float)Renderer::getScreenHeight();

		if (size.x() >= w * 0.9f && size.y() >= h * 0.9f)
			return BACKGROUND;

		if (header > 0 && y + size.y() <= header + 0.5f)
			return HEADER;

		// What starts in the shared column belongs to it (nothing else does)
		if (right > left && x >= left - 0.5f && x < right - 0.5f)
			return SHARED;

		return SLIDES;
	}

	// The opacity of an element of a role at a column fade (1 at rest)
	static unsigned char opacityOf(Role role, float fade, bool fadeBackground)
	{
		if (role == SLIDES || (role == BACKGROUND && !fadeBackground))
			return 255;

		return (unsigned char)(Math::clamp(fade, 0.0f, 1.0f) * 255.0f);
	}

	// Draws an element at an opacity, its own one kept
	static void renderAt(GuiComponent* comp, const Transform4x4f& trans, unsigned char opacity)
	{
		if (opacity == 0)
			return;

		if (opacity == 255)
		{
			comp->render(trans);
			return;
		}

		comp->setAmbientOpacity(opacity);
		comp->render(trans);
		comp->setAmbientOpacity(255);
	}

private:
	static float fraction(const std::shared_ptr<ThemeData>& theme, const std::string& name)
	{
		std::string value = theme->getVariable(name);
		if (value.empty())
			return 0;

		float f = (float)atof(value.c_str());
		return f > 0 && f < 1 ? f : 0;
	}
};

#endif // ES_APP_VIEWS_COLUMN_LAYOUT_H
