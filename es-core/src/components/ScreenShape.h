#pragma once
#ifndef ES_CORE_COMPONENTS_SCREEN_SHAPE_H
#define ES_CORE_COMPONENTS_SCREEN_SHAPE_H

#include "math/Vector2f.h"

// A picture stretched to a screen's shape (a TV game's capture at 4:3) keeps its orientation: one taller than
// wide in a landscape box (an upright arcade game) gets the box's shape turned upright, inside it and as tall,
// and the other way round. Pictures close to square, or turned the same way as the box, take the box.
namespace ScreenShape
{
	inline Vector2f fit(const Vector2f& box, const Vector2f& picture)
	{
		if (box.x() <= 0 || box.y() <= 0 || picture.x() <= 0 || picture.y() <= 0)
			return box;

		bool boxWide = box.x() > box.y() * 1.05f;
		bool boxTall = box.y() > box.x() * 1.05f;
		bool picWide = picture.x() > picture.y() * 1.05f;
		bool picTall = picture.y() > picture.x() * 1.05f;

		if (boxWide && picTall)
			return Vector2f(box.y() * box.y() / box.x(), box.y());

		if (boxTall && picWide)
			return Vector2f(box.x(), box.x() * box.x() / box.y());

		return box;
	}
}

#endif // ES_CORE_COMPONENTS_SCREEN_SHAPE_H
