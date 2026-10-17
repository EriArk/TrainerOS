#include "RuntimeMultiplayer.h"
#include "RuntimeIdentity.h"
#include <QtConcurrent>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
namespace trainer {
void RuntimeMultiplayer::verifyImport(AdventureRegistration record,QJsonObject wanted,
 std::shared_ptr<std::atomic_bool> cancelled,ImportCheck done) {
    const auto offered=wanted["runtime"].toObject();
    const bool remote=wanted["mode"]=="remoteplay.v1";
    const auto digest=QString::fromLatin1(QCryptographicHash::hash((offered["id"].toString()+"/"+offered["content"].toString()).toUtf8(),QCryptographicHash::Sha256).toHex());
    if(!allowed_||offered.isEmpty()||(remote?QString::fromLatin1(QCryptographicHash::hash("remoteplay.v1/"+QJsonDocument(offered).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex())!=wanted["id"]:(digest!=wanted["id"]||offered["id"]!=wanted["mode"]))){done({},"The lobby game changed or has no verified runtime identity.");return;}
    auto* watcher=new QFutureWatcher<QPair<QJsonObject,QString>>(this);
    connect(watcher,&QFutureWatcherBase::finished,this,[watcher,cancelled,done]{
        const auto result=watcher->result();watcher->deleteLater();if(!*cancelled)done(result.first,result.second);
    });
    watcher->setFuture(QtConcurrent::run([record,wanted,offered,remote,cancelled,installation=installation_,psp=ppsspp_.installation(),dolphin=dolphin_.installation()]{
        using Result=QPair<QJsonObject,QString>;
        const QFileInfo before(record.contentPath);
        if(!before.isFile()||!before.isReadable())return Result{{},"This file is unavailable or cannot be read."};
        const auto size=before.size();const auto modified=before.lastModified();
        const auto profile=runtimeIdentity(record,installation,psp,dolphin,*cancelled,qBound(2,offered["players"].toInt(2),4));
        if(*cancelled)return Result{};
        const QFileInfo after(record.contentPath);
        if(size!=after.size()||modified!=after.lastModified()||!after.isReadable())return Result{{},"The file changed while checking. Choose it again."};
        if(profile.isEmpty())return Result{{},"This file or the installed emulator cannot use the lobby's multiplayer mode."};
        if(remote ? profile!=offered : (!supportsMultiplayerSession(profile,offered)||profile["id"]!=wanted["mode"]||!multiplayerConnectionAllowed(profile,"online")))
            return Result{{},"This file or emulator differs from the lobby's edition. Your library was not changed."};
        return Result{profile,{}};
    }));
}
}
