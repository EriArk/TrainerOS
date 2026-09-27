#pragma once
#include <QString>
#include <QStringList>

namespace trainer {
// Built-in presentation identities, not executable plugins or save authority.
// Persist IDs rather than display labels; artwork packs have a separate schema.
struct Experience {
    QString id;
    int version;
    QStringList pokemonFaces;
};
inline const Experience& pokemonExperience() {
    static const Experience value{"pokemon",1,{"dex","party","boxes","center","playroom","shops"}};
    return value;
}
inline bool knownExperience(const QString& id) {return id=="pokemon" || id=="multiverse";}
}
