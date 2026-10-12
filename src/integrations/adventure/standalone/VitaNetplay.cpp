#include "VitaNetplay.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace trainer::vita {
namespace {
bool hex(const QString& value,int size=64) {return value.size()==size&&QRegularExpression("^[a-f0-9]+$").match(value).hasMatch();}
bool endpoint(const QJsonObject& value) {
    if(value["mode"]=="online")return value.size()==2&&value["code"].toString().size()<=160
        &&QRegularExpression("^[0-9]+(?:-[a-z]+){8}$").match(value["code"].toString()).hasMatch();
    const int port=value["port"].toInt();
    return value.size()==4&&value["mode"]=="nearby"&&port>0&&port<=65535&&value["port"]==QJsonValue(port)
        &&hex(value["fingerprint"].toString())&&hex(value["token"].toString(),32);
}
bool helperVerified(const std::atomic_bool& cancel) {
    const auto root=bundleRoot(),canonical=QFileInfo(root).canonicalFilePath();
    QFile file(root+"/manifest.json");
    if(canonical.isEmpty()||QFileInfo(file.fileName()).isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>262144)return false;
    const auto value=QJsonDocument::fromJson(file.readAll()).object();
    if(value["protocol"]!=1||value["source"]!="46602447c7f84d53790b44e1869ab5105178bf16"||value["settings"]!="native-lan-two-owned-v1")return false;
    const auto files=value["files"].toObject();
    // Verify executable preparation code before invoking it. It verifies the
    // complete native bundle and effective installed game before admission.
    for(const auto& name:QStringList{"transport/launcher.py","transport/game.py","transport/native_log.py","transport/profiles.json","transport/requirements.lock","transport/venv/bin/python"}) {
        const auto path=root+'/'+name;
        const auto expected=name.endsWith("/python")?value["interpreterSha256"].toString():files[name].toString();
        if(!hex(expected)||(!name.endsWith("/python")&&!QFileInfo(path).canonicalFilePath().startsWith(canonical+'/')))return false;
        QFile source(path);if(!source.open(QIODevice::ReadOnly)||source.size()>64*1024*1024)return false;
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while(!source.atEnd()){if(cancel)return false;const auto bytes=source.read(1024*1024);if(bytes.isEmpty())return false;hash.addData(bytes);}
        if(source.error()!=QFile::NoError||QString::fromLatin1(hash.result().toHex())!=expected)return false;
    }
    return QFileInfo(root+"/transport/venv/bin/python").isExecutable();
}
QJsonObject helper(const QString& mode,const QString& target,const std::atomic_bool& cancel) {
    if(!helperVerified(cancel))return {};
    QProcess process;
    process.start(bundleRoot()+"/transport/venv/bin/python",{bundleRoot()+"/transport/launcher.py",mode,target});
    if(!process.waitForStarted(3000))return {};
    QElapsedTimer timer;timer.start();QByteArray output;
    while(process.state()!=QProcess::NotRunning) {
        process.waitForFinished(50);output+=process.readAllStandardOutput();process.readAllStandardError();
        if(cancel||timer.elapsed()>120000||output.size()>8192){process.kill();process.waitForFinished(3000);return {};}
    }
    output+=process.readAllStandardOutput();
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0||output.size()>8192)return {};
    return QJsonDocument::fromJson(output).object();
}
bool writePrivate(const QString& path,const QByteArray& bytes) {
    QFile file(path);return file.open(QIODevice::WriteOnly|QIODevice::NewOnly)
        &&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)&&file.write(bytes)==bytes.size()&&file.flush();
}
}
QString bundleRoot() {return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/vita3k-netplay";}
QJsonObject netplayIdentity(const AdventureRegistration& record,const std::atomic_bool& cancel) {
    if(record.adventure.platformId!="psvita"||record.adventure.adapterId!="vita3k"||record.adventure.collectionOnly
       ||QFileInfo(record.contentPath).suffix().toLower()!="psvita")return {};
    auto value=helper("identity",record.contentPath,cancel);
    if(value["id"]!="runtime.vita3k.lan.v1"||!hex(value["content"].toString())||!hex(value["runtime"].toString())
       ||value["settings"]!="native-lan-two-owned-v1"||value["players"]!=2||value["lateJoin"]!=false||value["temporaryProgress"]!=false)return {};
    value["label"]=record.adventure.title.left(96);return value;
}
ProcessCommand command(const AdventureRegistration& record,const Request& request,const QStringList& inputPrefix) {
    ProcessCommand result;result.runtimeControls={{"kind","vita3k"},{"game",record.adventure.id},
        {"netplay",request.expected.toVariantMap()},{"temporaryProgress",false}};
    result.prepare=[record,request,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!hex(request.identity)||(!request.host&&(!endpoint(request.endpoint)||request.endpoint["mode"]!=(request.online?"online":"nearby")))
           ||(!request.online&&QHostAddress(request.address).protocol()!=QAbstractSocket::IPv4Protocol))return "This Vita invitation is invalid.";
        if(!sameMultiplayerGame(netplayIdentity(record,cancel),request.expected))return "Your Vita game or its play setup changed. Invite again.";
        auto directory=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/traineros-vita-XXXXXX");
        if(!directory->isValid())return "Couldn't prepare the Vita session.";
        const auto path=QFileInfo(directory->path()).canonicalFilePath();
        QJsonObject channel=request.host?QJsonObject{}:request.endpoint;
        channel["host"]=request.host;channel["identity"]=request.identity;channel["mode"]=request.online?"online":"nearby";
        if(!request.online)channel["address"]=request.address;
        const QJsonObject config{{"link",record.contentPath},{"expected",request.expected},{"channel",channel}};
        if(!writePrivate(path+"/request.json",QJsonDocument(config).toJson(QJsonDocument::Compact)))return "Couldn't protect the Vita connection details.";
        if(helper("prepare",path+"/request.json",cancel)["ready"]!=true)return "Your Vita game or its play setup is unavailable. Check the installation and try again.";
        out.finalize=[directory](const ProcessOutcome&){return QString();};
        out.program="/usr/bin/env";out.arguments={"QT_QPA_PLATFORM=xcb","PYTHONDONTWRITEBYTECODE=1"};out.arguments+=inputPrefix;
        out.arguments<<bundleRoot()+"/transport/venv/bin/python"<<bundleRoot()+"/transport/network-session.py"<<path+"/network.json";
        out.workingDirectory=path;return {};
    };
    return result;
}
QJsonObject event(const QByteArray& bytes) {
    const auto line=bytes.trimmed();constexpr auto prefix="TRAINEROS_VITA ";
    if(!line.startsWith(prefix)||line.size()>2048)return {};
    const auto decoded=QByteArray::fromBase64Encoding(line.mid(qstrlen(prefix)),QByteArray::AbortOnBase64DecodingErrors);if(!decoded)return {};
    const auto value=QJsonDocument::fromJson(decoded.decoded).object();
    if(value.size()==1&&(value["event"]=="running"||value["event"]=="error"))return value;
    return value.size()==2&&value["event"]=="ready"&&endpoint(value["endpoint"].toObject())?value:QJsonObject{};
}
}
