#include "FlycastNetplay.h"
#include "integrations/adventure/retroarch/RetroArchDisc.h"
#include "core/model/GamePlayers.h"
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
