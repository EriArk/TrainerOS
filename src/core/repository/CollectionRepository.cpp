#include "CollectionRepository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QFileInfo>
#include <QDataStream>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
#include <tuple>

// Static-library resources must be explicitly linked by each consuming binary.
static void initializeCollection() { Q_INIT_RESOURCE(collection); }
namespace trainer {
namespace {
const QJsonObject& data() {
    static const auto value = [] {
        initializeCollection();
        QFile file(":/collection/catalogue.json");
        if (!file.open(QIODevice::ReadOnly)) qFatal("Cannot open the bundled collection catalogue");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || document.object()["version"].toInt() != 1)
            qFatal("Invalid bundled collection catalogue");
        return document.object();
    }();
    return value;
}
}
QList<PlatformLabel> collectionPlatforms() {
    QList<PlatformLabel> result;
    for (const auto& value : data()["platforms"].toArray()) {
        const auto p = value.toObject();
        result.append({p["id"].toString(), p["name"].toString(), p["badge"].toString(), p["shape"].toString(),
            p["chronology"].isDouble() ? std::optional<int>(p["chronology"].toInt()) : std::nullopt});
    }
    return result;
}
PlatformLabel platformLabel(const QString& id) {
    for (const auto& p : collectionPlatforms()) if (p.id == id) return p;
    for (const auto& p : multiversePlatforms()) if (p.id == id) return p;
    return {id, "Unspecified platform", "?", "console"};
}
QList<PlatformLabel> multiversePlatforms() {
    // Registry membership describes a content category, not installed runtime
    // readiness. Empty systems never become cards in the production browser.
    QList<PlatformLabel> result;
    for (const auto& p : collectionPlatforms()) if (p.id != "android" && p.id != "pc") result.append(p);
    const QList<PlatformLabel> additional{
        {"nes","Nintendo Entertainment System","NES","cartridge"},
        {"snes","Super Nintendo","SNES","cartridge"},
        {"virtualboy","Virtual Boy","VB","console"},
        {"mastersystem","Master System","SMS","cartridge"},
        {"megadrive","Mega Drive","MD","cartridge"},
        {"gamegear","Game Gear","GG","handheld"},
        {"segacd","Mega-CD","MCD","disc"},{"sega32x","32X","32X","cartridge"},
        {"saturn","Saturn","SAT","disc"},{"dreamcast","Dreamcast","DC","disc"},
        {"psx","PlayStation","PS","disc"},{"ps2","PlayStation 2","PS2","disc"},
        {"psp","PSP","PSP","handheld"},{"vita","PS Vita","VITA","handheld"},
        {"pcengine","PC Engine","PCE","cartridge"},{"pcenginecd","PC Engine CD","PCE CD","disc"},
        {"supergrafx","SuperGrafx","SGX","cartridge"},
        {"ngp","Neo Geo Pocket","NGP","handheld"},{"ngpc","Neo Geo Pocket Color","NGPC","handheld"},
        {"neogeo","Neo Geo","NG","cartridge"},{"neogeocd","Neo Geo CD","NG CD","disc"},
        {"fbneo","Arcade · FinalBurn Neo","ARCADE","console"},{"mame","Arcade · MAME","ARCADE","console"},
        {"atomiswave","Atomiswave","AW","console"},{"naomi","NAOMI","NAOMI","console"},
        {"atari2600","Atari 2600","2600","cartridge"},{"atari5200","Atari 5200","5200","cartridge"},
        {"atari7800","Atari 7800","7800","cartridge"},{"lynx","Atari Lynx","LYNX","handheld"},
        {"jaguar","Atari Jaguar","JAG","cartridge"},{"3do","3DO","3DO","disc"},
        {"wonderswan","WonderSwan","WS","handheld"},{"wonderswancolor","WonderSwan Color","WSC","handheld"},
        {"colecovision","ColecoVision","CV","cartridge"},{"intellivision","Intellivision","INTV","cartridge"},
        {"vectrex","Vectrex","VEC","console"},{"c64","Commodore 64","C64","computer"},
        {"amiga","Amiga","AMIGA","computer"},{"amstradcpc","Amstrad CPC","CPC","computer"},
        {"zxspectrum","ZX Spectrum","ZX","computer"},{"msx","MSX","MSX","computer"},
        {"msx2","MSX2","MSX2","computer"},{"dos","DOS","DOS","computer"},
        {"scummvm","ScummVM","SCUMM","computer"}};
    for (const auto& p : additional) if (std::none_of(result.begin(), result.end(), [&](const auto& r) { return r.id == p.id; })) result.append(p);
    return result;
}
QList<EditionChronology> collectionChronology() {
    QList<EditionChronology> result;
    for (const auto& value : data()["editions"].toArray()) {
        const auto e = value.toObject();
        result.append({e["id"].toString(), e["platform"].toString(),
            e["releaseYear"].isDouble() ? std::optional<int>(e["releaseYear"].toInt()) : std::nullopt,
            e["order"].isDouble() ? std::optional<int>(e["order"].toInt()) : std::nullopt});
    }
    return result;
}
void sortWorldAdventures(QList<Adventure>& adventures, const QList<PlatformLabel>& platforms,
        const QList<EditionChronology>& editions) {
    constexpr int unknown = std::numeric_limits<int>::max();
    QHash<QString, int> eras;
    QHash<QString, EditionChronology> releases;
    for (const auto& p : platforms) eras.insert(p.id, p.chronology.value_or(unknown));
    for (const auto& e : editions) releases.insert(e.id, e);
    using Key = std::tuple<int, QString, int, int, QString, QString>;
    QHash<QString, Key> keys;
    for (const auto& a : adventures) {
        int year = unknown, order = unknown;
        const auto e = releases.constFind(a.catalogueId);
        // A hack has its own release history, even if its record references a base edition.
        if (a.kind != AdventureKind::RomHack && e != releases.cend() && e->platformId == a.platformId) {
            year = e->releaseYear.value_or(unknown); order = e->order.value_or(unknown);
        }
        keys.insert(a.id, {eras.value(a.platformId, unknown), a.platformId, year, order,
            a.title.toCaseFolded(), a.id});
    }
    std::sort(adventures.begin(), adventures.end(), [&](const auto& a, const auto& b) {
        return keys.value(a.id) < keys.value(b.id);
    });
}
QList<World> collectionWorlds() {
    QList<World> result;
    for (const auto& value : data()["worlds"].toArray()) {
        const auto w = value.toObject(); result.append({w["id"].toString(), w["name"].toString(), {}});
    }
    return result;
}
QList<Adventure> collectionCatalogue(bool includeExcluded) {
    static const auto entries = [] {
        QList<Adventure> result;
        for (const auto& value : data()["editions"].toArray()) {
            const auto e = value.toObject(); Adventure a;
            a.catalogueId = e["id"].toString(); a.id = "catalogue:" + a.catalogueId;
            a.title = e["title"].toString(); a.platformId = e["platform"].toString();
            a.worldId = e["world"].toString(); a.adapterId = "unconfigured";
            a.kind = e["remake"].toBool() ? AdventureKind::Remake : AdventureKind::Original;
            for (const auto& w : e["also"].toArray()) a.additionalWorldIds.append(w.toString());
            a.description = e["description"].toString(); a.limitation = e["limitation"].toString();
            a.collectionOnly = true; result.append(a);
        }
        return result;
    }();
    if (includeExcluded) return entries;
    static const auto visible=[&] {
        QList<Adventure> result;
        for (const auto& a : entries) if (collectionExclusion(a).isEmpty()) result.append(a);
        return result;
    }();
    return visible;
}
QString collectionIdentity(const Adventure& a) {
    static const auto canonicalNames=[] {QHash<QString,QString> names;for(const auto& value:data()["editions"].toArray()){const auto e=value.toObject();names.insert(e["id"].toString(),e["title"].toString());}return names;}();
    const auto title=a.kind==AdventureKind::RomHack ? a.title : canonicalNames.value(a.catalogueId,a.title);
    QString name=title.section(QString::fromUtf8(" · "),0,0).normalized(QString::NormalizationForm_D).toCaseFolded();
    static const QRegularExpression accents("\\p{M}"), punctuation("\\bversion\\b|[^a-z0-9]");
    name.remove(accents);
    name.replace("pocket monsters","pokemon");
    name.remove(punctuation);
    if(name=="pokemoncardgb2grdansanjou" || name=="pokemontradingcardgame2") name="pokemontradingcardgame2theinvasionofteamgr";
    if(name=="pocketmonstersmidoriprepatchedenglish" || name=="pokemonmidoriprepatchedenglish") name="pokemongreen";
    // A hack called Ruby is still not the official Ruby edition.
    return a.platformId + ":" + (a.kind==AdventureKind::RomHack ? "hack:" : "official:") + name;
}
QString collectionExclusion(const Adventure& a, const QString& filename) {
    if (a.domain!="pokemon") return {};
    const QString evidence=(a.variant+" "+QFileInfo(filename).fileName()+" "+a.title).toCaseFolded();
    static const QRegularExpression translated("english[ _-]*translation|(?:pre[ _-]*patched|translated)[ _-]*english|\\[t[+-]eng");
    const bool english=translated.match(evidence).hasMatch();
    if(evidence.contains("debug build") || evidence.contains("intro hack") || evidence.contains("unverified") || a.title==QString::fromUtf8("Pokémon")) return "ambiguous-or-service-build";
    static const QRegularExpression other("\\b(?:japan(?:ese)?|korea(?:n)?|china|chinese|taiwan|france|french|germany|german|spain|spanish|italy|italian|portugal|portuguese|brazil|russia|russian|dutch)\\b");
    // A multilingual European release with En is eligible; region alone is not language.
    static const QRegularExpression englishTag("\\([^)]*\\ben\\b[^)]*\\)");
    static const QRegularExpression languageCodes("\\((?:j|f|g|s|i|k|fr|de|es|it|ja|jp|ko|zh|pt|ru|nl)(?:,(?:fr|de|es|it|ja|jp|ko|zh|pt|ru|nl))*\\)");
    if (!english && !englishTag.match(evidence).hasMatch() && (other.match(evidence).hasMatch() || languageCodes.match(evidence).hasMatch())) return "non-english";
    static const auto exclusions=[] {
        QHash<QString,QString> byKey;
        for (const auto& value : data()["editions"].toArray()) {
            const auto e=value.toObject(); const auto reason=e["excludedFromWorlds"].toString();
            if(reason.isEmpty()) continue;
            byKey.insert("id:"+e["id"].toString(),reason);
            Adventure ref;ref.title=e["title"].toString();ref.platformId=e["platform"].toString();
            byKey.insert(collectionIdentity(ref),reason);
        }
        return byKey;
    }();
    static const QSet<QString> officialNames=[] { QSet<QString> names; for(const auto& value:data()["editions"].toArray()){const auto e=value.toObject();Adventure ref;ref.title=e["title"].toString();ref.platformId=e["platform"].toString();ref.kind=AdventureKind::RomHack;names.insert(collectionIdentity(ref));}return names;}();
    if(a.kind==AdventureKind::RomHack && !english && officialNames.contains(collectionIdentity(a))) return "unidentified-base-game-variant";
    auto reason=exclusions.value("id:"+a.catalogueId);
    if(reason.isEmpty() && a.kind!=AdventureKind::RomHack) reason=exclusions.value(collectionIdentity(a));
    if(reason=="non-english-original" && english) return {};
    if(!reason.isEmpty()) return reason;
    if (a.platformId=="android" || a.platformId=="pico") return "companion-service-or-special-input";
    if (evidence.contains("wii injection") || evidence.contains("virtual console")) return "duplicate-rerelease";
    return {};
}
QList<World> CollectionRepository::worlds() const {
    auto result = collectionWorlds();
    for (const auto& w : personal_.worlds()) {
        auto it=std::find_if(result.begin(),result.end(),[&](const auto& existing){return existing.id==w.id;});
        if(it==result.end())result.append(w); else it->name=w.name;
    }
    const auto games=adventures();
    result.removeIf([&](const auto& w) { return std::none_of(games.cbegin(),games.cend(),[&](const auto& a){return a.domain=="pokemon" && (a.worldId==w.id || a.additionalWorldIds.contains(w.id));}); });
    return result;
}
QList<Adventure> CollectionRepository::adventures() const {
    auto candidates = personal_.adventures();
    const auto registrations=personal_.registrations();
    // UI consumers ask for this projection repeatedly. Normalize and sort only
    // when its actual source data changes, including unavailable/removed files.
    QByteArray key; QDataStream stream(&key,QIODevice::WriteOnly);
    stream << candidates.size() << registrations.size();
    for(const auto& a:candidates) stream << a.id << a.worldId << a.title << a.adapterId << int(a.kind)
        << a.description << bool(a.status) << int(a.status.value_or(JourneyStatus::NotStarted))
        << a.badges.value_or(-1) << a.caught.value_or(-1) << a.additionalWorldIds
        << a.platformId << a.catalogueId << a.variant << a.collectionOnly << a.limitation << a.domain;
    for(const auto& r:registrations) stream << r.adventure.id << r.revision << r.contentAvailable << r.removed << r.contentPath;
    if(key==curatedKey_) return curatedAdventures_;
    QList<Adventure> result; QSet<QString> owned, identities;
    // Stable winner; prefer a present file and configured adapter, without merging history.
    QHash<QString,std::pair<bool,bool>> ranks;
    QHash<QString,QString> paths;
    for(const auto& r:registrations) {ranks.insert(r.adventure.id,{r.contentAvailable,r.adventure.adapterId!="unconfigured"});paths.insert(r.adventure.id,r.contentPath);}
    std::sort(candidates.begin(),candidates.end(),[&](const auto& a,const auto& b){
        if(ranks.value(a.id)!=ranks.value(b.id)) return ranks.value(a.id)>ranks.value(b.id);
        return a.id<b.id;
    });
    for (const auto& a : candidates) {
        if (a.domain!="pokemon") { result.append(a); continue; }
        if (!collectionExclusion(a,paths.value(a.id)).isEmpty()) continue;
        const auto identity=collectionIdentity(a);
        if (identities.contains(identity)) continue;
        identities.insert(identity);result.append(a);
    }
    const auto catalogue = collectionCatalogue();
    for (auto& a : result) if (a.domain == "pokemon" && !a.catalogueId.isEmpty()) {
        owned.insert(a.catalogueId);
        for (const auto& reference : catalogue) if (reference.catalogueId == a.catalogueId) {
            a.limitation = reference.limitation;
            if (a.description.isEmpty()) a.description = reference.description;
            break;
        }
    }
    for (const auto& a : collectionCatalogue()) if (!owned.contains(a.catalogueId) && !identities.contains(collectionIdentity(a))) result.append(a);
    sortWorldAdventures(result, collectionPlatforms(), collectionChronology());
    curatedKey_=std::move(key);curatedAdventures_=result;
    return result;
}
void CollectionRepository::saveAdventureAsync(const AdventureRegistration& value, QObject* context,
        std::function<void(LibraryWriteResult)> completed) {
    auto record = value;
    if (!record.adventure.catalogueId.isEmpty()) {
        const auto catalogue = collectionCatalogue(true);
        const auto entry = std::find_if(catalogue.begin(), catalogue.end(), [&](const auto& a) { return a.catalogueId == record.adventure.catalogueId; });
        if (entry == catalogue.end() || entry->platformId != record.adventure.platformId) {
            completed({false, "This file needs the matching platform edition. Choose its card in Worlds."}); return;
        }
        const QHash<QString, QStringList> formats{
            {"gb", {"gb", "zip", "7z"}}, {"gbc", {"gb", "gbc", "zip", "7z"}}, {"gba", {"gba", "zip", "7z"}},
            {"nds", {"nds", "srl", "zip", "7z"}}, {"n3ds", {"3ds", "cci", "cxi", "cia"}},
            {"n64", {"z64", "n64", "v64", "zip", "7z"}}, {"gc", {"iso", "gcm", "rvz", "gcz", "ciso"}},
            {"wii", {"iso", "rvz", "gcz", "wbfs", "wad", "ciso"}}, {"wiiu", {"wua", "wud", "wux", "rpx"}},
            {"switch", {"nsp", "xci", "nro"}}, {"pokemini", {"min", "zip", "7z"}},
            {"pc", {"exe", "iso", "cue", "bin", "zip", "7z"}}, {"pico", {"md", "bin", "zip", "7z"}}, {"android", {"apk", "apkm", "xapk", "apks"}}};
        if (!formats.value(entry->platformId).contains(QFileInfo(record.contentPath).suffix().toLower())) {
            completed({false, "Choose a game file for " + platformLabel(entry->platformId).name + ". This file type belongs to another format."}); return;
        }
    }
    const auto storedWorlds = personal_.worlds();
    if (!record.newWorld && std::none_of(storedWorlds.begin(), storedWorlds.end(), [&](const auto& w) { return w.id == record.adventure.worldId; })) {
        for (const auto& w : collectionWorlds()) if (w.id == record.adventure.worldId) record.newWorld = w;
    }
    for (const auto& id : record.adventure.additionalWorldIds) {
        if (std::any_of(storedWorlds.begin(), storedWorlds.end(), [&](const auto& w) { return w.id == id; })) continue;
        for (const auto& w : collectionWorlds()) if (w.id == id) record.additionalNewWorlds.append(w);
    }
    record.adventure.collectionOnly = false;
    personal_.saveAdventureAsync(record, context, std::move(completed));
}
}
