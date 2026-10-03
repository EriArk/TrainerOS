#include "PpssppNetplay.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace trainer::ppsspp {
QString configFile(const StandaloneInstallation& i) {
    if(!i.configFile.isEmpty())return i.configFile;
    const auto root=i.prefixArguments.contains("org.ppsspp.PPSSPP")
        ?QDir::homePath()+"/.var/app/org.ppsspp.PPSSPP/config"
        :QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return root+"/ppsspp/PSP/SYSTEM/ppsspp.ini";
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const StandaloneInstallation& i,const std::atomic_bool& cancel) {
    if(r.adventure.platformId!="psp"||r.adventure.adapterId!="ppsspp"||i.program.isEmpty()||
       !i.platforms.contains("psp")||QFileInfo(r.contentPath).size()!=209911808)return {};
    const auto content=retroarch::fileDigest(r.contentPath,210*1024*1024,cancel);
    if(content!="9b21dd44a2b9ceb746ab9ed9cea4b30de69e63880704c0e34177f1c30c38b82b")return {};
    const auto runtime=retroarch::fileDigest(i.runtimeFile,256*1024*1024,cancel);
    // This first route is verified against the installed ARM64 1.20.4 binary.
    if(runtime!="c99cec693067f8c94ae0d8b5e7299392ed5c0cfae3f8231176ed25ea6c155dfc")return {};
    return {{"id","runtime.ppsspp.lumines-us.v1"},{"label","Lumines - Puzzle Fusion"},
        {"content",content},{"runtime",runtime},{"settings","ppsspp-adhoc-isolated-config-clock-v2"}};
}
QString configureNetplay(ProcessCommand& cmd,const StandaloneInstallation& i,const NetplayRequest& request,const std::atomic_bool& cancel) {
    if(cancel)return "Opening cancelled.";
    if(request.online ? request.address!="socom.cc" :
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
    QString nickname=request.nickname;
    nickname.remove(QRegularExpression("[\\x00-\\x1f\\x7f=\\[\\]]"));nickname=nickname.simplified().left(24);
    if(nickname.isEmpty())nickname="Trainer";
    // Keep PPSSPP's saved console identity, including a per-game override.
    // Some games bind ordinary saves to this MAC; changing it per invitation
    // makes the same memory stick appear to belong to another PSP.
    // The exact Lumines route needs PPSSPP's real-clock synchronization on both
    // peers to leave matching together. Keep it session-only; see ppsspp.md.
    const QByteArray bytes="[Network]\nEnableWlan = True\nEnableUPnP = False\n"
        "EnableAdhocServer = "+QByteArray(request.host&&!request.online?"True":"False")+
        "\nproAdhocServer = "+request.address.toUtf8()+"\nAdhocServerRelayMode = "+QByteArray(request.online?"1":"2")+
        "\nPortOffset = 10000\nAllowSavestateWhileConnected = False\nAllowSpeedControlWhileConnected = False\n"
        "[SystemParam]\nNickName = "+nickname.toUtf8()+
        "\n[General]\nForceLagSync2 = True\nAutoLoadSaveState = 0\nEnableCheats = False\nEnablePlugins = False\n"
        "[Achievements]\nAchievementsEnable = False\n";
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
