#include "DolphinLan.h"
#include "DolphinNetplay.h"
#include "NetplayCache.h"
#include "core/model/GamePlayers.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace trainer::dolphinlan {
namespace {
constexpr auto Settings="dolphin-bba-tapserver-isolated-v1";
constexpr auto Revision="c77bbaa0f372c3f72281602a8b087206706542cb";
bool hex(const QString& s){return QRegularExpression("^[a-f0-9]{64}$").match(s).hasMatch();}
QJsonObject manifest(const QString& path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>8192)return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool write(const QString& path,const QByteArray& bytes) {
    QSaveFile file(path);return file.open(QIODevice::WriteOnly)&&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
        &&file.write(bytes)==bytes.size()&&file.commit();
}
}
QString bundleRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/dolphin-lan";}
bool supportedDisc(const QString& id) {
    // Native LAN families, independent of region/revision. These are not a
    // NetPlay allowlist. Full content must still match on both peers.
    return QRegularExpression("^(GM4|GKY|GTE)[A-Z0-9]01$").match(id).hasMatch();
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const StandaloneInstallation& i,const std::atomic_bool& cancel) {
    if(cancel||r.adventure.collectionOnly||r.adventure.platformId!="gc"||r.adventure.adapterId!="dolphin"||!i.platforms.contains("gc")
       ||!QStringList{"iso","gcm","rvz","wia","gcz","ciso"}.contains(QFileInfo(r.contentPath).suffix().toLower()))return {};
    const auto m=manifest(dolphin::bridgeRoot()+"/manifest.json"),lan=manifest(bundleRoot()+"/manifest.json");
    const auto helper=bundleRoot()+"/stream_transport",tool=dolphin::bridgeRoot()+"/bin/dolphin-tool";
    if(m["protocol"]!=1||m["source"]!=Revision||m["bba"]!="tapserver-eof-v1"||lan["protocol"]!=1||lan["settings"]!=Settings
       ||!QFileInfo(helper).isExecutable()||!hex(lan["sha256"].toString())
       ||retroarch::fileDigest(helper,64LL*1024*1024,cancel)!=lan["sha256"].toString()
       ||!hex(m["toolSha256"].toString())||retroarch::fileDigest(tool,128LL*1024*1024,cancel)!=m["toolSha256"].toString())return {};
    const auto runtime=retroarch::fileDigest(dolphin::bridgeFile(),256LL*1024*1024,cancel);
    if(!hex(runtime)||runtime!=m["sha256"].toString())return {};
    QProcess probe;probe.setStandardErrorFile(QProcess::nullDevice());probe.start(tool,{"header","-j","-i",r.contentPath});
    if(!probe.waitForStarted(1500))return {};
    QElapsedTimer timer;timer.start();
    while(!probe.waitForFinished(50))if(cancel||timer.elapsed()>10000||probe.bytesAvailable()>8192){probe.kill();probe.waitForFinished(1000);return {};}
    const auto output=probe.readAllStandardOutput();const auto header=QJsonDocument::fromJson(output).object();
    if(cancel||probe.exitCode()!=0||probe.exitStatus()!=QProcess::NormalExit||output.size()>8192
       ||header["platform"]!="gc"||!supportedDisc(header["game_id"].toString()))return {};
    const auto content=retroarch::fileDigest(r.contentPath,1500LL*1024*1024,cancel);
    if(!hex(content))return {};
    return {{"id","runtime.dolphin.bba.v1"},{"label",r.adventure.title.left(96)},{"content",content},{"runtime",runtime},
        {"settings",Settings},{"disc",header["game_id"]},{"players",2},{"lateJoin",false},{"temporaryProgress",true}};
}
QString configure(ProcessCommand& cmd,const StandaloneInstallation& i,QString content,QString connection,bool host,const QJsonObject& expected) {
    QFile file(connection);QFileInfo info(connection);
    if(!info.isAbsolute()||info.isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>4096)return "Couldn't protect the GameCube LAN connection.";
#ifndef Q_OS_WIN
    if(info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther))return "Couldn't protect the GameCube LAN connection.";
#endif
    QMap<QByteArray,QByteArray> values;
    for(const auto& line:file.readAll().split('\n')) {
        if(line.isEmpty())continue;
        const int at=line.indexOf('=');
        if(at<1||line.contains('\r')||values.contains(line.left(at)))return "Invalid GameCube LAN connection.";
        values.insert(line.left(at),line.mid(at+1));
    }
    const auto socket=QString::fromUtf8(values["socket"]),mac=QString::fromUtf8(values["mac"]);
    if(values.size()!=4||values["mode"]!="bba"||values["host"]!=(host?"true":"false")
       ||socket!=info.absolutePath()+"/bba.sock"||socket.toUtf8().size()>100
       ||!QRegularExpression(host?"^00:09:bf:[0-9a-f]{2}:[0-9a-f]{2}:01$":"^00:09:bf:[0-9a-f]{2}:[0-9a-f]{2}:02$").match(mac).hasMatch())return "Invalid GameCube LAN connection.";
    auto dir=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/traineros-bba-XXXXXX");
    if(!dir->isValid()||!QDir().mkpath(dir->path()+"/Config"))return "Couldn't prepare GameCube LAN.";
    const auto root=dir->path();
    const auto input=i.configFile.isEmpty()?QDir::homePath()+"/.var/app/org.DolphinEmu.dolphin-emu/config/dolphin-emu/GCPadNew.ini":QFileInfo(i.configFile).dir().filePath("GCPadNew.ini");
    if(QFileInfo(input).size()>1024*1024||!QFile::copy(input,root+"/Config/GCPadNew.ini"))return "Couldn't read your GameCube controller settings.";
    if(!prepareNetplayCache(root+"/Cache/Shaders","dolphin",expected["runtime"].toString()))return "Couldn't prepare the graphics cache.";
    const QByteArray config="[Interface]\nConfirmStop = False\nOnScreenDisplayMessages = False\n[Display]\nFullscreen = True\nRenderToMain = False\n"
        "[Core]\nCPUThread = True\nDSPHLE = True\nGFXBackend = Vulkan\nEnableCheats = False\nSkipIPL = True\n"
        "SIDevice0 = 6\nSIDevice1 = 0\nSIDevice2 = 0\nSIDevice3 = 0\nSlotA = 8\nSlotB = 255\nSerialPort1 = 11\n"
        "BBA_MAC = "+mac.toUtf8()+"\nBBA_TAPSERVER_DESTINATION = "+socket.toUtf8()+"\n"
        "[Analytics]\nPermissionAsked = True\nEnabled = False\n";
    if(!write(root+"/Config/Dolphin.ini",config)||!write(root+"/Config/GFX.ini","[Settings]\nInternalResolution = 1\nAspectRatio = 0\n"))return "Couldn't prepare GameCube LAN settings.";
    // No NetPlay session environment: each process boots its own console and P1.
    cmd.program="/usr/bin/env";cmd.arguments={"-u","TRAINEROS_DOLPHIN_SESSION"};
    const int separator=i.prefixArguments.indexOf("--");
    if(separator>=0){cmd.arguments<<i.program;cmd.arguments+=i.prefixArguments.mid(0,separator+1);}
    cmd.arguments<<dolphin::bridgeFile()<<"-b"<<"-C"<<"Dolphin.Interface.ConfirmStop=False"<<"-u"<<root<<"-e"<<content;
    cmd.runtimeControls={{"kind","dolphin"},{"netplay",expected.toVariantMap()},{"netplayHost",host},{"temporaryProgress",true}};
    const auto settled=cmd.settled;cmd.settled=[settled,dir](const ProcessOutcome& out){if(settled)settled(out);dir->remove();};
    return {};
}
ProcessCommand command(const AdventureRegistration& r,const StandaloneInstallation& i,const QJsonObject& expected,QString connection,bool host) {
    ProcessCommand cmd;cmd.runtimeControls={{"kind","dolphin"},{"game",r.adventure.id},{"netplay",expected.toVariantMap()},{"temporaryProgress",true}};
    cmd.prepare=[r,i,expected,connection,host](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!sameMultiplayerGame(netplayIdentity(r,i,cancel),expected))return "Your GameCube game or LAN setup changed. Choose the game again.";
        const auto error=configure(out,i,r.contentPath,connection,host,expected);out.runtimeControls["game"]=r.adventure.id;return error;
    };return cmd;
}
}
