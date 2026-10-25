#include "AzaharNetplay.h"
#include "NetplayCache.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEndian>

namespace trainer::azahar {
namespace {
bool hex(const QString& value,int size=64) {return value.size()==size&&QRegularExpression("^[a-f0-9]+$").match(value).hasMatch();}
bool compressedHeader(QFile& file,const QByteArray& header,const QString& suffix) {
    // Azahar 2126.2 Z3DSFileHeader; native seekable-Zstd remains the decoder.
    const auto magic=suffix=="zcci"?QByteArray("NCSD"):suffix=="zcxi"?QByteArray("NCCH"):QByteArray("3DSX");
    if(header.size()<32||header.first(4)!="Z3DS"||header.mid(4,4)!=magic||header[8]!=1||header[9]!=0)return false;
    const quint64 start=qFromLittleEndian<quint16>(header.constData()+10)+quint64(qFromLittleEndian<quint32>(header.constData()+12));
    const auto packed=qFromLittleEndian<quint64>(header.constData()+16),raw=qFromLittleEndian<quint64>(header.constData()+24);
    if(start<32||start>16*1024*1024||packed<17||start>quint64(file.size())||packed!=quint64(file.size())-start||raw<32||raw>8ULL*1024*1024*1024)return false;
    if(!file.seek(file.size()-9))return false;
    const auto footer=file.read(9);if(footer.size()!=9||footer.last(4)!=QByteArray::fromHex("b1ea928f"))return false;
    const auto frames=qFromLittleEndian<quint32>(footer.constData());const auto flags=quint8(footer[4]);
    if(!frames||frames>1048576||(flags&0x7f))return false;
    const quint64 tableSize=quint64(frames)*((flags&0x80)?12:8)+17;
    if(tableSize>packed||!file.seek(file.size()-tableSize))return false;
    const auto table=file.read(tableSize);
    if(quint64(table.size())!=tableSize||table.first(4)!=QByteArray::fromHex("5e2a4d18")||qFromLittleEndian<quint32>(table.constData()+4)!=tableSize-8)return false;
    quint64 compressed=0,decoded=0;
    for(quint32 n=0;n<frames;++n) {
        const auto at=table.constData()+8+n*((flags&0x80)?12:8);
        compressed+=qFromLittleEndian<quint32>(at);decoded+=qFromLittleEndian<quint32>(at+4);
    }
    return compressed+tableSize==packed&&decoded==raw;
}
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
bool writePrivate(const QString& path,const QByteArray& bytes) {
    QFile file(path);return file.open(QIODevice::WriteOnly|QIODevice::NewOnly)
        &&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)&&file.write(bytes)==bytes.size()&&file.flush();
}

}
QString bundleRoot() {return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/azahar-netplay";}
QJsonObject bundle(const std::atomic_bool& cancel) {
    const auto root=bundleRoot(),canonical=QFileInfo(root).canonicalFilePath();
    QFile file(root+"/manifest.json");
    if(canonical.isEmpty()||QFileInfo(file.fileName()).isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>16384)return {};
    auto value=QJsonDocument::fromJson(file.readAll()).object();
    if(value["protocol"]!=1||value["source"]!=sourceRevision||value["settings"]!="native-room-two-private-v1")return {};
    const auto files=value["files"].toObject();
    const QStringList expected{"bin/azahar","transport/room-session.py","transport/secure-channel.py","transport/requirements.lock"};
    if(files.size()!=expected.size())return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for(const auto& name:expected) {
        const auto path=root+'/'+name;
        if(!QFileInfo(path).canonicalFilePath().startsWith(canonical+'/')||!hex(files[name].toString())
           ||digest(path,cancel,256LL*1024*1024)!=files[name].toString())return {};
        hash.addData(name.toUtf8());hash.addData(QByteArray(1,'\0'));hash.addData(files[name].toString().toLatin1());
    }
    const auto python=root+"/transport/venv/bin/python";
    if(!QFileInfo(root+"/bin/azahar").isExecutable()||!QFileInfo(python).isExecutable()
       ||!hex(value["interpreterSha256"].toString())||digest(python,cancel,64*1024*1024)!=value["interpreterSha256"].toString())return {};
    hash.addData(value["interpreterSha256"].toString().toLatin1());hash.addData(value["settings"].toString().toLatin1());
    value["digest"]=QString::fromLatin1(hash.result().toHex());return value;
}
QJsonObject netplayIdentity(const AdventureRegistration& record,const std::atomic_bool& cancel) {
    if(record.adventure.platformId!="n3ds"||record.adventure.adapterId!="azahar"||record.adventure.collectionOnly)return {};
    const auto suffix=QFileInfo(record.contentPath).suffix().toLower();
    QFile file(record.contentPath);if(!file.open(QIODevice::ReadOnly)||file.size()<32)return {};
    const auto header=file.read(512);
    // CIA/ZCIA installs and remote Artic Base are not launchable images.
    const bool packed=QStringList{"zcci","zcxi","z3dsx"}.contains(suffix);
    const bool elf=QStringList{"elf","axf"}.contains(suffix)&&header.startsWith(QByteArray("\x7f" "ELF\x01\x01",6))
        &&header.size()>=52&&qFromLittleEndian<quint16>(header.constData()+18)==40;
    if(!(elf||(packed&&compressedHeader(file,header,suffix))||(suffix=="3dsx"&&header.startsWith("3DSX"))||
         ((suffix=="cci"||suffix=="3ds")&&header.mid(256,4)=="NCSD")||((suffix=="cxi"||suffix=="app")&&header.mid(256,4)=="NCCH")))return {};
    const auto installed=bundle(cancel);if(installed.isEmpty())return {};
    const auto content=digest(record.contentPath,cancel,8LL*1024*1024*1024);if(content.isEmpty())return {};
    return {{"id","runtime.azahar.3ds.v1"},{"label",record.adventure.title.left(96)},
        {"content",content},{"runtime",installed["digest"]},{"settings",installed["settings"]},
        {"players",2},{"lateJoin",false},{"temporaryProgress",false}};
}
ProcessCommand command(const AdventureRegistration& record,const Request& request,const QStringList& inputPrefix) {
    ProcessCommand result;result.runtimeControls={{"kind","azahar"},{"game",record.adventure.id},
        {"netplay",request.expected.toVariantMap()},{"temporaryProgress",false}};
    result.prepare=[record,request,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!hex(request.identity)||(!request.host&&(!endpoint(request.endpoint)||request.endpoint["mode"]!=(request.online?"online":"nearby")))
           ||(!request.online&&QHostAddress(request.address).protocol()!=QAbstractSocket::IPv4Protocol))return "This 3DS invitation is invalid.";
        if(!sameMultiplayerGame(netplayIdentity(record,cancel),request.expected))return "Your 3DS game or emulator changed. Invite again.";
        auto directory=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/traineros-3ds-XXXXXX");
        if(!directory->isValid())return "Couldn't prepare the 3DS session.";
        const auto path=QFileInfo(directory->path()).canonicalFilePath(),user=path+"/user";
        if(!QDir().mkpath(user+"/config"))return "Couldn't prepare private 3DS settings.";
        if(!prepareNetplayCache(user+"/shaders","azahar",request.expected["runtime"].toString()))
            return "Couldn't prepare the graphics cache.";
        QSettings original(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/azahar-emu/qt-config.ini",QSettings::IniFormat);
        const int graphics=original.value("Renderer/graphics_api",2).toInt();
        if(graphics!=0&&graphics!=2)return "This 3DS renderer isn't available in the multiplayer build.";
        const auto data=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/azahar-emu";
        const bool custom=original.value("Data Storage/use_custom_storage",false).toBool();
        const auto prepareDirectory=[](const QString& value)->QString {
            if(!QDir::isAbsolutePath(value)||!QDir().mkpath(value))return {};
            return QFileInfo(value).canonicalFilePath();
        };
        const auto nand=prepareDirectory(custom?original.value("Data Storage/nand_directory").toString():data+"/nand");
        const auto sdmc=prepareDirectory(custom?original.value("Data Storage/sdmc_directory").toString():data+"/sdmc");
        const auto sysdata=prepareDirectory(data+"/sysdata");
        if(nand.isEmpty()||sdmc.isEmpty()||sysdata.isEmpty())return "Your 3DS data directory is unavailable.";
        QSettings settings(user+"/config/qt-config.ini",QSettings::IniFormat);
        for(const auto& key:original.allKeys())if(key.startsWith("Controls/")||key.startsWith("Layout/")||key.startsWith("Renderer/")||key.startsWith("Audio/")||key.startsWith("Core/")||key.startsWith("System/"))settings.setValue(key,original.value(key));
        const auto set=[&](const QString& key,const QVariant& value){settings.setValue(key,value);settings.setValue(key+"/default",false);};
        // Physical Home belongs to TrainerOS even when an older ordinary
        // emulator profile mapped it to the emulated console's Home button.
        for(const auto& key:settings.allKeys())if(key.startsWith("Controls/")&&key.endsWith("/button_home"))set(key,"engine:keyboard,code:0");
        set("UI/firstStart",false);set("UI/confirmClose",false);set("UI/pauseWhenInBackground",false);
        set("Data Storage/use_custom_storage",true);set("Data Storage/nand_directory",nand+'/');
        set("Data Storage/sdmc_directory",sdmc+'/');set("Renderer/graphics_api",graphics);
        settings.sync();if(settings.status()!=QSettings::NoError)return "Couldn't prepare private 3DS settings.";
        QJsonObject channel=request.host?QJsonObject{}:request.endpoint;
        channel["host"]=request.host;channel["identity"]=request.identity;channel["mode"]=request.online?"online":"nearby";
        if(!request.online)channel["address"]=request.address;
        const auto root=bundleRoot(),python=root+"/transport/venv/bin/python";
        const QJsonObject config{{"channel",channel},{"content",record.contentPath},{"runtime",root+"/bin/azahar"},
            {"python",python},{"helper",root+"/transport/secure-channel.py"},{"user",user},{"sysdata",sysdata}};
        if(!writePrivate(path+"/network.json",QJsonDocument(config).toJson(QJsonDocument::Compact)))return "Couldn't protect the 3DS connection details.";
        out.finalize=[directory](const ProcessOutcome&){return QString();};
        out.program="/usr/bin/env";out.arguments={"QT_QPA_PLATFORM=xcb"};out.arguments+=inputPrefix;
        out.arguments<<python<<root+"/transport/room-session.py"<<path+"/network.json";out.workingDirectory=path;return {};
    };
    return result;
}
QJsonObject event(const QByteArray& bytes) {
    const auto line=bytes.trimmed();constexpr auto prefix="TRAINEROS_3DS ";
    if(!line.startsWith(prefix)||line.size()>2048)return {};
    const auto decoded=QByteArray::fromBase64Encoding(line.mid(qstrlen(prefix)),QByteArray::AbortOnBase64DecodingErrors);if(!decoded)return {};
    const auto value=QJsonDocument::fromJson(decoded.decoded).object();
    if(value.size()==1&&(value["event"]=="running"||value["event"]=="error"))return value;
    return value.size()==2&&value["event"]=="ready"&&endpoint(value["endpoint"].toObject())?value:QJsonObject{};
}
}
