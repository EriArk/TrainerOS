#pragma once
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace trainer::retroarch::handheld {
inline QByteArray readFile(const QString& path) {
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return {};
    return f.readAll();
}
inline bool writeFile(const QString& path,const QByteArray& bytes) {
    QSaveFile f(path);
    return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();
}
inline QString rtcPath(const QString& save) {
    return save.left(save.size()-4)+".rtc";
}
// A durable intent joins the two individually atomic battery-file replacements.
// Recovery is also run before ordinary launch, not just on the next link session.
// Never replay over an independently modified file or follow a symlink.
inline QString recoverPair(const QString& save) {
    const auto intent=save+".link-return.json";
    if(!QFileInfo::exists(intent))return {};
    const auto failure=QString("Couldn't finish returning the multiplayer save. Its recovery files were kept beside your save.");
    if(QFileInfo(intent).isSymLink())return failure;
    const auto j=QJsonDocument::fromJson(readFile(intent)).object();
    if(j["version"].toInt()!=1)return failure;
    const auto rtc=rtcPath(save);
    const auto decode=[&](const char* key){return QByteArray::fromBase64(j[key].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);};
    const auto oldSave=decode("oldSave"),newSave=decode("newSave"),oldRtc=decode("oldRtc"),newRtc=decode("newRtc");
    if(newSave.isEmpty()||newSave.size()>131072||newRtc.size()!=8||
       oldSave.size()>131072||(!oldRtc.isEmpty()&&oldRtc.size()!=8)||
       j["saveExisted"].toBool()!=!oldSave.isEmpty()||j["rtcExisted"].toBool()!=!oldRtc.isEmpty())return failure;
    const auto expected=[](const QString& p,const QByteArray& before,const QByteArray& after,bool existed){
        if(QFileInfo(p).isSymLink())return false;
        if(!QFileInfo::exists(p))return !existed;
        const auto bytes=readFile(p);return bytes==after||(existed&&bytes==before);
    };
    if(!expected(save,oldSave,newSave,j["saveExisted"].toBool())||
       !expected(rtc,oldRtc,newRtc,j["rtcExisted"].toBool()))return failure;
    if(readFile(rtc)!=newRtc&&!writeFile(rtc,newRtc))return failure;
    if(readFile(save)!=newSave&&!writeFile(save,newSave))return failure;
    return QFile::remove(intent)?QString():failure;
}
inline QString returnPair(const QString& save,const QByteArray& oldSave,const QByteArray& newSave,
                          const QByteArray& oldRtc,const QByteArray& newRtc) {
    const auto intent=save+".link-return.json";
    if(QFileInfo::exists(intent)||QFileInfo(intent).isSymLink())return "A previous multiplayer save still needs recovery.";
    if(QFileInfo(save+".before-link").isSymLink()||QFileInfo(rtcPath(save)+".before-link").isSymLink())
        return "Couldn't safely back up your game save and clock.";
    if((!oldSave.isEmpty()&&!writeFile(save+".before-link",oldSave))||
       (!oldRtc.isEmpty()&&!writeFile(rtcPath(save)+".before-link",oldRtc)))return "Couldn't back up your game save and clock.";
    const auto encode=[](const QByteArray& b){return QString::fromLatin1(b.toBase64());};
    const QJsonObject j{{"version",1},{"saveExisted",!oldSave.isEmpty()},{"rtcExisted",!oldRtc.isEmpty()},
        {"oldSave",encode(oldSave)},{"newSave",encode(newSave)},
        {"oldRtc",encode(oldRtc)},{"newRtc",encode(newRtc)}};
    if(!writeFile(intent,QJsonDocument(j).toJson(QJsonDocument::Compact)))return "Couldn't keep the multiplayer save and clock for recovery.";
    return recoverPair(save);
}
}
