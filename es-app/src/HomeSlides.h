#pragma once
#ifndef ES_APP_HOME_SLIDES_H
#define ES_APP_HOME_SLIDES_H

#include <set>
#include <string>
#include <vector>

// The tabbed home's slides: the captures a system card slides through.
// Free of ES types so it can be tested alone.
namespace HomeSlides
{
	// The theme variable "systemslides": one game per line, "+" for the capture a card shows first, "-" for
	// one it leaves out (a title screen, a menu, a duplicate), then the game's own system, a space and its
	// path as the gamelist writes it. "%", "=", "<", ">" and "$" in a path are written as %25, %3D, %3C, %3E
	// and %24 (a theme variable with "=" or "<" is read as an expression):
	//   +snes ./Some Game.sfc
	//   -snes ./Some Title Screen.sfc
	struct Rules
	{
		std::set<std::string> representatives;
		std::set<std::string> excluded;
	};

	Rules parseRules(const std::string& definition);

	// The key of a game in Rules: its system, a space and its relative path
	std::string key(const std::string& system, const std::string& relativePath);

	struct Candidate
	{
		std::string key;	// as in Rules
		std::string id;		// what the caller finds the game by (its path)
		std::string name;
		std::string media;	// the capture or the video; empty when there is none
		float rating;		// 0 to 1
		bool favorite;
	};

	// A card's captures: its favorites with a capture, else its best-rated games with one (at most
	// fallbackCount); the excluded left out; the representatives first, then by rating and name.
	std::vector<std::string> orderSlides(std::vector<Candidate> candidates, const Rules& rules, size_t fallbackCount);
}

#endif // ES_APP_HOME_SLIDES_H
