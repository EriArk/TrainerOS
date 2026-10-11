#include "FlycastNetplay.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

namespace trainer::flycast {
namespace {
bool hex(const QString& s,int length){return s.size()==length&&QRegularExpression("^[a-f0-9]+$").match(s).hasMatch();}
QString digest(const QString& path,const std::atomic_bool& cancel,qint64 limit) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()<=0||f.size()>limit)return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    while(!f.atEnd()){if(cancel)return {};const auto b=f.read(1024*1024);if(b.isEmpty())return {};h.addData(b);}
    return QString::fromLatin1(h.result().toHex());
}
bool write(const QString& path,const QByteArray& bytes){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
bool relayUrl(const QUrl& u) {
    return u.isValid()&&u.scheme()=="udp"&&!u.host().isEmpty()&&u.userInfo().isEmpty()
        &&u.path().isEmpty()&&u.query().isEmpty()&&u.fragment().isEmpty()&&u.port()>0&&u.port()<=65535;
}
bool key(const QString& s){return QRegularExpression("^[A-Za-z0-9]{6}$").match(s).hasMatch()&&s!="NOKEY"&&s!="MAXCN";}
}
QString bundleRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/flycast-netplay";}
QJsonObject bundle(const std::atomic_bool& cancel) {
    const auto root=bundleRoot();QFile file(root+"/manifest.json");
    if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return {};
    auto m=QJsonDocument::fromJson(file.readAll()).object();
    if(m["protocol"]!=1||m["source"]!=sourceRevision||!relayUrl(QUrl(m["onlineRelay"].toString()))
       ||(m["renderer"]!="vulkan"&&m["renderer"]!="opengl"))return {};
    const auto files=m["files"].toObject();
    if(files.size()!=3||!files.contains("bin/flycast")||!files.contains("firmware/dc_boot.bin")||!files.contains("firmware/dc_flash.bin"))return {};
    if(QFileInfo(root+"/firmware/dc_boot.bin").size()!=2*1024*1024||QFileInfo(root+"/firmware/dc_flash.bin").size()!=128*1024)return {};
    QCryptographicHash h(QCryptographicHash::Sha256);const auto canonical=QFileInfo(root).canonicalFilePath()+"/";
    for(auto it=files.begin();it!=files.end();++it) {
        const auto path=root+"/"+it.key();
        if(!QFileInfo(path).canonicalFilePath().startsWith(canonical)||!hex(it.value().toString(),64)
           ||digest(path,cancel,256*1024*1024)!=it.value().toString())return {};
        h.addData(it.key().toUtf8());h.addData(QByteArray(1,'\0'));h.addData(it.value().toString().toLatin1());
    }
#ifdef Q_OS_UNIX
    if(!QFileInfo(root+"/bin/flycast").isExecutable())return {};
#endif
    h.addData(m["onlineRelay"].toString().toUtf8());
    m["digest"]=QString::fromLatin1(h.result().toHex());return m;
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const std::atomic_bool& cancel) {
    if(r.adventure.platformId!="dreamcast"||r.adventure.adapterId!="retroarch"
       ||QFileInfo(r.contentPath).suffix().toLower()!="chd")return {};
    QFile f(r.contentPath);if(!f.open(QIODevice::ReadOnly)||f.size()<124)return {};
    const auto header=f.read(16);
    // CHD v5 header only admits this container. Native disc validation still
    // belongs to Flycast; a header or catalogue title is not compatibility proof.
    if(header!=QByteArray::fromHex("4d436f6d707248440000007c00000005"))return {};
    const auto m=bundle(cancel);if(m.isEmpty())return {};
    const auto hash=digest(r.contentPath,cancel,2LL*1024*1024*1024);if(hash.isEmpty())return {};
    return {{"id","runtime.flycast-dojo.dc.v1"},{"label",r.adventure.title.left(96)},
        {"content",hash},{"runtime",m["digest"]},{"settings","dojo-v1-fresh-vmu-analog2-"+m["renderer"].toString()},
        {"players",2},{"lateJoin",false}};
}
ProcessCommand command(const AdventureRegistration& r,const Request& request,const QStringList& inputPrefix) {
    ProcessCommand cmd;cmd.runtimeControls={{"kind","flycast"},{"game",r.adventure.id},
        {"netplay",request.expected.toVariantMap()},{"temporaryProgress",true}};
    cmd.prepare=[r,request,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!hex(request.token,32)||(!request.host&&request.online&&!key(request.key))
           ||(!request.online&&QHostAddress(request.address).protocol()!=QAbstractSocket::IPv4Protocol))
            return "This Dreamcast invitation is invalid.";
        if(!sameMultiplayerGame(netplayIdentity(r,cancel),request.expected))return "Your Dreamcast game or emulator changed. Invite again.";
        const auto m=bundle(cancel);if(m.isEmpty())return "The Dreamcast setup changed. Invite again.";
        const QUrl relay(m["onlineRelay"].toString());
        auto dir=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/trainer-flycast-XXXXXX");
        if(!dir->isValid()||!QDir().mkpath(dir->path()+"/data"))return "Couldn't prepare temporary Dreamcast progress.";
        const auto root=dir->path();
        for(const auto name:{"dc_boot.bin","dc_flash.bin"}) {
            const auto target=root+"/data/"+name;
            if(!QFile::copy(bundleRoot()+"/firmware/"+name,target)
               ||digest(target,cancel,2*1024*1024)!=m["files"].toObject()[QString("firmware/")+name])
                return "Couldn't prepare the Dreamcast firmware copy.";
        }
        const QString cfg=QString("[config]\nDreamcast.AutoLoadState = no\nDreamcast.AutoSaveState = no\n"
            "Dreamcast.Region = 1\nDreamcast.Language = 1\nDreamcast.Broadcast = 0\nDreamcast.Cable = 3\n"
            "pvr.rend = %1\nFetchBoxart = no\n[window]\nfullscreen = yes\n"
            "[network]\nGGPO = yes\nGGPOAnalogAxes = 2\nGGPODelay = 2\nEnableUPnP = no\n"
            "GGPOPort = 19713\nGGPORemotePort = %2\nActAsServer = %3\nserver = %4\n"
            "[dojo]\nEnable = no\nRelay = %5\nRelayKey = %6\nDefaultRelayPort = %2\n"
            "EnableMatchCode = no\nHostJoinSelect = no\nIgnoreNetSave = yes\nNetCustomVmu = no\n"
            "EnableMemRestore = no\nCopyMissingSharedMem = no\nRecordMatches = no\nTransmitting = no\n"
            "Receiving = no\nOutputStreamTxt = no\nTrainerSession = %7\n")
            .arg(m["renderer"]=="vulkan"?4:0).arg(request.online?relay.port():19713)
            .arg(request.host?"yes":"no",request.online?relay.host():request.address,
                 request.online?"yes":"no",request.host?QString():request.key,request.token);
        if(!write(root+"/emu.cfg",cfg.toUtf8()))return "Couldn't prepare Dreamcast play.";
        out.program="/usr/bin/env";out.workingDirectory=root;
        out.arguments={"FLYCAST_ROOT="+root,"XDG_CONFIG_HOME="+root,"XDG_DATA_HOME="+root,
                       "XDG_CONFIG_DIRS="+root,"XDG_DATA_DIRS="+root};
        out.arguments+=inputPrefix;out.arguments<<bundleRoot()+"/bin/flycast"<<r.contentPath;
        out.settled=[dir](const ProcessOutcome&){};return {};
    };return cmd;
}
QJsonObject event(const QByteArray& bytes) {
    const auto line=bytes.trimmed();constexpr auto prefix="[TrainerOSFlycast] ";
    if(line.size()>128||!line.startsWith(prefix))return {};
    const auto value=line.mid(qstrlen(prefix));
    if(value=="running"||value=="error"||value=="ready")return {{"event",QString::fromLatin1(value)}};
    if(value.startsWith("relay ")&&key(QString::fromLatin1(value.mid(6))))
        return {{"event","ready"},{"key",QString::fromLatin1(value.mid(6))}};
    return {};
}
}
