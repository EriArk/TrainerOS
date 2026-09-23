#include "core/storage/LocalStateStore.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>

// Explicit private-copy validation; no GUI, controllers, platform services,
// account provider or external-save operation is constructed.
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    if(args.size()!=3 || args[1]!="--migration-copy" || !QFileInfo::exists(args[2]+"/traineros.sqlite3"))return 2;
    trainer::LocalStateStore store(args[2]);
    QObject::connect(&store,&trainer::LocalStateStore::opened,&app,[&](bool ready){
        if(!ready){qCritical()<<store.error();app.exit(1);return;}
        qInfo()<<"Store opened; owner present:"<<!store.ownerId().isEmpty()<<"Adventures:"<<store.adventures().size();
        app.exit(0);
    });
    QTimer::singleShot(30000,&app,[&]{app.exit(3);});
    QTimer::singleShot(0,&store,&trainer::LocalStateStore::open);
    return app.exec();
}
