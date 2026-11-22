#include "utils/WordCompletion.h"

#include <cassert>
#include <clocale>

int main()
{
    assert(std::setlocale(LC_CTYPE, ""));

    WordCompletion model;
    for (const auto title : {"Super Mario Land", "Super Mario World", "Super Metroid",
             "Mega Man X", "Mega Man Zero", "Mario Tennis", "Mario Golf", "Mario Kart",
             "Pokémon Azul", "Pokémon Oro", "L'été - Final", "龍の冒険"})
        model.addTitle(title);

    auto words = [&](const std::string& query) {
        return model.suggest(query, query.size(), 3).words;
    };
    assert(words("su").at(0) == "Super");
    assert(words("Super ").at(0) == "Mario");
    assert(words("Super ").at(1) == "Metroid");
    assert(words("Mega m").at(0) == "Man");
    assert((words("Super Mario ") == std::vector<std::string>{"Land", "World"}));
    assert(words("Poké").at(0) == "Pokémon");
    assert(words("poke").at(0) == "Pokémon");
    assert(words("L'é").at(0) == "L'été");
    assert(words("龍").at(0) == "龍の冒険");
    assert(words("zzzz").empty());
    assert(words("Zero ").empty());

    std::string query = "Su";
    auto choices = model.suggest(query, query.size(), 3);
    assert(choices.insert(query, 0) == 6 && query == "Super ");
    choices = model.suggest(query, query.size(), 3);
    assert(choices.insert(query, 0) == 12 && query == "Super Mario ");

    std::string middle = "Mega Mxx Zero";
    choices = model.suggest(middle, 6, 3);
    assert(choices.insert(middle, 0) == 9 && middle == "Mega Man Zero");

    std::string punctuation = "Su: ";
    choices = model.suggest(punctuation, 2, 3);
    assert(choices.insert(punctuation, 0) == 5 && punctuation == "Super: ");
}
