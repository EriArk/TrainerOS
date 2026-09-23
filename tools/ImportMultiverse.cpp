#include "core/storage/LocalStateStore.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include "integrations/adventure/AdapterRouter.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>
#include <QSet>

// Explicit maintenance import. The same store lock, worker, validation and
// adapter composition as the shell own all mutations. Never touches ROM/save bytes.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    if(args.size()!=3){qCritical()<<"Usage: trainer_library_import DATA_DIRECTORY MANIFEST.json";return 2;}
    QFile file(args[2]);if(!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024)return 2;
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);
    const auto root=document.object();const auto entries=root["entries"].toArray();
    if(parse.error!=QJsonParseError::NoError||root["version"].toInt()!=1||entries.isEmpty()||entries.size()>5000)return 2;
    QList<trainer::AdventureRegistration> records;QSet<QString> ids,paths;
    for(const auto& item:entries) {
        const auto value=item.toObject();trainer::AdventureRegistration r;
        r.adventure.id=value["id"].toString();r.adventure.title=value["title"].toString();
        r.adventure.domain="multiverse";r.adventure.platformId=value["platform"].toString();
        r.adventure.description=value["description"].toString();r.adventure.adapterId="unconfigured";
        r.contentPath=value["path"].toString();const QFileInfo content(r.contentPath);
        if(r.adventure.id.isEmpty()||r.adventure.title.trimmed().isEmpty()||r.adventure.title.size()>96||r.adventure.platformId.isEmpty()
            ||!content.isAbsolute()||!content.isFile()||!content.isReadable()||ids.contains(r.adventure.id)||paths.contains(content.canonicalFilePath()))return 2;
        ids.insert(r.adventure.id);paths.insert(content.canonicalFilePath());records.append(r);
    }
    trainer::LocalStateStore store(QDir(args[1]).absolutePath());
    const auto integrations=QDir(args[1]).filePath("integrations/");
    trainer::RetroArchAdapter retroarch(store,trainer::RetroArchInstallation::load(integrations+"retroarch.json"));
    trainer::StandaloneAdapter melonds("melonds",store,trainer::StandaloneInstallation::load(integrations+"melonds.json","melonds"));
    trainer::StandaloneAdapter dolphin("dolphin",store,trainer::StandaloneInstallation::load(integrations+"dolphin.json","dolphin"));
    trainer::AdapterRouter adapters({&retroarch,&melonds,&dolphin});
    int index=0,added=0,skipped=0;
    std::function<void()> next;
    next=[&] {
        if(index==records.size()){qInfo()<<"Import complete. Added:"<<added<<"Preserved existing:"<<skipped;app.exit(0);return;}
        auto r=records[index++];
        if(store.registration(r.adventure.id)){++skipped;QTimer::singleShot(0,&app,next);return;}
        adapters.prepareInstallation(r);
        store.saveAdventureAsync(r,&app,[&](auto result){
            if(!result.success){qCritical()<<"Import stopped after"<<added<<"records:"<<result.error;app.exit(1);return;}
            ++added;QTimer::singleShot(0,&app,next);
        });
    };
    QObject::connect(&store,&trainer::LocalStateStore::opened,&app,[&](bool ready) {
        if(!ready){qCritical()<<store.error();app.exit(1);return;}
        // Validate all collisions before writing the first new entry. Existing
        // names/configuration/history are preserved on an idempotent retry.
        for(const auto& r:records) {
            const auto existing=store.registration(r.adventure.id);
            if(existing && (existing->adventure.domain!="multiverse"||existing->adventure.platformId!=r.adventure.platformId
                ||QFileInfo(existing->contentPath).canonicalFilePath()!=QFileInfo(r.contentPath).canonicalFilePath())) {
                qCritical()<<"Existing identity differs; import unchanged.";app.exit(1);return;
            }
            for(const auto& a:store.adventures())if(a.id!=r.adventure.id) {
                const auto other=store.registration(a.id);
                if(other && QFileInfo(other->contentPath).canonicalFilePath()==QFileInfo(r.contentPath).canonicalFilePath()) {
                    qCritical()<<"File already registered with another identity; import unchanged.";app.exit(1);return;
                }
            }
        }
        next();
    });
    QTimer::singleShot(0,&store,&trainer::LocalStateStore::open);
    return app.exec();
}
