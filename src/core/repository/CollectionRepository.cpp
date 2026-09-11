#include "CollectionRepository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QFileInfo>
#include <algorithm>

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
        result.append({p["id"].toString(), p["name"].toString(), p["badge"].toString(), p["shape"].toString()});
    }
    return result;
}
PlatformLabel platformLabel(const QString& id) {
    for (const auto& p : collectionPlatforms()) if (p.id == id) return p;
    return {id, "Unspecified platform", "?", "console"};
}
QList<World> collectionWorlds() {
    QList<World> result;
    for (const auto& value : data()["worlds"].toArray()) {
        const auto w = value.toObject(); result.append({w["id"].toString(), w["name"].toString(), {}});
    }
    return result;
}
QList<Adventure> collectionCatalogue() {
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
    return entries;
}
QList<World> CollectionRepository::worlds() const {
    auto result = collectionWorlds();
    for (const auto& w : personal_.worlds()) {
        if (std::none_of(result.begin(), result.end(), [&](const auto& existing) { return existing.id == w.id; })) result.append(w);
    }
    return result;
}
QList<Adventure> CollectionRepository::adventures() const {
    auto result = personal_.adventures(); QSet<QString> owned;
    const auto catalogue = collectionCatalogue();
    for (auto& a : result) if (!a.catalogueId.isEmpty()) {
        owned.insert(a.catalogueId);
        for (const auto& reference : catalogue) if (reference.catalogueId == a.catalogueId) {
            a.limitation = reference.limitation;
            if (a.description.isEmpty()) a.description = reference.description;
            break;
        }
    }
    for (const auto& a : collectionCatalogue()) if (!owned.contains(a.catalogueId)) result.append(a);
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        // Available files first, then the remaining checklist. Names are stable
        // across controller visits; a card never disappears merely for being absent.
        if (a.collectionOnly != b.collectionOnly) return !a.collectionOnly;
        const int order = QString::compare(a.title, b.title, Qt::CaseInsensitive);
        if (order != 0) return order < 0;
        return a.id < b.id;
    });
    return result;
}
void CollectionRepository::saveAdventureAsync(const AdventureRegistration& value, QObject* context,
        std::function<void(LibraryWriteResult)> completed) {
    auto record = value;
    if (!record.adventure.catalogueId.isEmpty()) {
        const auto catalogue = collectionCatalogue();
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
