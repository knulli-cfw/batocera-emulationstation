#pragma once
#ifndef ES_CORE_COMPONENTS_IMAGE_ASPECT_CHECK_H
#define ES_CORE_COMPONENTS_IMAGE_ASPECT_CHECK_H

#include "GuiComponent.h"
#include "Log.h"
#include "math/Transform4x4f.h"
#include "math/Vector2i.h"
#include "renderers/Renderer.h"

#include <cmath>
#include <cstdlib>
#include <set>
#include <string>

// A check for render tests, on only with the environment variable ES_IMAGE_ASPECT: an image (or a video frame)
// drawn at another aspect ratio than the part of its texture it shows is logged once per element, source and
// drawn size, as "ImageAspect: ...". A theme stretches a picture only where it means to (a TV game's capture
// shown at its screen's 4:3, say); the test fails on any other.
namespace ImageAspectCheck
{
	inline bool enabled()
	{
		static const bool on = getenv("ES_IMAGE_ASPECT") != nullptr;
		return on;
	}

	// v: the quad's four vertices (0 top left, 1 bottom left, 2 top right, 3 bottom right, as ES lays them out)
	inline void check(const GuiComponent* comp, const std::string& source, const Renderer::Vertex* v, const Vector2i& texture,
		const Transform4x4f& trans)
	{
		if (!enabled() || comp == nullptr || v == nullptr || texture.x() <= 0 || texture.y() <= 0)
			return;

		float dw = std::fabs(v[2].pos.x() - v[0].pos.x()) * std::fabs(trans.r0().x());
		float dh = std::fabs(v[1].pos.y() - v[0].pos.y()) * std::fabs(trans.r1().y());
		float tw = std::fabs(v[2].tex.x() - v[0].tex.x()) * texture.x();
		float th = std::fabs(v[1].tex.y() - v[0].tex.y()) * texture.y();
		if (dw < 1.0f || dh < 1.0f || tw <= 0.0f || th <= 0.0f)
			return;

		// The width the drawn height asks for at the texture's ratio, within a pixel and a half or 3 %
		float expected = dh * tw / th;
		if (std::fabs(dw - expected) <= std::max(1.5f, 0.03f * dw))
			return;

		static std::set<std::string> logged;
		auto parent = comp->getParent();
		std::string key = comp->getTag() + "|" + source + "|" + std::to_string((int)std::round(dw)) + "x" + std::to_string((int)std::round(dh));
		if (!logged.insert(key).second)
			return;

		LOG(LogError) << "ImageAspect: " << (comp->getTag().empty() ? "?" : comp->getTag()) << " in " << (parent == nullptr || parent->getTag().empty() ? "?" : parent->getTag())
			<< " | " << source << " | texture " << (int)std::round(tw) << "x" << (int)std::round(th) << " drawn " << (int)std::round(dw) << "x" << (int)std::round(dh);
	}
}

#endif // ES_CORE_COMPONENTS_IMAGE_ASPECT_CHECK_H
