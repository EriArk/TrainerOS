#pragma once
#include <QString>
#include <QList>
#include <QRegularExpression>

namespace trainer {
struct SeriesDefinition { QString id, name, colour, pattern; };
// Classification is a view over stable Adventures, never a ROM move or save migration.
inline const QList<SeriesDefinition>& seriesDefinitions() {
    static const QList<SeriesDefinition> definitions{
        {"pokemon", "Pokémon", "#eabc43", ""},
        {"mario", "Super Mario", "#e85a45", R"(\b(mario|luigi|yoshi|wario|captain toad)\b)"},
        {"zelda", "The Legend of Zelda", "#66b670", R"(\bzelda\b)"},
        {"sonic", "Sonic", "#4ea4ed", R"(\bsonic\b)"},
        {"final-fantasy", "Final Fantasy", "#8d91e8", R"(\bfinal fantasy\b)"},
        {"metroid", "Metroid", "#ed9941", R"(\bmetroid\b)"},
        {"castlevania", "Castlevania", "#b180d7", R"(\bcastlevania\b)"},
        {"kirby", "Kirby", "#ed8fac", R"(\bkirby\b)"},
        {"metal-slug", "Metal Slug", "#95b856", R"(\bmetal slug\b)"},
        {"multiverse", "Multiverse", "#6aabd7", ""}
    };
    return definitions;
}
inline QString seriesForTitle(QString title) {
    title=title.normalized(QString::NormalizationForm_KD).toLower();
    title.replace('-', ' '); title.replace('_', ' ');
    // Crossovers remain in Multiverse rather than silently choosing one franchise.
    if(title.contains("smash bros") || title.contains("olympic games"))return "multiverse";
    static const auto patterns=[] {
        QList<QPair<QString,QRegularExpression>> result;
        for(const auto& d:seriesDefinitions())if(!d.pattern.isEmpty())result.append({d.id,QRegularExpression(d.pattern,QRegularExpression::CaseInsensitiveOption)});
        return result;
    }();
    for(const auto& p:patterns)if(p.second.match(title).hasMatch())return p.first;
    return "multiverse";
}
inline SeriesDefinition seriesDefinition(const QString& id) {
    for(const auto& d:seriesDefinitions())if(d.id==id)return d;
    return seriesDefinitions().last();
}
inline QString seriesDisplayTitle(QString title) {
    // Display-only cleanup for automatically imported filenames. Keep unknown
    // annotations (hacks, translations, revisions) and all stored identities.
    static const QRegularExpression tags(
        R"(\s*\((?:(?:USA|US|Europe|World|Australia|Japan)(?:, (?:USA|Europe|World|Australia|Japan))*|[a-z]{2}(?:,[a-z]{2})+)\))",
        QRegularExpression::CaseInsensitiveOption);
    title.remove(tags);
    return title.simplified();
}
}
