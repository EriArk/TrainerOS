#include "OfflinePokedex.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

static void initializePokedexResource() { Q_INIT_RESOURCE(pokedex_reference); }
namespace trainer {
PokedexCatalog OfflinePokedex::load() {
    if (catalog_) return *catalog_;
    initializePokedexResource();
    QFile file(":/pokedex/pokedex.json");
    if (!file.open(QIODevice::ReadOnly)) return {false,{}, {},"The offline field guide couldn't be opened."};
    const auto loaded=decode(file.readAll());
    if (loaded.success) catalog_=loaded;
    return loaded;
}
PokedexCatalog OfflinePokedex::decode(const QByteArray& bytes) {
    const auto failed=[] { return PokedexCatalog{false,{}, {},"The offline field guide needs recovery. Reinstall the application to restore its reference data."}; };
    if (bytes.size()>8*1024*1024) return failed();
    const auto root=QJsonDocument::fromJson(bytes).object();
    if (root["version"].toInt()!=1 || !root["entries"].isArray() || !root["collections"].isArray()) return failed();
    PokedexCatalog catalog;catalog.source="Offline · PokéAPI";
    QSet<QString> collections,ids,forms;QSet<int> numbers;
    for (const auto& item : root["collections"].toArray()) {
        const auto value=item.toObject();const auto id=value["id"].toString(),name=value["name"].toString();
        if (id.isEmpty() || name.isEmpty() || collections.contains(id)) return failed();
        collections.insert(id);catalog.collections.append({id,name});
    }
    // The current controller chooser has twenty cells, including All and Cancel.
    if (collections.size()>18) return failed();
    for (const auto& item : root["entries"].toArray()) {
        const auto value=item.toObject();PokedexEntry entry;
        entry.id=value["id"].toString();entry.number=value["number"].toInt();entry.name=value["name"].toString();
        if (entry.id.isEmpty() || entry.name.isEmpty() || entry.number<=0 || entry.number>10000 || ids.contains(entry.id) || numbers.contains(entry.number)) return failed();
        ids.insert(entry.id);numbers.insert(entry.number);
        for (const auto& region : value["collections"].toArray()) {
            if (!collections.contains(region.toString())) return failed();
            entry.collectionIds.append(region.toString());
        }
        for (const auto& related : value["family"].toArray()) entry.familyIds.append(related.toString());
        for (const auto& itemForm : value["forms"].toArray()) {
            const auto valueForm=itemForm.toObject();PokedexForm form;
            form.id=valueForm["id"].toString();form.name=valueForm["name"].toString();
            if (form.id.isEmpty() || form.name.isEmpty() || forms.contains(form.id)) return failed();
            forms.insert(form.id);
            for (const auto& type : valueForm["types"].toArray()) { if(type.toString().isEmpty())return failed();form.types.append(type.toString()); }
            form.heightDm=valueForm["heightDm"].toInt();form.weightHg=valueForm["weightHg"].toInt();
            for (const auto& stat : valueForm["stats"].toArray()) { const int n=stat.toInt();if(n<0||n>255)return failed();form.stats.append(n); }
            if (form.types.isEmpty() || form.types.size()>2 || form.heightDm<0 || form.weightHg<0 || form.stats.size()!=6) return failed();
            entry.forms.append(form);
        }
        if (entry.forms.isEmpty()) return failed();
        entry.types=entry.forms.first().types;catalog.entries.append(entry);
    }
    if (catalog.entries.size()!=root["speciesCount"].toInt() || forms.size()!=root["formCount"].toInt() || catalog.entries.isEmpty()) return failed();
    for (const auto& entry : catalog.entries) for (const auto& relative : entry.familyIds) if(!ids.contains(relative))return failed();
    return catalog;
}
}
