#pragma once

#include <map>
#include <string>
#include <vector>

// The vocabulary belongs to the currently visible library. Offsets are UTF-8 byte positions.
class WordCompletion
{
public:
	struct Choices
	{
		size_t begin, end;
		bool completing;
		std::vector<std::string> words;
		size_t insert(std::string& text, size_t index) const;
	};

	void addTitle(const std::string& title);
	Choices suggest(const std::string& text, size_t cursor, size_t limit) const;

private:
	struct Word
	{
		std::string spelling;
		unsigned count = 0, starts = 0;
		std::map<std::string, unsigned> following;
	};
	std::map<std::string, Word> mWords;
	std::map<std::pair<std::string, std::string>, std::map<std::string, unsigned>> mPhrases;
};
