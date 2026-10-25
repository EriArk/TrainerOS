#include "PpssppNetplay.h"
#include "PspContentIdentity.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEndian>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QProcess>
#include <QElapsedTimer>

namespace trainer::ppsspp {
QString contentProbeRoot() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/traineros/psp-content";
}
QString contentProbeFile() {
#ifdef Q_OS_WIN
    return contentProbeRoot()+"/psp-content-probe.exe";
#else
    return contentProbeRoot()+"/psp-content-probe";
#endif
}
namespace {
QString packedDiscId(const QString& path,const std::atomic_bool& cancel) {
    if(cancel)return {};
    const QFileInfo source(path),binary(contentProbeFile());
    QFile manifestFile(contentProbeRoot()+"/manifest.json");
    if(!source.isFile()||source.size()<24||source.size()>2LL*1024*1024*1024||
       !binary.isFile()||binary.isSymLink()||binary.size()>64*1024*1024||
       !manifestFile.open(QIODevice::ReadOnly)||manifestFile.size()>4096)return {};
#ifndef Q_OS_WIN
    if(!binary.isExecutable())return {};
#endif
    const auto manifest=QJsonDocument::fromJson(manifestFile.readAll()).object();
    if(manifest["recipe"]!="psp-content-v1"||manifest["source"]!="fa50bb1976065c4f8b1b47af227d367fe9771555")return {};
    const auto expected=manifest["files"].toObject()[binary.fileName()].toString();
    if(expected.size()!=64||retroarch::fileDigest(binary.absoluteFilePath(),64*1024*1024,cancel)!=expected)return {};
    QProcess process;process.setProgram(binary.absoluteFilePath());
    process.setArguments({"--psp-content",source.absoluteFilePath()});
    process.setStandardErrorFile(QProcess::nullDevice());process.start();
    if(!process.waitForStarted(1500))return {};
    QElapsedTimer deadline;deadline.start();QByteArray output;
    do {
        process.waitForFinished(50);output+=process.readAllStandardOutput();
        if(cancel||deadline.elapsed()>15000||output.size()>4096){process.kill();process.waitForFinished(1000);return {};}
    } while(process.state()!=QProcess::NotRunning);
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)return {};
    const auto value=QJsonDocument::fromJson(output).object();
    const auto disc=value["disc"].toString();
    if(value["protocol"]!=1||value["format"]!=source.suffix().toLower()||
       !QRegularExpression("\\A[A-Z]{4}[0-9]{5}\\z").match(disc).hasMatch())return {};
    return disc;
}
}
QString hostRelayRoot() {
    const auto root=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/traineros/psp-network";
    QFile file(root+"/manifest.json");if(!file.open(QIODevice::ReadOnly)||file.size()>4096)return {};
    const auto manifest=QJsonDocument::fromJson(file.readAll()).object();
    if(manifest["recipe"]!="psp-host-v1")return {};
    for(const auto& name:{"stream_transport","psp_relay"}) {
        const QFileInfo info(root+"/"+name);QFile binary(info.absoluteFilePath());
        if(!info.isExecutable()||info.isSymLink()||info.size()>64*1024*1024||!binary.open(QIODevice::ReadOnly)
           ||QString::fromLatin1(QCryptographicHash::hash(binary.readAll(),QCryptographicHash::Sha256).toHex())!=manifest["files"].toObject()[name].toString())return {};
    }
    return root;
}
QString configFile(const StandaloneInstallation& i) {
    if(!i.configFile.isEmpty())return i.configFile;
    const auto root=i.prefixArguments.contains("org.ppsspp.PPSSPP")
        ?QDir::homePath()+"/.var/app/org.ppsspp.PPSSPP/config"
        :QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return root+"/ppsspp/PSP/SYSTEM/ppsspp.ini";
}
QString discId(const QString& path,const std::atomic_bool& cancel) {
    const auto suffix=QFileInfo(path).suffix().toLower();
    if(suffix=="cso"||suffix=="pbp")return packedDiscId(path,cancel);
    QFile file(path);
    if(cancel||QFileInfo(path).suffix().compare("iso",Qt::CaseInsensitive)||!file.open(QIODevice::ReadOnly)||
       file.size()<18*2048||file.size()>2LL*1024*1024*1024||file.size()%2048)return {};
    const auto read=[&](quint64 offset,quint64 size)->QByteArray {
        if(cancel||size>1024*1024||offset>quint64(file.size())||size>quint64(file.size())-offset||!file.seek(offset))return {};
        const auto bytes=file.read(size);return quint64(bytes.size())==size?bytes:QByteArray{};
    };
    return content::umdDiscId(file.size(),read);
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const StandaloneInstallation& i,const std::atomic_bool& cancel) {
    if(r.adventure.platformId!="psp"||r.adventure.adapterId!="ppsspp"||i.program.isEmpty()||
       !i.platforms.contains("psp"))return {};
    const auto runtime=retroarch::fileDigest(i.runtimeFile,256*1024*1024,cancel);
    // Admit the reviewed relay repair and retain the original build for rollback.
    // Wire/settings identity remains independent of the device's executable hash.
    const bool threadedRelay=runtime=="01eafca876b8bc05ec2e087f6619bd127d5f6cfbc68aff804b6d362f5fdc25d5";
    const bool coordinationTimeout=threadedRelay||runtime=="dec19c9c42380155cde9c492f1d998ea6d13701b592d331d4f0073018cee73e6";
    const bool coordinationRecovery=coordinationTimeout||runtime=="a8489753f9691ab00bd5e7fd1e7c392bd10d3119635e44c598896d76a8ebc5f2";
    const bool boundedRecovery=coordinationRecovery||runtime=="b5acbf94ee69f270b55759cc84e22d4ee54798e9e0eb6d438d2ae87cbe5c9906";
    const bool repairedRelay=boundedRecovery||runtime=="6db5a5bc7263b930b29fa6a65348c8a292acdb38306b74bdee071a22c5339834";
    if(!repairedRelay&&runtime!="c99cec693067f8c94ae0d8b5e7299392ed5c0cfae3f8231176ed25ea6c155dfc")return {};
    const QString relaySettings=threadedRelay?QStringLiteral("-pdp-threading-v5"):
        coordinationTimeout?QStringLiteral("-coordination-timeout-v4"):
        coordinationRecovery?QStringLiteral("-coordination-recovery-v3"):
        boundedRecovery?QStringLiteral("-relay-recovery-v2"):
        repairedRelay?QStringLiteral("-relay-recv-v1"):QString();
    QString content;
    // Preserve the exact installed route independently of the new format probe.
    if(QFileInfo(r.contentPath).size()==209911808) {
        content=retroarch::fileDigest(r.contentPath,210*1024*1024,cancel);
        if(content=="9b21dd44a2b9ceb746ab9ed9cea4b30de69e63880704c0e34177f1c30c38b82b")
            return {{"id","runtime.ppsspp.lumines-us.v1"},{"label","Lumines - Puzzle Fusion"},
                {"content",content},{"runtime",runtime},{"settings","ppsspp-adhoc-isolated-config-clock-focus-v3"+relaySettings}};
    }
    const auto disc=discId(r.contentPath,cancel);if(disc.isEmpty())return {};
    if(content.isEmpty())content=retroarch::fileDigest(r.contentPath,2LL*1024*1024*1024,cancel);
    if(content.isEmpty())return {};
    return {{"id","runtime.ppsspp.adhoc.v1"},{"label",r.adventure.title.left(96)},
        {"content",content},{"disc",disc},{"runtime",runtime},{"settings","ppsspp-adhoc-native-clock-focus-v2"+relaySettings},
        {"players",2},{"lateJoin",false}};
}
QByteArray networkSettings(const NetplayRequest& request) {
    QString nickname=request.nickname;
    nickname.remove(QRegularExpression("[\\x00-\\x1f\\x7f=\\[\\]]"));nickname=nickname.simplified().left(24);
    if(nickname.isEmpty())nickname="Trainer";
    // Keep PPSSPP's saved console identity, including a per-game override.
    // Some games bind ordinary saves to this MAC; changing it per invitation
    // makes the same memory stick appear to belong to another PSP.
    // The exact Lumines route needs PPSSPP's real-clock synchronization on both
    // peers to leave matching together. Keep it session-only; see ppsspp.md.
    return "[Network]\nEnableWlan = True\nEnableUPnP = False\n"
        "EnableAdhocServer = "+QByteArray(request.host&&(!request.online||request.hostRelay)?"True":"False")+
        "\nproAdhocServer = "+request.address.toUtf8()+"\nAdhocServerRelayMode = "+QByteArray(request.online?"1":"2")+
        "\nPortOffset = 10000\nAllowSavestateWhileConnected = False\nAllowSpeedControlWhileConnected = False\n"
        "[SystemParam]\nNickName = "+nickname.toUtf8()+
        "\n[General]\nForceLagSync2 = "+QByteArray(request.expected["id"]=="runtime.ppsspp.lumines-us.v1"?"True":"False")+
        "\nAutoLoadSaveState = 0\nEnableCheats = False\nEnablePlugins = False\n"
        // A solo per-game pause preference must not stop the emulated peer
        // while the player opens Home/chat or minimizes this online session.
        "PauseOnLostFocus = False\nPauseWhenMinimized = False\n"
        "[Achievements]\nAchievementsEnable = False\n";
}
QString configureNetplay(ProcessCommand& cmd,const StandaloneInstallation& i,const NetplayRequest& request,const std::atomic_bool& cancel) {
    if(cancel)return "Opening cancelled.";
    if(request.hostRelay?(!request.online||request.address!=(request.host?"127.0.0.1":"127.0.0.2")):
       request.online ? request.address!="socom.cc" :
       (QHostAddress(request.address).isNull()||QHostAddress(request.address).protocol()!=QAbstractSocket::IPv4Protocol))
        return "This multiplayer address is invalid.";
    const QFileInfo config(configFile(i));
    const auto system=config.absolutePath();
    if(!config.isAbsolute()||!config.isFile()||config.isSymLink()||!config.isReadable()||
       QFileInfo(system).isSymLink())return "Couldn't read your PSP settings.";
    QDir psp(system);if(!psp.cdUp())return "Couldn't read your PSP storage.";
    // Keep the ordinary memory stick. Only SYSTEM is copied: appended settings
    // are saved by PPSSPP, including into per-game INIs, so symlinking it is wrong.
    auto directory=std::make_shared<QTemporaryDir>(system+"/.traineros-netplay-XXXXXX");
    if(!directory->isValid())return "Couldn't prepare multiplayer settings.";
    QFile::setPermissions(directory->path(),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    const auto root=directory->path()+"/config";
    const auto target=root+"/ppsspp/PSP";
    if(!QDir().mkpath(target+"/SYSTEM"))return "Couldn't prepare multiplayer settings.";
    const auto files=QDir(system).entryInfoList({"*.ini"},QDir::Files);
    qint64 size=0;
    if(files.size()>512)return "Too many PSP settings files to prepare multiplayer.";
    for(const auto& f:files) {
        if(cancel)return "Opening cancelled.";
        size+=f.size();
        if(f.size()>1024*1024||size>8*1024*1024||!QFile::copy(f.filePath(),target+"/SYSTEM/"+f.fileName()))
            return "Couldn't copy your PSP settings.";
    }
    if(config.fileName()!="ppsspp.ini")return "This PSP settings layout isn't supported yet.";
    const auto saves=psp.filePath("SAVEDATA");
    if(!QDir().mkpath(saves)||!QFile::link(saves,target+"/SAVEDATA"))return "Couldn't retain your PSP saves.";
    // Preserve installed textures and game resources without duplicating them.
    for(const auto& name:{"TEXTURES","GAME"}) {
        const auto source=psp.filePath(name);
        if(QFileInfo(source).isDir()&&!QFile::link(source,target+'/'+name))return "Couldn't retain your PSP resources.";
    }
    const auto bytes=networkSettings(request);
    const auto appended=directory->path()+"/network.ini";
    QSaveFile file(appended);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return "Couldn't prepare multiplayer settings.";
    const auto app=cmd.arguments.indexOf("org.ppsspp.PPSSPP");
    if(app>=0) {
        // Flatpak reserves XDG_CONFIG_HOME and resets --env for it. Set it
        // inside the sandbox through env, which execs PPSSPP without a shell.
        cmd.arguments.insert(app,"--command=env");
        cmd.arguments.insert(app+2,"XDG_CONFIG_HOME="+root);
        cmd.arguments.insert(app+3,"/app/bin/PPSSPPSDL");
    }
    else {
        // env replaces itself with the native executable; no command shell.
        cmd.arguments.prepend(cmd.program);cmd.arguments.prepend("XDG_CONFIG_HOME="+root);cmd.program="/usr/bin/env";
    }
    const auto content=cmd.arguments.takeLast();
    cmd.arguments<<"--appendconfig="+appended<<content;
    cmd.runtimeControls["netplay"]=request.expected.toVariantMap();
    cmd.runtimeControls["netplayHost"]=request.host;
    const auto settled=cmd.settled;
    cmd.settled=[settled,directory](const ProcessOutcome& outcome){if(settled)settled(outcome);directory->remove();};
    return {};
}
}
