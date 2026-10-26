#include "FlycastNetplay.h"
#include "integrations/adventure/retroarch/RetroArchDisc.h"
#include "core/model/GamePlayers.h"
#include "platform/storage/RuntimeSaveStorage.h"
#include <QLockFile>
#include <QProcess>
#include <QElapsedTimer>
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
#include <QtEndian>

namespace trainer::flycast {
namespace {
bool hex(const QString& s,int length){return s.size()==length&&QRegularExpression("^[a-f0-9]+$").match(s).hasMatch();}
QString digest(const QString& path,const std::atomic_bool& cancel,qint64 limit) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()<=0||f.size()>limit)return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    while(!f.atEnd()){if(cancel)return {};const auto b=f.read(1024*1024);if(b.isEmpty())return {};h.addData(b);}
    return QString::fromLatin1(h.result().toHex());
}
QString discContent(const QString& path,const std::atomic_bool& cancel,QString* first=nullptr,bool nested=false,
                    const std::function<void(const QString&)>& observe={}) {
    if(observe)observe(path);
    const QFileInfo info(path);QFile file(path);
    if(cancel||!file.open(QIODevice::ReadOnly)||file.size()<=0||file.size()>2LL*1024*1024*1024)return {};
    const auto extension=info.suffix().toLower();
    if(first)*first=path;
    if(extension=="cue") {
        if(!retroarch::validateDiscContent(path,cancel,observe).isEmpty())return {};
        const auto bytes=file.readAll();QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(bytes);
        const QRegularExpression row(R"cue(^FILE\s+(?:"([^"]+)"|(\S+))\s+\S+\s*$)cue",QRegularExpression::CaseInsensitiveOption);
        for(const auto& line:QString::fromUtf8(bytes).split('\n')) {
            const auto m=row.match(line.trimmed());if(!m.hasMatch())continue;
            const auto name=m.captured(1).isEmpty()?m.captured(2):m.captured(1);
            const auto value=digest(info.dir().filePath(name),cancel,2LL*1024*1024*1024);
            if(value.isEmpty())return {};
            hash.addData(value.toLatin1());
        }
        return QString::fromLatin1(hash.result().toHex());
    }
    if(extension=="chd") {
        if(file.size()<124||file.read(16)!=QByteArray::fromHex("4d436f6d707248440000007c00000005"))return {};
        return digest(path,cancel,2LL*1024*1024*1024);
    }
    if(extension=="cdi") {
        if(file.size()<32||!file.seek(file.size()-8))return {};
        const auto footer=file.read(8);if(footer.size()!=8)return {};
        const auto version=qFromLittleEndian<quint32>(footer.constData()),offset=qFromLittleEndian<quint32>(footer.constData()+4);
        if(version<0x80000004||version>0x80000006||offset<8||offset>=file.size())return {};
        const auto start=version==0x80000006?file.size()-offset:qint64(offset);
        if(start>=file.size()-10||!file.seek(start))return {};
        const auto sessions=file.read(2);if(sessions.size()!=2||qFromLittleEndian<quint16>(sessions.constData())==0)return {};
        return digest(path,cancel,2LL*1024*1024*1024);
    }
    if(extension!="gdi"&&extension!="m3u")return {};
    if(file.size()>(extension=="gdi"?8192:65536)||(nested&&extension=="m3u"))return {};
    const auto bytes=file.readAll();
    if(bytes.contains('\0')||QString::fromUtf8(bytes).toUtf8()!=bytes)return {};
    QJsonArray parts;const auto lines=QString::fromUtf8(bytes).split('\n');
    if(extension=="m3u") {
        for(const auto& raw:lines) {
            const auto line=raw.trimmed();if(line.isEmpty()||line.startsWith('#'))continue;
            if(parts.size()>=16||line.size()>1024||line.contains(':')||line.contains('\\'))return {};
            QString launch;
            const auto value=discContent(info.dir().absoluteFilePath(line),cancel,&launch,true,observe);
            if(value.isEmpty())return {};
            if(parts.isEmpty()&&first)*first=launch;
            parts.append(value);
        }
    } else {
        bool ok=false;const int count=lines.value(0).trimmed().toInt(&ok);
        if(!ok||count<3||count>99)return {};
        const QRegularExpression row(R"gdi(^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(?:"([^"]+)"|(\S+))\s+(\d+)\s*$)gdi");
        quint64 previous=0;
        for(int n=1;n<lines.size();++n) {
            if(lines[n].trimmed().isEmpty())continue;
            const auto m=row.match(lines[n]);if(!m.hasMatch()||parts.size()>=count)return {};
            const auto track=m.captured(1).toUInt(&ok);if(!ok||track!=parts.size()+1)return {};
            const auto lba=m.captured(2).toULongLong(&ok);if(!ok||lba>0xffffff||(track>1&&lba<=previous))return {};previous=lba;
            const auto ctrl=m.captured(3).toUInt(&ok);if(!ok||ctrl>15)return {};
            const auto sector=m.captured(4).toUInt(&ok);if(!ok||(sector!=2048&&sector!=2352))return {};
            const auto name=m.captured(5).isEmpty()?m.captured(6):m.captured(5);
            if(QDir::isAbsolutePath(name)||name.contains(':')||name.contains('\\')||name.split('/').contains(".."))return {};
            const auto offset=m.captured(7).toULongLong(&ok);if(!ok||offset>0x7fffffff)return {};
            const auto target=info.dir().filePath(name);const QFileInfo data(target);
            if(observe)observe(target);
            if(!data.canonicalFilePath().startsWith(info.dir().canonicalPath()+'/')||offset>=quint64(data.size())||(quint64(data.size())-offset)%sector)return {};
            const auto value=digest(target,cancel,2LL*1024*1024*1024);if(value.isEmpty())return {};
            parts.append(QJsonArray{qint64(lba),int(ctrl),int(sector),qint64(offset),value});
        }
        if(parts.size()!=count)return {};
    }
    if(parts.isEmpty()||cancel)return {};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonObject{{extension+"-v1",parts}}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}
bool write(const QString& path,const QByteArray& bytes){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
bool relayUrl(const QUrl& u) {
    return u.isValid()&&u.scheme()=="udp"&&!u.host().isEmpty()&&u.userInfo().isEmpty()
        &&u.path().isEmpty()&&u.query().isEmpty()&&u.fragment().isEmpty()&&u.port()>0&&u.port()<=65535;
}
bool key(const QString& s){return QRegularExpression("^[A-Za-z0-9]{6}$").match(s).hasMatch()&&s!="NOKEY"&&s!="MAXCN";}
QJsonObject arcadeContent(const AdventureRegistration& r,const std::atomic_bool& cancel,
                          const QString& stage={},const QString& expected={}) {
    QTemporaryDir task(QDir::tempPath()+"/trainer-arcade-probe-XXXXXX");
    if(!task.isValid())return {};
    const QJsonObject request{{"source",r.contentPath},{"platform",r.adventure.platformId},
        {"bundle",bundleRoot()},{"stage",stage},{"expected",expected}};
    const auto file=task.filePath("request.json");
    if(!write(file,QJsonDocument(request).toJson(QJsonDocument::Compact)))return {};
    QProcess process;process.setProgram("/usr/bin/python3");
    process.setArguments({bundleRoot()+"/arcade.py",stage.isEmpty()?"inspect":"prepare",file});
    process.setStandardErrorFile(QProcess::nullDevice());process.start();
    if(!process.waitForStarted(3000))return {};
    QByteArray output;QElapsedTimer timer;timer.start();
    while(process.state()!=QProcess::NotRunning) {
        process.waitForFinished(50);output+=process.readAllStandardOutput();
        if(cancel||timer.elapsed()>600000||output.size()>1024*1024) {
            process.terminate();if(!process.waitForFinished(2000)){process.kill();process.waitForFinished(1000);}return {};
        }
    }
    output+=process.readAllStandardOutput();
    if(process.exitStatus()!=QProcess::NormalExit||output.size()>1024*1024)return {};
    auto value=QJsonDocument::fromJson(output).object();
    if(process.exitCode()!=0){value["error"]=value["error"].toString("Couldn't prepare the arcade set.");return value;}
    if(value["protocol"]!=1||!hex(value["content"].toString(),64)||value["platform"]!=r.adventure.platformId)return {};
    return value;
}
}
bool arcadePlatform(const QString& platform) {return platform=="naomi"||platform=="naomi2"||platform=="atomiswave";}
QString cabinetRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/flycast-lan";}
namespace {
QJsonObject cabinetBundle(const std::atomic_bool& cancel,const std::function<void(const QString&)>& observe={}) {
    const auto root=cabinetRoot();
    if(observe)for(const auto& name:{"manifest.json","bin/flycast","cabinet_endpoint","stream_transport"})observe(root+'/'+name);
    QFile f(root+"/manifest.json");if(!f.open(QIODevice::ReadOnly)||f.size()>8192)return {};
    auto m=QJsonDocument::fromJson(f.readAll()).object();const auto files=m["files"].toObject();
    if(m["protocol"]!=1||m["source"]!=sourceRevision||m["settings"]!="flycast-cabinet-private-v1"
       ||files.size()!=3||!QFileInfo("/usr/bin/bwrap").isExecutable())return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    for(const auto& name:{"bin/flycast","cabinet_endpoint","stream_transport"}) {
        const auto expected=files[name].toString();
        if(!hex(expected,64)||!QFileInfo(root+'/'+name).isExecutable()||digest(root+'/'+name,cancel,256LL*1024*1024)!=expected)return {};
        h.addData(expected.toLatin1());
    }
    m["digest"]=QString::fromLatin1(h.result().toHex());return m;
}
QStringList cabinetEnvironment(const QString& root) {
    return {"-u","TRAINEROS_FLYCAST_SESSION","-u","TRAINEROS_FLYCAST_INSPECT","FLYCAST_ROOT="+root,"XDG_CONFIG_HOME="+root,"XDG_DATA_HOME="+root,
            "XDG_CONFIG_DIRS="+root,"XDG_DATA_DIRS="+root};
}
bool cabinetConfig(const QString& root,int region,bool host,const QString& renderer,bool inspect=false) {
    return write(root+"/emu.cfg",QString("[config]\nDreamcast.AutoLoadState = no\nDreamcast.AutoSaveState = no\n"
        "Dreamcast.Region = %1\nDreamcast.Language = 1\naica.Volume = %6\npvr.rend = %2\nFetchBoxart = no\n"
        "[window]\nfullscreen = yes\n[network]\nGGPO = no\nEnable = yes\nTrainerCabinet = yes\nEnableUPnP = no\n"
        "ActAsServer = %3\nLocalPort = %4\nserver = 127.0.0.1:%5\n"
        "[dojo]\nEnable = no\nRelay = no\n")
        .arg(region).arg(renderer=="vulkan"?4:0).arg(host?"yes":"no").arg(host?37391:37392).arg(host?37392:37391)
        .arg(inspect?0:100).toUtf8());
}
}
QJsonObject cabinetIdentity(const AdventureRegistration& r,const std::atomic_bool& cancel,
                            const std::function<void(const QString&)>& observe) {
    if(!arcadePlatform(r.adventure.platformId))return {};
    const auto m=cabinetBundle(cancel,observe);if(m.isEmpty())return {};
    auto identity=netplayIdentity(r,cancel,observe);if(identity.isEmpty())return {};
    QTemporaryDir stage(QDir::tempPath()+"/trainer-cabinet-inspect-XXXXXX");if(!stage.isValid())return {};
    const auto content=arcadeContent(r,cancel,stage.path(),identity["content"].toString());
    if(content["entry"].toString().isEmpty()||!cabinetConfig(stage.path(),content["region"].toInt(),true,"opengl",true))return {};
    QStringList args{"--die-with-parent","--unshare-net","--bind","/","/","--","/usr/bin/env"};
    args+=cabinetEnvironment(stage.path());args<<"TRAINEROS_FLYCAST_INSPECT=1"<<cabinetRoot()+"/bin/flycast"<<content["entry"].toString();
    QProcess probe;probe.setWorkingDirectory(stage.path());probe.setStandardErrorFile(QProcess::nullDevice());
    probe.start("/usr/bin/bwrap",args);if(!probe.waitForStarted(3000))return {};
    QElapsedTimer timer;timer.start();QByteArray output;
    while(probe.state()!=QProcess::NotRunning) {
        probe.waitForFinished(50);output+=probe.readAllStandardOutput();
        if(cancel||timer.elapsed()>60000||output.size()>1024*1024){probe.kill();probe.waitForFinished(2000);return {};}
    }
    output+=probe.readAllStandardOutput();
    if(output.size()>1024*1024||probe.exitCode()!=0||probe.exitStatus()!=QProcess::NormalExit)return {};
    QJsonObject result;bool seen=false;const QByteArray marker="[TrainerOSArcade] ";
    for(const auto& line:output.split('\n'))if(line.startsWith(marker)) {
        if(seen)return {};
        seen=true;result=QJsonDocument::fromJson(line.mid(marker.size())).object();
    }
    if(result.size()!=2||result["protocol"]!=1||(result["link"]!="naomi"&&result["link"]!="serial"))return {};
    identity["id"]="runtime.flycast.cabinet.v1";
    identity["runtime"]=QString::fromLatin1(QCryptographicHash::hash((identity["runtime"].toString()+m["digest"].toString()).toLatin1(),QCryptographicHash::Sha256).toHex());
    identity["settings"]="flycast-cabinet-private-v1-"+r.adventure.platformId;
    identity["link"]=result["link"];identity["connections"]=QJsonArray{"nearby","online"};return identity;
}
ProcessCommand cabinetCommand(const AdventureRegistration& r,const QJsonObject& expected,QString connection,bool host,const QStringList& inputPrefix) {
    ProcessCommand cmd;cmd.runtimeControls={{"kind","flycast"},{"game",r.adventure.id},{"netplay",expected.toVariantMap()},{"temporaryProgress",true}};
    cmd.prepare=[r,expected,connection,host,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!sameMultiplayerGame(cabinetIdentity(r,cancel),expected))return "Your arcade game or connection package changed. Choose the game again.";
        QFile file(connection);QFileInfo info(connection);
        if(!info.isAbsolute()||info.isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>4096
           ||(info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)))return "Couldn't protect the arcade connection.";
        const QByteArray config="mode=cabinet\nhost="+QByteArray(host?"true":"false")+"\nsocket="+info.absolutePath().toUtf8()+"/cabinet.sock\n";
        if(file.readAll()!=config)return "Invalid arcade connection.";
        auto stage=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/trainer-cabinet-XXXXXX");if(!stage->isValid())return "Couldn't prepare arcade progress.";
        const auto content=arcadeContent(r,cancel,stage->path(),expected["content"].toString());
        if(content["entry"].toString().isEmpty()||!cabinetConfig(stage->path(),content["region"].toInt(),host,bundle(cancel)["renderer"].toString()))return "Couldn't prepare the arcade set.";
        out.program="/usr/bin/bwrap";out.workingDirectory=stage->path();
        out.arguments={"--die-with-parent","--unshare-net","--bind","/","/","--",cabinetRoot()+"/cabinet_endpoint",connection,"/usr/bin/env"};
        out.arguments+=cabinetEnvironment(stage->path());
        out.arguments+=inputPrefix;out.arguments<<cabinetRoot()+"/bin/flycast"<<content["entry"].toString();
        out.settled=[stage](const ProcessOutcome&){};return {};
    };return cmd;
}
QString bundleRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/flycast-netplay";}
QJsonObject bundle(const std::atomic_bool& cancel) {
    const auto root=bundleRoot();QFile file(root+"/manifest.json");
    if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return {};
    auto m=QJsonDocument::fromJson(file.readAll()).object();
    const auto relay=m.value("onlineRelay");
    if(m["protocol"]!=1||m["source"]!=sourceRevision
       ||(!relay.isUndefined()&&(!relay.isString()||(!relay.toString().isEmpty()&&!relayUrl(QUrl(relay.toString())))))
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
    h.addData(relay.toString().toUtf8());
    m["digest"]=QString::fromLatin1(h.result().toHex());
    const auto arcade=m["arcade"].toObject();
    if(arcade["protocol"]=="flycast-arcade-sets-v1") {
        QCryptographicHash extra(QCryptographicHash::Sha256);extra.addData(m["digest"].toString().toLatin1());
        bool valid=true;
        for(const auto& name:{"arcade.py","arcade-catalog.json"}) {
            const auto expected=arcade[name].toString();
            if(!hex(expected,64)||digest(root+'/'+name,cancel,4*1024*1024)!=expected){valid=false;break;}
            extra.addData(expected.toLatin1());
        }
        if(valid)m["arcadeDigest"]=QString::fromLatin1(extra.result().toHex());
    }
    return m;
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const std::atomic_bool& cancel,
                          const std::function<void(const QString&)>& observe) {
    const bool arcade=arcadePlatform(r.adventure.platformId);
    if((r.adventure.platformId!="dreamcast"&&!arcade)||r.adventure.adapterId!="retroarch")return {};
    if(arcade) {
        if(observe)for(const auto& name:{"manifest.json","bin/flycast","firmware/dc_boot.bin","firmware/dc_flash.bin","arcade.py","arcade-catalog.json","firmware"})observe(bundleRoot()+'/'+name);
        const auto m=bundle(cancel);if(m["arcadeDigest"].toString().isEmpty())return {};
        const auto content=arcadeContent(r,cancel);
        if(observe)for(const auto& path:content["observed"].toArray())observe(path.toString());
        if(!content["error"].toString().isEmpty()||!hex(content["content"].toString(),64))return {};
        return {{"id","runtime.flycast-dojo.arcade.v1"},{"label",r.adventure.title.left(96)},
            {"content",content["content"]},{"runtime",m["arcadeDigest"]},
            {"settings","dojo-arcade-fresh-nvram-v1-"+r.adventure.platformId+'-'+m["renderer"].toString()},
            {"players",2},{"lateJoin",false},{"temporaryProgress",true},
            {"connections",m.value("onlineRelay").toString().isEmpty()?QJsonArray{"nearby"}:QJsonArray{"nearby","online"}}};
    }
    // Observe content even before a bundle exists, including missing GDI tracks.
    const auto hash=discContent(r.contentPath,cancel,nullptr,false,observe);if(hash.isEmpty())return {};
    if(observe)for(const auto& file:{"manifest.json","bin/flycast","firmware/dc_boot.bin","firmware/dc_flash.bin"})observe(bundleRoot()+'/'+file);
    const auto m=bundle(cancel);if(m.isEmpty())return {};
    return {{"id","runtime.flycast-dojo.dc.v1"},{"label",r.adventure.title.left(96)},
        {"content",hash},{"runtime",m["digest"]},{"settings","dojo-v2-fresh-vmu-analog2-delay0-"+m["renderer"].toString()},
        {"players",2},{"lateJoin",false},{"temporaryProgress",true},
        {"connections",m.value("onlineRelay").toString().isEmpty()?QJsonArray{"nearby"}:QJsonArray{"nearby","online"}}};
}
ProcessCommand command(const AdventureRegistration& r,const Request& request,const QStringList& inputPrefix) {
    ProcessCommand cmd;cmd.runtimeControls={{"kind","flycast"},{"game",r.adventure.id},
        {"netplay",request.expected.toVariantMap()},{"temporaryProgress",true}};
    cmd.prepare=[r,request,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!hex(request.token,32)||(!request.host&&request.online&&!key(request.key))
           ||(!request.online&&QHostAddress(request.address).protocol()!=QAbstractSocket::IPv4Protocol))
            return "This game invitation is invalid.";
        if(!sameMultiplayerGame(netplayIdentity(r,cancel),request.expected))return "Your game or emulator changed. Invite again.";
        const auto m=bundle(cancel);if(m.isEmpty())return "The multiplayer setup changed. Invite again.";
        const QUrl relay(m.value("onlineRelay").toString());
        if(request.online&&!relayUrl(relay))return "Online play is not available in this installation.";
        const bool arcade=arcadePlatform(r.adventure.platformId);
        auto dir=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/trainer-flycast-XXXXXX");
        if(!dir->isValid())return "Couldn't prepare temporary multiplayer progress.";
        const auto root=dir->path();
        QString content;int region=1;
        if(arcade) {
            const auto prepared=arcadeContent(r,cancel,root,request.expected["content"].toString());
            if(!prepared["error"].toString().isEmpty())return prepared["error"].toString();
            content=prepared["entry"].toString();region=prepared["region"].toInt(-1);
            if(content.isEmpty()||!QFileInfo::exists(content)||region<0||region>3)return "Couldn't prepare the arcade set.";
        } else if(!QDir().mkpath(root+"/data"))return "Couldn't prepare temporary Dreamcast progress.";
        if(!arcade)for(const auto name:{"dc_boot.bin","dc_flash.bin"}) {
            const auto target=root+"/data/"+name;
            if(!QFile::copy(bundleRoot()+"/firmware/"+name,target)
               ||digest(target,cancel,2*1024*1024)!=m["files"].toObject()[QString("firmware/")+name])
                return "Couldn't prepare the Dreamcast firmware copy.";
        }
        QString cfg=QString("[config]\nDreamcast.AutoLoadState = no\nDreamcast.AutoSaveState = no\n"
            "Dreamcast.Region = 1\nDreamcast.Language = 1\nDreamcast.Broadcast = 0\nDreamcast.Cable = 3\n"
            "pvr.rend = %1\nFetchBoxart = no\n[window]\nfullscreen = yes\n"
            "[network]\nGGPO = yes\nGGPOAnalogAxes = 2\nGGPODelay = 0\nEnableUPnP = no\n"
            "GGPOPort = 19713\nGGPORemotePort = %2\nActAsServer = %3\nserver = %4\n"
            "[dojo]\nEnable = no\nRelay = %5\nRelayKey = %6\nDefaultRelayPort = %2\n"
            "EnableMatchCode = no\nHostJoinSelect = no\nIgnoreNetSave = yes\nNetCustomVmu = no\n"
            "EnableMemRestore = no\nCopyMissingSharedMem = no\nRecordMatches = no\nTransmitting = no\n"
            "Receiving = no\nOutputStreamTxt = no\nTrainerSession = %7\n")
            .arg(m["renderer"]=="vulkan"?4:0).arg(request.online?relay.port():19713)
            .arg(request.host?"yes":"no",request.online?relay.host():request.address,
                 request.online?"yes":"no",request.host?QString():request.key,request.token);
        if(arcade)cfg.replace("Dreamcast.Region = 1",QString("Dreamcast.Region = %1").arg(region));
        if(!write(root+"/emu.cfg",cfg.toUtf8()))return "Couldn't prepare multiplayer settings.";
        out.program="/usr/bin/env";out.workingDirectory=root;
        out.arguments={"TRAINEROS_FLYCAST_SESSION="+request.token,"FLYCAST_ROOT="+root,"XDG_CONFIG_HOME="+root,"XDG_DATA_HOME="+root,
                       "XDG_CONFIG_DIRS="+root,"XDG_DATA_DIRS="+root};
        if(!arcade&&discContent(r.contentPath,cancel,&content)!=request.expected["content"].toString())return "The Dreamcast disc set changed. Invite again.";
        out.arguments+=inputPrefix;out.arguments<<bundleRoot()+"/bin/flycast"<<content;
        out.settled=[dir](const ProcessOutcome&){};return {};
    };return cmd;
}
QString onlineRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/flycast-online";}
namespace {
constexpr int CardBytes=128*1024;
const QStringList onlineFiles{"dc_nvmem.bin","vmu_save_A1.bin","vmu_save_A2.bin",
    "vmu_save_B1.bin","vmu_save_B2.bin","vmu_save_C1.bin","vmu_save_C2.bin","vmu_save_D1.bin","vmu_save_D2.bin"};
QJsonObject onlineBundle(const std::atomic_bool& cancel,const std::function<void(const QString&)>& observe={}) {
    const auto root=onlineRoot();if(observe)observe(root+"/manifest.json");
    QFile file(root+"/manifest.json");if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return {};
    auto value=QJsonDocument::fromJson(file.readAll()).object();
    if(value["protocol"]!=1||value["source"]!="5aa091fde632fb332c8d8c34e280d62dc951954c"
       ||value["settings"]!="dcnet-owned-vmu-v1"||(value["renderer"]!="vulkan"&&value["renderer"]!="opengl"))return {};
    const auto files=value["files"].toObject();if(files.size()!=4)return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for(const auto& name:{"bin/flycast","catalog.json","firmware/dc_boot.bin","firmware/dc_flash.bin"}) {
        const auto path=root+'/'+name;if(observe)observe(path);
        if(!hex(files[name].toString(),64)||!QFileInfo(path).canonicalFilePath().startsWith(QFileInfo(root).canonicalFilePath()+'/')
           ||digest(path,cancel,256*1024*1024)!=files[name].toString())return {};
        hash.addData(files[name].toString().toLatin1());
    }
    if(!QFileInfo(root+"/bin/flycast").isExecutable()||QFileInfo(root+"/firmware/dc_boot.bin").size()!=2*1024*1024
       ||QFileInfo(root+"/firmware/dc_flash.bin").size()!=CardBytes)return {};
    hash.addData(value["renderer"].toString().toLatin1());value["digest"]=QString::fromLatin1(hash.result().toHex());return value;
}
QStringList onlineEnvironment(const QString& root) {
    return {"XDG_CONFIG_HOME="+root+"/config","XDG_DATA_HOME="+root+"/data",
        "TRAINEROS_DC_SESSION=1","TRAINEROS_FLYCAST_SESSION=dcnet-owned"};
}
bool onlineConfig(const QString& root,const QJsonObject& bundle,bool bba) {
    const auto config=root+"/config/flycast",data=root+"/data/flycast";
    if(!QDir().mkpath(config)||!QDir().mkpath(data))return false;
    for(const auto& name:{"dc_boot.bin","dc_flash.bin"})if(!QFile::copy(onlineRoot()+"/firmware/"+name,data+'/'+name))return false;
    const auto bytes=QString("[config]\nDreamcast.AutoLoadState = no\nDreamcast.AutoSaveState = no\n"
        "Dreamcast.Region = 3\nDreamcast.Language = 1\nDreamcast.Cable = 3\n"
        "PerGameVmu = no\nUsePhysicalVmuMemory = no\npvr.rend = %1\nFetchBoxart = no\n"
        "[window]\nfullscreen = yes\n[network]\nDCNet = yes\nEnable = no\nGGPO = no\nBattleCable = no\n"
        "EnableUPnP = no\nTrainerOnline = yes\nISPUsername = flycast1\nEmulateBBA = %2\n")
        .arg(bundle["renderer"]=="vulkan"?4:0).arg(bba?"yes":"no").toUtf8();
    return write(config+"/emu.cfg",bytes)&&QFile::setPermissions(config+"/emu.cfg",QFile::ReadOwner|QFile::WriteOwner);
}
// Fixed-size opaque envelope: missing cards have a zero flag and zero padding.
// Shared save storage can enforce exact-size return without interpreting VMUs.
QByteArray packOnline(const QString& directory) {
    QByteArray result("TODC1",5);
    for(const auto& name:onlineFiles) {
        QFile file(directory+'/'+name);const QFileInfo info(file);
        if(info.isSymLink())return {};
        if(!info.exists()){result.append(char(0));result.append(QByteArray(CardBytes,'\0'));continue;}
        if(!info.isFile()||info.size()!=CardBytes||!file.open(QIODevice::ReadOnly))return {};
        const auto data=file.readAll();if(data.size()!=CardBytes)return {};
        result.append(char(1));result.append(data);
    }return result;
}
bool unpackOnline(const QByteArray& bytes,const QString& directory) {
    if(bytes.size()!=5+onlineFiles.size()*(1+CardBytes)||!bytes.startsWith("TODC1"))return false;
    for(int i=0;i<onlineFiles.size();++i) {
        const int offset=5+i*(1+CardBytes);const auto data=bytes.mid(offset+1,CardBytes);
        if(bytes[offset]==0){if(data!=QByteArray(CardBytes,'\0'))return false;continue;}
        if(bytes[offset]!=1||!write(directory+'/'+onlineFiles[i],data))return false;
        if(!QFile::setPermissions(directory+'/'+onlineFiles[i],QFile::ReadOwner|QFile::WriteOwner))return false;
    }return true;
}
}
QJsonObject onlineIdentity(const AdventureRegistration& r,const std::atomic_bool& cancel,const std::function<void(const QString&)>& observe) {
    if(r.adventure.platformId!="dreamcast"||r.adventure.adapterId!="retroarch"||r.adventure.collectionOnly)return {};
    const auto installed=onlineBundle(cancel,observe);if(installed.isEmpty())return {};
    QString first;const auto content=discContent(r.contentPath,cancel,&first,false,observe);if(content.isEmpty())return {};
    QTemporaryDir stage(QDir::tempPath()+"/trainer-dc-probe-XXXXXX");
    if(!stage.isValid()||!onlineConfig(stage.path(),installed,false))return {};
    QProcess probe;probe.setProgram("/usr/bin/env");
    auto args=onlineEnvironment(stage.path());args<<"TRAINEROS_DC_INSPECT=1"<<onlineRoot()+"/bin/flycast"<<first;
    probe.setArguments(args);probe.setWorkingDirectory(stage.path());probe.setStandardErrorFile(QProcess::nullDevice());probe.start();
    if(!probe.waitForStarted(3000))return {};
    QElapsedTimer timer;timer.start();QByteArray output;
    while(probe.state()!=QProcess::NotRunning) {
        probe.waitForFinished(50);output+=probe.readAllStandardOutput();
        if(cancel||timer.elapsed()>30000||output.size()>65536){probe.kill();probe.waitForFinished(2000);return {};}
    }
    output+=probe.readAllStandardOutput();if(probe.exitCode()!=0||probe.exitStatus()!=QProcess::NormalExit)return {};
    QString serial;
    for(const auto& line:output.split('\n'))if(line.startsWith("[TrainerOSDisc] ")) {
        const auto raw=line.mid(16).trimmed();if(!serial.isEmpty()||!hex(QString::fromLatin1(raw),20))return {};
        serial=QString::fromLatin1(QByteArray::fromHex(raw)).trimmed().toUpper();
    }
    serial.remove('-');serial.remove(' ');if(QRegularExpression("^MK[0-9]+$").match(serial).hasMatch())serial.remove(0,2);
    if(!QRegularExpression("^[A-Z0-9]{4,16}$").match(serial).hasMatch())return {};
    QFile file(onlineRoot()+"/catalog.json");if(!file.open(QIODevice::ReadOnly)||file.size()>128*1024)return {};
    const auto catalog=QJsonDocument::fromJson(file.readAll()).object();if(catalog["protocol"]!=1)return {};
    const auto entry=catalog["entries"].toObject()[serial].toObject();if(entry["title"].toString().isEmpty()||!entry["bba"].isBool())return {};
    return {{"id","runtime.flycast.online.v1"},{"label",r.adventure.title.left(96)},
        {"content",content},{"runtime",installed["digest"]},{"settings","dcnet-owned-vmu-v1"},
        {"product",serial},{"bba",entry["bba"]},{"players",2},{"lateJoin",false},
        {"privateProgress",true},{"temporaryProgress",false},{"connections",QJsonArray{"online"}}};
}
ProcessCommand onlineCommand(const AdventureRegistration& r,const QJsonObject& expected,QString owner,QString backupRoot,const QStringList& inputPrefix) {
    ProcessCommand cmd;cmd.runtimeControls={{"kind","flycast"},{"game",r.adventure.id},{"netplay",expected.toVariantMap()},{"temporaryProgress",false}};
    cmd.prepare=[r,expected,owner,backupRoot,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(owner.isEmpty()||owner.size()>128||!QDir::isAbsolutePath(backupRoot)||QFileInfo(backupRoot).isSymLink())return "Your save owner is unavailable.";
        if(!sameMultiplayerGame(onlineIdentity(r,cancel),expected))return "Your Dreamcast game or Online setup changed. Choose the game again.";
        const auto ownerKey=QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex());
        QString path=backupRoot;
        for(const auto& part:QStringList{"native-online",ownerKey,expected["content"].toString()}) {
            path+='/'+part;if(QFileInfo(path).isSymLink()||!QDir().mkpath(path)||!QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))return "Couldn't protect your Online progress.";
        }
        auto lock=std::make_shared<QLockFile>(path+"/session.lock");lock->setStaleLockTime(0);
        if(!lock->tryLock(0))return "This Online save is already in use.";
        const SaveTargetResolver resolve=[path,owner,r](const AdventureRegistration&)->SaveTarget {
            SaveTarget t;t.adventureId=r.adventure.id;t.title=r.adventure.title;t.savePath=path+"/progress.bin";
            t.contentRevision=discContent(r.contentPath,std::atomic_bool{false});t.contextRevision="dcnet-owned-vmu-v1/"+owner+'/'+QString::number(r.revision);
            t.backupOwner=owner;t.lineageOwner=owner;t.supported=!t.contentRevision.isEmpty();return t;
        };
        const auto seed=captureRuntimeSave(backupRoot,r,resolve);if(!seed.error.isEmpty())return seed.error;
        auto stage=std::make_shared<QTemporaryDir>(path+"/session-XXXXXX");
        if(!stage->isValid()||!onlineConfig(stage->path(),onlineBundle(cancel),expected["bba"].toBool()))return "Couldn't prepare the Online session.";
        const auto data=stage->path()+"/data/flycast";
        if(seed.existed&&!unpackOnline(seed.bytes,data))return "Your Online save could not be read. The original was kept.";
        out.finalize=[stage,lock,r,backupRoot,resolve,seed,data](const ProcessOutcome& outcome)->QString {
            if(!outcome.started)return {};
            stage->setAutoRemove(false);
            if(outcome.crashed||outcome.stopped||outcome.exitCode!=0)return "The Online session ended unexpectedly. Its progress copy was kept for recovery.";
            const auto bytes=packOnline(data);if(bytes.isEmpty())return "The Online save could not be verified. Its session copy was kept.";
            const auto error=returnRuntimeSave(backupRoot,r,seed,bytes,resolve);
            if(error.isEmpty())stage->setAutoRemove(true);return error;
        };
        QString content;if(discContent(r.contentPath,cancel,&content)!=expected["content"].toString())return "Your Dreamcast disc changed.";
        out.program="/usr/bin/env";out.arguments=onlineEnvironment(stage->path());out.arguments+=inputPrefix;
        out.arguments<<onlineRoot()+"/bin/flycast"<<content;out.workingDirectory=stage->path();return {};
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
