#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <array>
#include <optional>

namespace trainer {
enum class PokemonSlotKind { Empty, Known, Egg, Unreadable };
struct PokemonMove { QString name; int pp = 0, maxPp = 0; };
struct PokemonRecord {
    PokemonSlotKind kind = PokemonSlotKind::Empty;
    QString speciesId, formId, speciesName, nickname, ability, nature, item, condition;
    QStringList types;
    int number = 0, level = 0;
    bool shiny = false;
    std::optional<int> hp;
    // HP, Attack, Defense, Sp. Attack, Sp. Defense, Speed. Box stats are derived.
    std::array<int, 6> stats{};
    QList<PokemonMove> moves;
};
struct PokemonBox { QString name; QList<PokemonRecord> members; };
struct PartySnapshot {
    QString error;
    QList<PokemonRecord> party;
    QList<PokemonBox> boxes;
    int currentBox = 0;
};
}
