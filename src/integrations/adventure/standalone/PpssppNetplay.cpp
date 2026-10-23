#include "PpssppNetplay.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEndian>

namespace trainer::ppsspp {
QString configFile(const StandaloneInstallation& i) {
    if(!i.configFile.isEmpty())return i.configFile;
    const auto root=i.prefixArguments.contains("org.ppsspp.PPSSPP")
        ?QDir::homePath()+"/.var/app/org.ppsspp.PPSSPP/config"
        :QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return root+"/ppsspp/PSP/SYSTEM/ppsspp.ini";
}
QString discId(const QString& path,const std::atomic_bool& cancel) {
    QFile file(path);
    if(cancel||QFileInfo(path).suffix().compare("iso",Qt::CaseInsensitive)||!file.open(QIODevice::ReadOnly)||
       file.size()<18*2048||file.size()>2LL*1024*1024*1024||file.size()%2048)return {};
    const auto read=[&](quint64 offset,quint64 size)->QByteArray {
        if(cancel||size>1024*1024||offset>quint64(file.size())||size>quint64(file.size())-offset||!file.seek(offset))return {};
        const auto bytes=file.read(size);return quint64(bytes.size())==size?bytes:QByteArray{};
    };
    const auto le32=[](const QByteArray& b,int p){return qFromLittleEndian<quint32>(b.constData()+p);};
    const auto pvd=read(16*2048,2048);
    if(pvd.size()!=2048||pvd.left(7)!=QByteArray::fromHex("01434430303101")||
       qFromLittleEndian<quint16>(pvd.constData()+128)!=2048||qFromBigEndian<quint16>(pvd.constData()+130)!=2048||
       le32(pvd,80)<18||le32(pvd,80)!=qFromBigEndian<quint32>(pvd.constData()+84)||
       quint64(le32(pvd,80))*2048>quint64(file.size()))return {};
    const auto extent=[&](const QByteArray& entry,bool directory)->QByteArray {
        if(entry.size()<34||quint8(entry[0])!=entry.size()||quint8(entry[32])+33>entry.size()||
           entry[1]!=0||entry[26]!=0||entry[27]!=0||(quint8(entry[25])&~3)||bool(entry[25]&2)!=directory||
           le32(entry,2)!=qFromBigEndian<quint32>(entry.constData()+6)||
           le32(entry,10)!=qFromBigEndian<quint32>(entry.constData()+14)||
           quint64(le32(entry,2))*2048+le32(entry,10)>quint64(le32(pvd,80))*2048)return {};
        return read(quint64(le32(entry,2))*2048,le32(entry,10));
    };
    const auto child=[&](const QByteArray& directory,const QByteArray& wanted)->QByteArray {
        QByteArray result;
        for(int p=0;p<directory.size();) {
            if(cancel)return {};
            const int size=quint8(directory[p]);
            if(!size){p=(p/2048+1)*2048;continue;}
            if(size<34||p+size>directory.size()||p%2048+size>2048||quint8(directory[p+32])+33>size)return {};
            const auto entry=directory.mid(p,size);
            auto name=entry.mid(33,quint8(entry[32]));
            if(name.endsWith(";1"))name.chop(2);
            if(name==wanted){if(!result.isEmpty())return {};result=entry;}
            p+=size;
        }
        return result;
    };
    const auto root=extent(pvd.mid(156,quint8(pvd[156])),true);
    const auto game=extent(child(root,"PSP_GAME"),true);
    const auto sfo=extent(child(game,"PARAM.SFO"),false);
    if(sfo.size()<20||sfo.left(4)!=QByteArray::fromHex("00505346")||le32(sfo,4)!=0x101)return {};
    const quint64 keys=le32(sfo,8),data=le32(sfo,12),count=le32(sfo,16);
    if(count>256||20+count*16>keys||keys>=data||data>=quint64(sfo.size()))return {};
    QMap<QByteArray,QByteArray> fields;
    for(quint64 n=0;n<count;++n) {
        const int pos=20+n*16;
        const quint64 key=keys+qFromLittleEndian<quint16>(sfo.constData()+pos);
        const quint64 value=data+le32(sfo,pos+12),length=le32(sfo,pos+4),maximum=le32(sfo,pos+8);
        if(key>=data||value>quint64(sfo.size())||length>maximum||maximum>quint64(sfo.size())-value)return {};
        const auto end=sfo.indexOf('\0',key);
        if(end<0||quint64(end)>=data)return {};
        const auto name=sfo.mid(key,end-key);
        if(name!="DISC_ID"&&name!="CATEGORY")continue;
        if(fields.contains(name)||qFromLittleEndian<quint16>(sfo.constData()+pos+2)!=0x204||
           !length||length>32||sfo[value+length-1]!='\0')return {};
        fields[name]=sfo.mid(value,length-1);
    }
    const auto id=QString::fromLatin1(fields.value("DISC_ID"));
    return fields.value("CATEGORY")=="UG"&&QRegularExpression("^[A-Z]{4}[0-9]{5}$").match(id).hasMatch()?id:QString();
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const StandaloneInstallation& i,const std::atomic_bool& cancel) {
    if(r.adventure.platformId!="psp"||r.adventure.adapterId!="ppsspp"||i.program.isEmpty()||
       !i.platforms.contains("psp"))return {};
    const auto runtime=retroarch::fileDigest(i.runtimeFile,256*1024*1024,cancel);
    // Admit the reviewed relay repair and retain the original build for rollback.
    // Wire/settings identity remains independent of the device's executable hash.
    const bool repairedRelay=runtime=="6db5a5bc7263b930b29fa6a65348c8a292acdb38306b74bdee071a22c5339834";
    if(!repairedRelay&&runtime!="c99cec693067f8c94ae0d8b5e7299392ed5c0cfae3f8231176ed25ea6c155dfc")return {};
    const QString relaySettings=repairedRelay?QStringLiteral("-relay-recv-v1"):QString();
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
        "EnableAdhocServer = "+QByteArray(request.host&&!request.online?"True":"False")+
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
