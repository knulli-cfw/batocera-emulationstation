#include "WordCompletion.h"
#include "utils/StringUtil.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <tuple>

namespace {
struct Token { size_t begin, end; };

std::vector<Token> tokenize(const std::string& text)
{
	std::vector<Token> tokens;
	bool inWord = false;
	for (size_t next = 0; next < text.size();)
	{
		const auto start = next;
		const auto ch = Utils::String::chars2Unicode(text, next);
		bool letter = std::iswalnum(ch) || (inWord && ch >= 0x300 && ch <= 0x36f);
		if (inWord && (ch == '\'' || ch == 0x2019) && next < text.size())
		{
			auto peek = next;
			letter = std::iswalnum(Utils::String::chars2Unicode(text, peek));
		}
		if (letter && !inWord) tokens.push_back({start, next});
		else if (letter) tokens.back().end = next;
		inWord = letter;
	}
	return tokens;
}

// Match common accented spellings from the pad's primary letter layer.
std::string key(const std::string& word)
{
	auto lower = Utils::String::toLower(word);
	std::string out;
	for (size_t next = 0; next < lower.size();)
	{
		const auto start = next;
		const auto ch = Utils::String::chars2Unicode(lower, next);
		switch (ch)
		{
		case U'à': case U'á': case U'â': case U'ä': case U'ã': out += 'a'; break;
		case U'è': case U'é': case U'ê': case U'ë': out += 'e'; break;
		case U'ì': case U'í': case U'î': case U'ï': out += 'i'; break;
		case U'ò': case U'ó': case U'ô': case U'ö': case U'õ': out += 'o'; break;
		case U'ù': case U'ú': case U'û': case U'ü': out += 'u'; break;
		case U'ñ': out += 'n'; break;
		case U'ç': out += 'c'; break;
		case 0x2019: out += '\''; break;
		default:
			if (ch < 0x300 || ch > 0x36f)
				out.append(lower, start, next-start);
		}
	}
	return out;
}
}

void WordCompletion::addTitle(const std::string& title)
{
	std::string older, previous;
	for (auto token : tokenize(title))
	{
		const auto spelling = title.substr(token.begin, token.end-token.begin);
		const auto folded = key(spelling);
		auto& word = mWords[folded];
		if (word.count++ == 0) word.spelling = spelling;
		if (!older.empty()) ++mPhrases[{older, previous}][folded];
		if (!previous.empty()) ++mWords.at(previous).following[folded];
		else ++word.starts;
		older = previous;
		previous = folded;
	}
}

WordCompletion::Choices WordCompletion::suggest(const std::string& text, size_t cursor, size_t limit) const
{
	cursor = std::min(cursor, text.size());
	Choices result{cursor, cursor, false, {}};
	std::string prefix, previous, older;
	for (auto token : tokenize(text))
	{
		if (cursor < token.begin) break;
		if (cursor <= token.end)
		{
			result.begin = token.begin;
			result.end = token.end;
			prefix = key(text.substr(token.begin, cursor-token.begin));
			result.completing = !prefix.empty();
			break;
		}
		older = previous;
		previous = key(text.substr(token.begin, token.end-token.begin));
	}

	const auto context = mWords.find(previous);
	const auto phrase = mPhrases.find({older, previous});
	struct Candidate { unsigned pair, adjacent, frequency; const std::string* word; };
	std::vector<Candidate> best;
	auto consider = [&](const Word& word, unsigned pair, unsigned adjacent, unsigned frequency)
	{
		Candidate candidate{pair, adjacent, frequency, &word.spelling};
		// The dictionary is ordered, so equal counts retain a stable alphabetical order.
		auto pos = std::find_if(best.begin(), best.end(), [&](const Candidate& other) {
			return std::tie(pair, adjacent, frequency) > std::tie(other.pair, other.adjacent, other.frequency);
		});
		if (pos == best.end() && best.size() == limit) return;
		best.insert(pos, candidate);
		if (best.size() > limit) best.pop_back();
	};

	if (prefix.empty() && !previous.empty())
	{
		if (context != mWords.end())
		{
			const auto& following = phrase == mPhrases.end() ? context->second.following : phrase->second;
			for (const auto& next : following)
			{
				const auto& word = mWords.at(next.first);
				consider(word, next.second, context->second.following.at(next.first), word.count);
			}
		}
	}
	else
	{
		for (auto it = mWords.lower_bound(prefix); it != mWords.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it)
		{
			const auto& word = it->second;
			unsigned adjacent = 0;
			if (context != mWords.end())
			{
				auto next = context->second.following.find(it->first);
				if (next != context->second.following.end()) adjacent = next->second;
			}
			unsigned pair = 0;
			if (phrase != mPhrases.end())
			{
				auto next = phrase->second.find(it->first);
				if (next != phrase->second.end()) pair = next->second;
			}
			if (!prefix.empty() || word.starts) consider(word, pair, adjacent, prefix.empty() ? word.starts : word.count);
		}
	}
	for (auto candidate : best) result.words.push_back(*candidate.word);
	return result;
}

size_t WordCompletion::Choices::insert(std::string& text, size_t index) const
{
	const auto& word = words.at(index);
	text.replace(begin, end-begin, word);
	auto cursor = begin + word.size();
	if (cursor == text.size()) text.push_back(' ');
	else if (!std::isspace(static_cast<unsigned char>(text[cursor]))) return cursor;
	return cursor+1;
}
