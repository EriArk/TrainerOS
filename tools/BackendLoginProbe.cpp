// Operator-only live acceptance probe. No tokens/passwords on stdout or disk.
// Browser uses an SSH-forwarded loopback port when the probe runs on the server.
#include "features/accounts/BackendAccount.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTimer>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>
using namespace trainer;
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().size()!=3)return 2;
    QCoreApplication::setOrganizationName("TrainerOSProbe");QCoreApplication::setApplicationName("NativeLogin");QStandardPaths::setTestModeEnabled(true);
    const auto pendingPath=app.arguments()[1],resultPath=app.arguments()[2];
    QHash<QString,QByteArray> memory;QString firstPlayer;int phase=0;bool restoring=false;
    BackendAccount account;
    auto save=[](const QString& path,const QJsonObject& data){QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate))return false;file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);return file.write(QJsonDocument(data).toJson())>0;};
    BackendAccount::Services services;
    services.read=[&](QString key,auto done){done(true,memory.value(key));};
    services.write=[&](QString key,QByteArray bytes,auto done){memory[key]=bytes;done(true);};
    services.openBrowser=[&](QUrl url){
        // Discover only this object's loopback listener; never inspect token payloads.
        const auto listeners=account.findChildren<QTcpServer*>();
        int port=0;for(auto* listener:listeners)if(listener->isListening())port=listener->serverPort();
        return save(pendingPath,{{"authorization_url",url.toString()},{"return_port",port}});
    };
    account.setServices(services);
    QObject::connect(&account,&BackendAccount::changed,&app,[&]{
        qInfo().noquote()<<account.detail();
        const auto snapshot=account.snapshot();
        if(snapshot["state"]=="signed_in"&&phase==0){
            firstPlayer=snapshot["player_id"].toString();phase=1;
            QTimer::singleShot(0,&app,[&]{restoring=true;account.configure({});account.configure("live-native-probe");});
        } else if(snapshot["state"]=="signed_in"&&phase==1&&restoring&&account.detail()=="Connected through Fluxer") {
            if(snapshot["player_id"].toString()!=firstPlayer){app.exit(3);return;}
            phase=2;QTimer::singleShot(0,&app,[&]{account.activate();});
        } else if(phase==2&&account.detail().startsWith("Signed out.")) {
            save(resultPath,{{"native_browser_return",true},{"session_restored_with_refresh",true},{"same_player",true},{"server_session_revoked",true},
                {"storage","test-only memory; not vault acceptance"},{"handheld",false}});QFile::remove(pendingPath);app.quit();
        }
    });
    QTimer::singleShot(300000,&app,[&]{QFile::remove(pendingPath);app.exit(4);});
    account.configure("live-native-probe");account.activate();
    return app.exec();
}
