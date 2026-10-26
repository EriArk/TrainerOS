#include "PcSteamBridge.h"
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>
namespace trainer {
namespace {
void reply(QLocalSocket* socket,const QJsonObject& value) {socket->write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');socket->flush();}
}
PcSteamBridge::PcSteamBridge(QObject* parent):QObject(parent) {
    connect(&server_,&QLocalServer::newConnection,this,[this]{
        while(auto* socket=server_.nextPendingConnection()) {
            socket->setReadBufferSize(4097);
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            QTimer::singleShot(10000,socket,[socket]{if(!socket->property("accepted").toBool())socket->disconnectFromServer();});
            connect(socket,&QLocalSocket::readyRead,this,[this,socket]{
                if(socket->property("accepted").toBool()){socket->readAll();return;}
                if(socket->bytesAvailable()>4096){socket->disconnectFromServer();return;}
                if(!socket->canReadLine())return;
                const auto request=QJsonDocument::fromJson(socket->readLine(4096)).object();
                const auto id=request["id"].toString();QString error;
                if(request["owner"].toString()!=owner_||!id.startsWith("pc:")||QUuid(id.mid(3)).isNull())error="This Trainer profile is not active.";
                else if(active_)error="A Steam game is already running.";
                else if(!launch)error="PC launch is unavailable.";
                else {
                    environment_={};const auto supplied=request["environment"].toObject();
                    for(const auto& key:{"SteamAppId","SteamGameId","SteamOverlayGameId"}) {
                        const auto value=supplied[key].toString();bool number=false;value.toULongLong(&number);
                        if(number&&value.size()<=20)environment_[key]=value;
                    }
                    active_=true;error=launch(id);if(!error.isEmpty()){active_=false;environment_={};}
                }
                if(!error.isEmpty()){reply(socket,{{"error",error}});socket->disconnectFromServer();return;}
                client_=socket;socket->setProperty("accepted",true);reply(socket,{{"state","started"}});
            });
        }
    });
}
void PcSteamBridge::configure(const QString& state,const QString& owner) {
    if(active_||owner_==owner)return;
    server_.close();lock_.reset();owner_=owner;if(owner.isEmpty())return;
#ifdef Q_OS_LINUX
    const auto hash=QString::fromLatin1(QCryptographicHash::hash((state+'|'+owner).toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if(directory.isEmpty()||!QDir().mkpath(directory))return;
    const auto socket=directory+"/traineros-pc-"+hash;
    lock_=std::make_unique<QLockFile>(socket+".lock");lock_->setStaleLockTime(0);
    if(!lock_->tryLock())return;
    QLocalServer::removeServer(socket);server_.setSocketOptions(QLocalServer::UserAccessOption);
    if(!server_.listen(socket))return;
    const auto root=state+"/pc/"+owner;QDir().mkpath(root);QSaveFile endpoint(root+"/steam-endpoint.json");
    const auto bytes=QJsonDocument(QJsonObject{{"socket",socket}}).toJson(QJsonDocument::Compact);
    if(!endpoint.open(QIODevice::WriteOnly)||endpoint.write(bytes)!=bytes.size()||!endpoint.commit())server_.close();
#endif
}
void PcSteamBridge::finished(bool failed) {
    if(client_){reply(client_,{{"state","finished"},{"exitCode",failed?1:0}});client_->disconnectFromServer();client_.clear();}
    active_=false;environment_={};
}
}
