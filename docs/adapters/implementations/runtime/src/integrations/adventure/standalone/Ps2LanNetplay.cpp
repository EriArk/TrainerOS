#include "Ps2LanNetplay.h"
#include "core/model/GamePlayers.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace trainer::ps2lan {
namespace {
bool hex(const QString& value){return QRegularExpression("^[a-f0-9]{64}$").match(value).hasMatch();}
bool verified(const std::atomic_bool& cancel) {
    const auto root=bundleRoot(),canonical=QFileInfo(root).canonicalFilePath();
    QFile file(root+"/manifest.json");
    if(canonical.isEmpty()||!file.open(QIODevice::ReadOnly)||file.size()>65536)return false;
    const auto manifest=QJsonDocument::fromJson(file.readAll()).object();
    if(manifest["protocol"]!=1||manifest["settings"]!="eclx1-namespace-own-cards-v1")return false;
    const auto files=manifest["files"].toObject();
    for(const auto& name:{"launcher.py","disc.py","session_config.py","profiles.json","ps2_endpoint","libchdr.so","stream_transport"}) {
        const auto path=root+'/'+name,hash=files[name].toString();
        if(!hex(hash)||!QFileInfo(path).canonicalFilePath().startsWith(canonical+'/')
           ||retroarch::fileDigest(path,64LL*1024*1024,cancel)!=hash)return false;
    }
    return QFileInfo("/usr/bin/bwrap").isExecutable()&&QFileInfo(root+"/ps2_endpoint").isExecutable()
        &&QFileInfo(root+"/stream_transport").isExecutable();
}
QJsonObject helper(QString mode,QString path,const std::atomic_bool& cancel) {
    if(!verified(cancel))return {};
    QProcess process;auto env=QProcessEnvironment::systemEnvironment();env.insert("PYTHONDONTWRITEBYTECODE","1");process.setProcessEnvironment(env);
    process.start("/usr/bin/python3",{bundleRoot()+"/launcher.py",mode,path});
    if(!process.waitForStarted(3000))return {};
    QElapsedTimer timer;timer.start();QByteArray output;
    while(process.state()!=QProcess::NotRunning) {
        process.waitForFinished(50);output+=process.readAllStandardOutput();process.readAllStandardError();
        if(cancel||timer.elapsed()>120000||output.size()>8192){process.kill();process.waitForFinished(2000);return {};}
    }
    output+=process.readAllStandardOutput();
    if(process.exitCode()!=0||process.exitStatus()!=QProcess::NormalExit||output.size()>8192)return {};
    return QJsonDocument::fromJson(output).object();
}
}
QString bundleRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/ps2-lan";}
QJsonObject netplayIdentity(const AdventureRegistration& record,const std::atomic_bool& cancel) {
    if(record.adventure.platformId!="ps2"||record.adventure.adapterId!="armsx2"||record.adventure.collectionOnly
       ||!QStringList{"chd","iso"}.contains(QFileInfo(record.contentPath).suffix().toLower()))return {};
    auto result=helper("identity",record.contentPath,cancel);
    if(result["id"]!="runtime.armsx2.lan.v1"||!hex(result["content"].toString())||!hex(result["runtime"].toString())
       ||result["settings"]!="eclx1-namespace-own-cards-v1"||result["players"]!=2
       ||result["lateJoin"]!=false||result["temporaryProgress"]!=false)return {};
    result["label"]=record.adventure.title.left(96);return result;
}
ProcessCommand command(const AdventureRegistration& record,const QJsonObject& expected,QString connection,bool host,const QStringList& inputPrefix) {
    ProcessCommand result;result.runtimeControls={{"kind","armsx2"},{"game",record.adventure.id},{"netplay",expected.toVariantMap()},{"temporaryProgress",false}};
    result.prepare=[record,expected,connection,host,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!sameMultiplayerGame(netplayIdentity(record,cancel),expected))return "Your PS2 game or its LAN setup changed. Choose the game again.";
        auto dir=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/traineros-ps2-XXXXXX");
        if(!dir->isValid())return "Couldn't prepare this PS2 session.";
        const auto path=dir->path()+"/request.json";
        const auto bytes=QJsonDocument(QJsonObject{{"content",record.contentPath},{"expected",expected},{"connection",connection},{"host",host}}).toJson(QJsonDocument::Compact);
        QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
           ||file.write(bytes)!=bytes.size()||!file.flush())return "Couldn't protect the PS2 connection details.";
        file.close();
        if(helper("prepare",path,cancel)["ready"]!=true)return "Couldn't prepare the PS2 LAN session. Your existing settings and saves were kept.";
        out.finalize=[dir](const ProcessOutcome&){return QString();};
        out.program="/usr/bin/env";out.arguments={"QT_QPA_PLATFORM=xcb","PYTHONDONTWRITEBYTECODE=1"};out.arguments+=inputPrefix;
        out.arguments<<"/usr/bin/python3"<<bundleRoot()+"/launcher.py"<<"run"<<path;
        out.workingDirectory=dir->path();return {};
    };
    return result;
}
}
