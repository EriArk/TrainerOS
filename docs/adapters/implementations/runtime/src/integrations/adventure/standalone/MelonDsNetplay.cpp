#include "MelonDsNetplay.h"
#include "integrations/adventure/CartridgeContent.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace trainer::melonds {
namespace {
bool hex(const QString& value,int size=64) {return value.size()==size&&QRegularExpression("^[a-f0-9]+$").match(value).hasMatch();}
QString digest(const QString& path,const std::atomic_bool& cancel,qint64 limit) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()<1||file.size()>limit)return {};
    const auto size=file.size();qint64 read=0;QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!file.atEnd()) {if(cancel)return {};const auto bytes=file.read(1024*1024);if(bytes.isEmpty())return {};read+=bytes.size();hash.addData(bytes);}
    return file.error()==QFile::NoError&&read==size?QString::fromLatin1(hash.result().toHex()):QString();
}
bool endpoint(const QJsonObject& value) {
    if(value["mode"]=="online")return value.size()==2&&value["code"].toString().size()<=160
        &&QRegularExpression("^[0-9]+(?:-[a-z]+){8}$").match(value["code"].toString()).hasMatch();
    const int port=value["port"].toInt();
    return value.size()==4&&value["mode"]=="nearby"&&port>0&&port<=65535&&value["port"]==QJsonValue(port)
        &&hex(value["fingerprint"].toString())&&hex(value["token"].toString(),32);
}
}
QString bundleRoot() {return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/melonds-netplay";}
QJsonObject bundle(const std::atomic_bool& cancel) {
    const auto root=bundleRoot(),canonical=QFileInfo(root).canonicalFilePath();
    QFile file(root+"/manifest.json");
    if(canonical.isEmpty()||QFileInfo(file.fileName()).isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>16384)return {};
    auto value=QJsonDocument::fromJson(file.readAll()).object();
    if(value["protocol"]!=1||value["source"]!=sourceRevision||value["settings"]!="detmp-two-owned-v1")return {};
    const auto files=value["files"].toObject();
    const QStringList expected{"bin/traineros-ds","transport/secure-channel.py","transport/requirements.lock"};
    if(files.size()!=expected.size())return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for(const auto& name:expected) {
        const auto path=root+"/"+name;
        if(!files.contains(name)||!QFileInfo(path).canonicalFilePath().startsWith(canonical+'/')
           ||!hex(files[name].toString())||digest(path,cancel,256*1024*1024)!=files[name].toString())return {};
        hash.addData(name.toUtf8());hash.addData(QByteArray(1,'\0'));hash.addData(files[name].toString().toLatin1());
    }
    const auto python=root+"/transport/venv/bin/python";
    if(!QFileInfo(root+"/bin/traineros-ds").isExecutable()||!QFileInfo(python).isExecutable()
       ||!hex(value["interpreterSha256"].toString())||digest(python,cancel,64*1024*1024)!=value["interpreterSha256"].toString())return {};
    hash.addData(value["interpreterSha256"].toString().toLatin1());
    hash.addData(value["settings"].toString().toLatin1());
    value["digest"]=QString::fromLatin1(hash.result().toHex());return value;
}
QJsonObject netplayIdentity(const AdventureRegistration& record,const std::atomic_bool& cancel) {
    if(record.adventure.platformId!="nds"||record.adventure.adapterId!="melonds"||record.adventure.collectionOnly
       ||!QStringList{"nds","srl","zip","7z"}.contains(QFileInfo(record.contentPath).suffix().toLower()))return {};
    const auto installed=bundle(cancel);if(installed.isEmpty())return {};
    const auto image=cartridge::read(record.contentPath,"nds",cancel);
    if(image.bytes.size()<512)return {};
    const auto content=image.digest();if(content.isEmpty())return {};
    return {{"id","runtime.melonds.nds.v1"},{"label",record.adventure.title.left(96)},
        {"content",content},{"runtime",installed["digest"]},{"settings",installed["settings"]},
        {"players",2},{"lateJoin",false},{"privateProgress",true}};
}
ProcessCommand command(const AdventureRegistration& record,const Request& request,const QString& backupRoot,
                       const SaveTargetResolver& resolve,const QStringList& inputPrefix) {
    ProcessCommand result;result.runtimeControls={{"kind","melonds"},{"game",record.adventure.id},
        {"netplay",request.expected.toVariantMap()},{"temporaryProgress",false}};
    result.prepare=[record,request,backupRoot,resolve,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!hex(request.identity)||(!request.host&&(!endpoint(request.endpoint)
           ||request.endpoint["mode"]!=(request.online?"online":"nearby")))
           ||(!request.online&&QHostAddress(request.address).protocol()!=QAbstractSocket::IPv4Protocol))
            return "This DS invitation is invalid.";
        if(!sameMultiplayerGame(netplayIdentity(record,cancel),request.expected))return "Your DS game or emulator changed. Invite again.";
        const auto parent=QDir(backupRoot).filePath("runtime-sessions");
        if(QFileInfo(parent).isSymLink()||!QDir().mkpath(parent))return "Couldn't prepare private DS progress.";
        auto directory=std::make_shared<QTemporaryDir>(QDir(parent).filePath("ds-XXXXXX"));
        if(!directory->isValid())return "Couldn't prepare the DS session.";
        const auto path=QFileInfo(directory->path()).canonicalFilePath();
        const auto error=prepareOwnedSession(out,record,backupRoot,resolve,path,request.host?0:1,request.expected["content"].toString());
        if(!error.isEmpty())return error;
        QString content=record.contentPath;
        if(cartridge::archived(content)) {
            content=cartridge::stage(cartridge::read(content,"nds",cancel),content,path+"/content",request.expected["content"].toString());
            if(content.isEmpty())return "Couldn't prepare the DS cartridge from its archive.";
        }
        QJsonObject config=request.host?QJsonObject{}:request.endpoint;
        config["host"]=request.host;config["mode"]=request.online?"online":"nearby";config["identity"]=request.identity;
        if(!request.online)config["address"]=request.address;
        config["python"]=bundleRoot()+"/transport/venv/bin/python";config["helper"]=bundleRoot()+"/transport/secure-channel.py";
        QFile file(path+"/network.json");const auto bytes=QJsonDocument(config).toJson(QJsonDocument::Compact);
        if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
           ||file.write(bytes)!=bytes.size()||!file.flush())return "Couldn't protect the DS connection details.";
        const auto finalize=out.finalize;
        out.finalize=[directory,finalize](const ProcessOutcome& outcome) {
            const auto error=finalize(outcome);
            if(!error.isEmpty())directory->setAutoRemove(false);
            return error;
        };
        out.program="/usr/bin/env";out.arguments={"QT_QPA_PLATFORM=xcb"};out.arguments+=inputPrefix;
        out.arguments<<bundleRoot()+"/bin/traineros-ds"<<content<<"--fullscreen"<<"--seats"<<"2"
            <<"--prepared-session"<<path<<"--network-session"<<path+"/network.json";
        out.workingDirectory=path;return {};
    };
    return result;
}
QJsonObject event(const QByteArray& bytes) {
    const auto line=bytes.trimmed();
    if(line=="[TrainerOSDs] running")return {{"event","running"}};
    if(line=="[TrainerOSDs] error")return {{"event","error"}};
    constexpr auto prefix="TRAINEROS_DS_ENDPOINT ";
    if(!line.startsWith(prefix)||line.size()>1024)return {};
    const auto decoded=QByteArray::fromBase64Encoding(line.mid(qstrlen(prefix)),QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded)return {};
    const auto value=QJsonDocument::fromJson(decoded.decoded).object();
    return endpoint(value)?QJsonObject{{"event","ready"},{"endpoint",value}}:QJsonObject{};
}
}
