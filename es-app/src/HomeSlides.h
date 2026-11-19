#pragma once
#ifndef ES_APP_HOME_SLIDES_H
#define ES_APP_HOME_SLIDES_H

#include <random>
#include <set>
#include <string>
#include <vector>

// The tabbed home's slides: the captures a system card slides through and the games its panel suggests.
// Free of ES types so it can be tested alone.
namespace HomeSlides
{
	// The theme variable "systemslides": one game per line, "+" for the capture a card shows first, "-" for
	// one it leaves out (a title screen, a menu, a duplicate), "~" for a video the panel never suggests (one
	// in black and white on a colour console), then the game's own system, a space and its path as the
	// gamelist writes it. "%", "=", "<", ">" and "$" in a path are written as %25, %3D, %3C, %3E
	// and %24 (a theme variable with "=" or "<" is read as an expression):
	//   +snes ./Some Game.sfc
	//   -snes ./Some Title Screen.sfc
	struct Rules
	{
		std::set<std::string> representatives;
		std::set<std::string> excluded;
		std::set<std::string> noVideo;
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

	// The panel's pool, as ids: the favorites with a video; none when there are favorites but none has one
	// (the panel then shows the best-rated favorite); only a system with no favorite at all uses its best-rated
	// games with a video (at most fallbackCount). A "~" video counts as none; "-" is about captures, so it
	// does not apply.
	std::vector<std::string> pickPool(std::vector<Candidate> candidates, const Rules& rules, size_t fallbackCount);

	// A utility's items for the home's panel (tools and emulators by last run, screenshots by file date):
	// the newest first, then the never dated by name; at most count ids
	struct Dated
	{
		std::string id;
		std::string name;
		std::string date;	// sortable (as a gamelist's "lastplayed"); empty when never
	};

	std::vector<std::string> newestFirst(std::vector<Dated> items, size_t count);

	// Items in a random order, each shown once before any repeats; a new round never starts with the item
	// shown last. A reset (the list made again after a game changed) keeps the current item while it is still
	// in the list, so the panel does not jump to another game; without one, it starts on prefer when that is in it.
	class Deck
	{
	public:
		Deck() : mIndex(0) { }

		void reset(const std::vector<std::string>& items, std::mt19937& rng, const std::string& prefer = "");
		bool empty() const { return mItems.empty(); }
		size_t size() const { return mItems.size(); }
		const std::string& current() const;

		// Moves to the next item; false (and stays) when there is no other one
		bool next(std::mt19937& rng);

	private:
		std::vector<std::string> mItems;
		size_t mIndex;
	};
}

#endif // ES_APP_HOME_SLIDES_H
