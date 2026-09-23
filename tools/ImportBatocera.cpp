#include "core/repository/BatoceraLibrary.h"
#include "core/repository/CollectionRepository.h"
#include "core/storage/LocalStateStore.h"
#include "integrations/adventure/AdapterRouter.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

// Maintenance verification uses exactly the shell discovery and store path.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().size()!=3) {qCritical()<<"Usage: trainer_batocera_import DATA_DIRECTORY ROMS_DIRECTORY";return 2;}
    trainer::LocalStateStore store(QDir(app.arguments()[1]).absolutePath());
    trainer::CollectionRepository collection(store);
    trainer::BatoceraLibrary folders(collection,QDir(app.arguments()[2]).absolutePath());
    const auto integrations=QDir(app.arguments()[1]).filePath("integrations/");
    trainer::RetroArchAdapter retroarch(folders,trainer::RetroArchInstallation::load(integrations+"retroarch.json"));
    trainer::StandaloneAdapter melonds("melonds",folders,trainer::StandaloneInstallation::load(integrations+"melonds.json","melonds"));
    trainer::StandaloneAdapter dolphin("dolphin",folders,trainer::StandaloneInstallation::load(integrations+"dolphin.json","dolphin"));
    trainer::AdapterRouter adapters({&retroarch,&melonds,&dolphin});
    folders.prepareInstallation=[&](auto& r){adapters.prepareInstallation(r);};
    QObject::connect(&store,&trainer::LocalStateStore::opened,&app,[&](bool ready){if(ready)folders.refreshContentAvailability();else app.exit(1);});
    QObject::connect(&folders,&trainer::BatoceraLibrary::scanFinished,&app,[&](int added,const QStringList& warnings){
        int media=0;for(const auto& a:folders.adventures())if(!folders.artwork(a.id).value("cover").toString().isEmpty())++media;
        qInfo()<<"Added:"<<added<<"With artwork:"<<media<<"Warnings:"<<warnings;
        app.exit(warnings.isEmpty()?0:1);
    });
    store.open();return app.exec();
}
