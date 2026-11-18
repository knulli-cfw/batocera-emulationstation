#include "HomeSlides.h"

#include <algorithm>

namespace HomeSlides
{
	static std::string trim(const std::string& text)
	{
		const char* blanks = " \t\r\n";
		size_t first = text.find_first_not_of(blanks);
		if (first == std::string::npos)
			return "";

		size_t last = text.find_last_not_of(blanks);
		return text.substr(first, last - first + 1);
	}

	static int hexValue(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	// %XX sequences back to their character; anything else is kept as written
	static std::string decode(const std::string& text)
	{
		std::string out;
		out.reserve(text.size());

		for (size_t i = 0; i < text.size(); i++)
		{
			if (text[i] == '%' && i + 2 < text.size())
			{
				int high = hexValue(text[i + 1]);
				int low = hexValue(text[i + 2]);
				if (high >= 0 && low >= 0)
				{
					out += (char)(high * 16 + low);
					i += 2;
					continue;
				}
			}

			out += text[i];
		}

		return out;
	}

	std::string key(const std::string& system, const std::string& relativePath)
	{
		return system + " " + relativePath;
	}

	Rules parseRules(const std::string& definition)
	{
		Rules rules;

		size_t start = 0;
		while (start <= definition.size())
		{
			size_t end = definition.find('\n', start);
			if (end == std::string::npos)
				end = definition.size();

			std::string line = trim(definition.substr(start, end - start));
			start = end + 1;

			if (line.size() < 4 || (line[0] != '+' && line[0] != '-'))
				continue;

			size_t space = line.find(' ');
			if (space == std::string::npos || space < 2)
				continue;

			std::string system = line.substr(1, space - 1);
			std::string path = decode(trim(line.substr(space + 1)));
			if (path.empty())
				continue;

			if (line[0] == '+')
				rules.representatives.insert(key(system, path));
			else
				rules.excluded.insert(key(system, path));
		}

		return rules;
	}

	// The best first; equal ratings by name, so the order does not depend on the gamelist's
	static bool byRating(const Candidate& a, const Candidate& b)
	{
		if (a.rating != b.rating)
			return a.rating > b.rating;

		return a.name < b.name;
	}

	// The favorites with media, else the best-rated games with media (at most fallbackCount), by rating
	static std::vector<Candidate> pool(std::vector<Candidate>& candidates, size_t fallbackCount)
	{
		std::vector<Candidate> withMedia;
		for (auto& candidate : candidates)
			if (!candidate.media.empty())
				withMedia.push_back(candidate);

		std::stable_sort(withMedia.begin(), withMedia.end(), byRating);

		std::vector<Candidate> favorites;
		for (auto& candidate : withMedia)
			if (candidate.favorite)
				favorites.push_back(candidate);

		if (!favorites.empty())
			return favorites;

		if (withMedia.size() > fallbackCount)
			withMedia.resize(fallbackCount);

		return withMedia;
	}

	std::vector<std::string> orderSlides(std::vector<Candidate> candidates, const Rules& rules, size_t fallbackCount)
	{
		std::vector<Candidate> kept;
		for (auto& candidate : candidates)
			if (rules.excluded.find(candidate.key) == rules.excluded.cend())
				kept.push_back(candidate);

		// The representatives first, then the cap of a system without favorites (a representative is kept)
		auto ordered = pool(kept, (size_t)-1);
		bool favorites = std::any_of(ordered.cbegin(), ordered.cend(), [](const Candidate& c) { return c.favorite; });

		std::stable_partition(ordered.begin(), ordered.end(), [&rules](const Candidate& c)
		{
			return rules.representatives.find(c.key) != rules.representatives.cend();
		});

		if (!favorites && ordered.size() > fallbackCount)
			ordered.resize(fallbackCount);

		std::vector<std::string> slides;
		for (auto& candidate : ordered)
			slides.push_back(candidate.media);

		return slides;
	}
}
