#include "RetroArchNetplay.h"
#include "RetroArchConfiguration.h"
#include "RetroArchDisc.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

namespace trainer::retroarch {
namespace {
struct CoreProfile {const char *core,*platforms,*firmware;};
// This table describes shared-console rollback, not handheld link emulation.
const CoreProfile cores[] = {
    {"fceumm","nes|fds","disksys.rom"},
    {"nestopia","nes|fds","disksys.rom"},
    {"snes9x","snes",""},
    {"genesis_plus_gx","sg1000|mastersystem|megadrive|segacd","bios_CD_U.bin|bios_CD_E.bin|bios_CD_J.bin|bios_MD.bin|bios_E.sms|bios_U.sms|bios_J.sms|sk.bin|sk2chip.bin|areplay.bin|ggenie.bin"},
    {"picodrive","mastersystem|megadrive|segacd|sega32x","bios_CD_U.bin|bios_CD_E.bin|bios_CD_J.bin"},
    {"mednafen_pce_fast","pcengine|pcenginecd","syscard3.pce|syscard2.pce|syscard1.pce|gexpress.pce"},
    {"mednafen_supergrafx","supergrafx|pcengine|pcenginecd","syscard3.pce|syscard2.pce|syscard1.pce|gexpress.pce"},
    {"stella","atari2600",""},
    {"fbneo","fbneo|neogeo","neogeo.zip|pgm.zip|skns.zip|decocass.zip|isgsm.zip|midssio.zip|nmk004.zip|ym2608.zip|cchip.zip|bubsys.zip|namcoc69.zip|namcoc70.zip|namcoc75.zip|qsound.zip|qsound_hle.zip"},
    {"mame","mame",""}
};
const CoreProfile* coreProfile(const QString& platform,const QString& core) {
    for(const auto& p:cores)if(core==p.core&&QString::fromLatin1(p.platforms).split('|').contains(platform))return &p;
    return nullptr;
}
QString multiPadLayout(const QString& platform,const QString& core) {
    if(!coreProfile(platform,core))return {};
    if(core=="snes9x")return "snes-multitap";
    if(core=="mednafen_pce_fast")return "pce-five-pad";
    if(core=="mednafen_supergrafx")return "sgx-multitap";
    if(core=="fbneo"&&platform=="fbneo")return "arcade-four-pad";
    // Mega Drive needs a game-specific Team Player / 4-Way Play decision;
    // NES needs Four Score / Famicom expansion selection. Counts cannot choose.
    return {};
}
QString identityLayout(const QJsonObject& identity) {
    const auto parts=identity["id"].toString().split('.');
    if(parts.size()!=5||parts[0]!="runtime"||parts[1]!="retroarch"||parts[4]!="v1")return {};
    const auto layout=multiPadLayout(parts[2],parts[3]);
    if(layout.isEmpty()||identity["settings"]!=parts[3]+'-'+layout+"-no-sram-v1")return {};
    return layout;
}
QString digest(const QString& path,qint64 limit,const std::atomic_bool& cancel,NetplayDigestCache* cache) {
    const QFileInfo f(path);
    const auto key=f.absoluteFilePath()+'|'+QString::number(f.size())+'|'+QString::number(f.lastModified().toMSecsSinceEpoch());
    if(cancel||!f.isFile()||f.size()<=0||f.size()>limit)return {};
    if(cache&&cache->contains(key))return cache->value(key);
    const auto value=fileDigest(path,limit,cancel);
    if(cache&&!value.isEmpty())cache->insert(key,value);
    return value;
}
QString contentDigest(const QString& path,const std::atomic_bool& cancel,NetplayDigestCache* cache) {
    const auto extension=QFileInfo(path).suffix().toLower();
    // These are manifests, not self-contained disc images. Never hash only the
    // descriptor and accidentally accept peers whose actual tracks differ.
    if(extension=="m3u"||extension=="ccd"||extension=="toc"||extension=="cmd")return {};
    if(extension!="cue")return digest(path,2LL*1024*1024*1024,cancel,cache);
    if(!validateDiscContent(path,cancel).isEmpty())return {};
    QFile cue(path);if(!cue.open(QIODevice::ReadOnly))return {};
    const auto bytes=cue.readAll();if(cue.error()!=QFile::NoError)return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(bytes);
    const QRegularExpression entry("^FILE\\s+(?:\"([^\"]+)\"|(\\S+))\\s+\\S+\\s*$",QRegularExpression::CaseInsensitiveOption);
    for(const auto& line:QString::fromUtf8(bytes).split('\n')) {
        const auto m=entry.match(line.trimmed());if(!m.hasMatch())continue;
        const auto name=m.captured(1).isEmpty()?m.captured(2):m.captured(1);
        const auto value=digest(QFileInfo(path).dir().filePath(name),2LL*1024*1024*1024,cancel,cache);
        if(value.isEmpty())return {};
        hash.addData(value.toLatin1());
    }
    return cancel?QString():QString::fromLatin1(hash.result().toHex());
}
struct Profile {const char *platform,*core,*id,*title,*rom,*archive;int players=2;};
const Profile profiles[] = {
    {"nes","fceumm","runtime.fceumm.pong-homebrew.v1","NES Pong",
     "b9116433d8f5d3293adfe871b47af68198e1596d40eccc2c3a99b14e2ca2afe0",""},
    {"nes","fceumm","runtime.fceumm.homebrew-war-2025.v1","Super Homebrew War",
     "063eec9f883b44a0a11aa63238316d4e676034aab72eae0b9a092ed50f2bceed","",4},
    {"snes","snes9x","runtime.snes9x.contra3-us.v1","Contra III",
     "a93ea87fc835c530b5135c5294433d15eef6dbf656144b387e89ac19cf864996",
     "9eab8d9bdae35e13a1459d1ccddbfb3efd7192817a09102b9abccbbc6e5f71ea"},
    {"megadrive","genesis_plus_gx","runtime.genesis.gunstar-us.v1","Gunstar Heroes",
     "f177810ce614be21c1a9214c0ca4d8f8d357b04a497c02fae185e4b2f97b6b87",
     "626061be86f7eb488da0eb0ef011c5a13e8a951cf6bb039340b4774c25a039bd"},
    {"megadrive","genesis_plus_gx","runtime.genesis.streets2-us.v1","Streets of Rage 2",
     "4a314edbfee92282850fe95c4c764921916efd9d3c2277fdec2581279b1369b1",
     "2a0f30f0aa6bc2d4e500361d972addf1e98537b14bf8e6cb59d3202d797306c4"}
};
bool token(const QString& s,int max) {
    return !s.isEmpty() && s.size()<=max && QRegularExpression("^[A-Za-z0-9_-]+$").match(s).hasMatch();
}
}
bool netplaySupported(const QString& platform,const QString& core) {return coreProfile(platform,core)!=nullptr;}
int netplayCapacity(const QString& platform,const QString& core,const QVariantMap& metadata) {
    const auto players=gamePlayers(metadata);
    // Arcade metadata gives the title maximum, not the current cabinet mode.
    // Keep automatic admission at two until the game-specific mode is prepared.
    if(core=="fbneo"||players.mode=="Taking turns"||multiPadLayout(platform,core).isEmpty())return 2;
    return qBound(2,players.maximum,4);
}
QJsonObject netplayProfile(QString platform,QString core,QString content,int players) {
    if(players<2||players>4)return {};
    for(const auto& p:profiles)if(platform==p.platform&&core==p.core&&(content==p.rom||(*p.archive&&content==p.archive))) {
        QJsonObject result{{"id",p.id},{"label",p.title},{"content",p.rom},
                          {"settings",core+"-default-no-sram-v3"}};
        if(p.players==4) {
            result["players"]=4;
            result["settings"]="fceumm-four-score-no-sram-v2";
        }
        return result;
    }
    if(!netplaySupported(platform,core)||!QRegularExpression("^[0-9a-f]{64}$").match(content).hasMatch())return {};
    const auto layout=players>2?multiPadLayout(platform,core):QString("two-pad");
    if(layout.isEmpty())return {};
    return {{"id","runtime.retroarch."+platform+'.'+core+".v1"},{"content",content},
            {"settings",core+'-'+layout+"-no-sram-v1"},{"players",players}};
}
QByteArray netplayControllers(const QJsonObject& identity,bool host,int slot) {
    const int players=identity["players"].toInt(2);
    if(players<2||players>4)return {};
    if(!slot&&players==2)slot=host?1:2; // Older two-player callers.
    if((host&&slot!=1)||(!host&&(slot<2||slot>players)))return {};
    QByteArray result="netplay_max_connections = \""+QByteArray::number(players-1)+"\"\n";
    if(players>2) {
        const auto layout=identityLayout(identity);
        const bool fourScore=players==4&&identity["settings"]=="fceumm-four-score-no-sram-v2";
        if(!fourScore&&layout.isEmpty())return {};
        const int ports=fourScore||layout=="snes-multitap"||layout=="pce-five-pad"||layout=="sgx-multitap"?5:4;
        result+="input_max_users = \""+QByteArray::number(ports)+"\"\n";
    }
    if(players==4||identity["id"].toString().startsWith("runtime.retroarch.")) {
        // Each instance uses its first local pad for exactly its assigned port.
        // Clear inherited requests so a guest cannot accidentally take two seats.
        for(int p=1;p<=16;++p)
            result+="netplay_request_device_p"+QByteArray::number(p)+" = \""+(p==slot?QByteArray("true"):QByteArray("false"))+"\"\n";
    }
    return result;
}
QStringList netplayControllerArguments(const QJsonObject& identity) {
    if(identity["players"].toInt(2)>2&&!identityLayout(identity).isEmpty()) {
        const auto layout=identityLayout(identity);
        QStringList args;
        const int ports=layout=="arcade-four-pad"?4:5;
        for(int port=1;port<=ports;++port)
            args<<"--device"<<QString::number(port)+':'+(layout=="snes-multitap"&&port==2?"257":"1");
        return args;
    }
    if(identity["id"].toString().startsWith("runtime.retroarch."))return {"--device","1:1","--device","2:1"};
    if(identity["players"].toInt(2)!=4||identity["settings"]!="fceumm-four-score-no-sram-v2")return {};
    // input_libretro_device_pN is a remap-file key, not an ordinary config key.
    // Use the supported CLI so Four Score is actually enabled, with the
    // mutually exclusive Famicom expansion controller explicitly disconnected.
    return {"--device","1:513","--device","2:513","--device","3:513","--device","4:513","--nodevice","5"};
}
QString netplayRelayEndpoint(const QByteArray& response) {
    if(response.size()>4096)return {};
    QMap<QByteArray,QByteArray> fields;
    for(const auto& line:response.split('\n')) {
        const auto end=line.indexOf('=');if(end<1)continue;
        const auto key=line.left(end).trimmed();
        if(fields.contains(key))return {};
        fields.insert(key,line.mid(end+1).trimmed());
    }
    const auto host=QString::fromLatin1(fields.value("tunnel_addr"));
    bool valid=false;const auto port=fields.value("tunnel_port").toUInt(&valid);
    if(fields.value("status")!="OK"||!valid||port<1||port>65535||host.size()>253||
       !QRegularExpression("^[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*$").match(host).hasMatch())return {};
    return host+'|'+QString::number(port); // RetroArch's custom-relay host|port format.
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const RetroArchInstallation& i,const std::atomic_bool& cancel,NetplayDigestCache* cache,int players) {
    const auto coreId=r.integrationConfig["core"].toString();
    const auto p=coreProfile(r.adventure.platformId,coreId);
    if(!p)return {};
    auto identity=netplayProfile(r.adventure.platformId,coreId,contentDigest(r.contentPath,cancel,cache),players);
    if(identity.isEmpty())return {};
    const auto core=digest(i.cores.value(coreId),512LL*1024*1024,cancel,cache);
    const auto runtime=digest(i.runtimeFile,512LL*1024*1024,cancel,cache);
    if(core.isEmpty()||runtime.isEmpty())return {};
    if(identity["id"].toString().startsWith("runtime.retroarch.")) {
        identity["label"]=r.adventure.title.left(96);
        QJsonObject firmware;
        const auto system=configuredPath(readSettings(i.configFile),"system_directory");
        for(const auto& name:QString::fromLatin1(p->firmware).split('|',Qt::SkipEmptyParts)) {
            for(const auto& prefix:QStringList{QString(),coreId+'/'}) {
                const auto candidate=QDir(system).filePath(prefix+name);
                if(system.isEmpty()||!QFileInfo(candidate).exists())continue;
                const auto value=digest(candidate,256LL*1024*1024,cancel,cache);
                if(value.isEmpty())return {};
                firmware[prefix+name]=value;
            }
            // Arcade cores also search alongside the game archive.
            if(coreId=="fbneo") {
                const auto candidate=QFileInfo(r.contentPath).dir().filePath(name);
                if(QFileInfo(candidate).exists()) {
                    const auto value=digest(candidate,256LL*1024*1024,cancel,cache);
                    if(value.isEmpty())return {};
                    firmware["content/"+name]=value;
                }
            }
        }
        // Keep the existing provider message budget independent of BIOS count.
        identity["firmware"]=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(firmware).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
    }
    identity["core"]=core;identity["runtime"]=runtime;return identity;
}
QString prepareNetplay(ProcessCommand& cmd,const AdventureRegistration& r,const RetroArchInstallation& i,
                      const NetplayRequest& request,const std::atomic_bool& cancel) {
    const auto identity=netplayIdentity(r,i,cancel,nullptr,request.expected["players"].toInt(2));
    if(identity.isEmpty())return "Multiplayer isn't supported for this game version yet.";
    if(!sameMultiplayerGame(identity,request.expected))return "Your game or emulator changed. Invite your friend again.";
    const auto controllers=netplayControllers(identity,request.host,request.slot);
    if(controllers.isEmpty())return "This multiplayer controller assignment is invalid.";
    if(!token(request.password,32)||!token(request.nickname,32)||!request.port)
        return "This multiplayer invitation is invalid.";
    if(request.host&&request.relay) {
        const auto parts=request.relayEndpoint.split('|');
        if(parts.size()!=2||netplayRelayEndpoint("status=OK\ntunnel_addr="+parts[0].toLatin1()+
            "\ntunnel_port="+parts[1].toLatin1())!=request.relayEndpoint)
            return "This online relay address is invalid.";
    }
    if(!request.host) {
        // Only an explicit authenticated/accepted invitation supplies an endpoint.
        // No shell command, path or emulator option may enter through this field.
        if(request.address.isEmpty()||request.address.size()>253||!QRegularExpression("^[A-Za-z0-9.-]+$").match(request.address).hasMatch())
            return "This multiplayer address is invalid.";
        if(request.relay && (!request.clientPort || request.relaySession.size()!=16 || !QRegularExpression("^[A-Za-z0-9+/]{16}$").match(request.relaySession).hasMatch()))
            return "This multiplayer room is invalid.";
    }
    const auto base=QFileInfo(i.configFile).absolutePath();
    if(!safePath(base)||QFileInfo(base).isSymLink())return "Couldn't prepare multiplayer settings.";
    auto directory=std::make_shared<QTemporaryDir>(base+"/.traineros-netplay-XXXXXX");
    if(!directory->isValid())return "Couldn't prepare multiplayer settings.";
    const auto path=directory->path();
    QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    const auto write=[&](const QString& name,const QByteArray& bytes){
        QSaveFile f(path+'/'+name);
        return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();
    };
    const QByteArray options=identityLayout(identity)=="sgx-multitap"?QByteArray("sgx_multitap = \"enabled\"\n"):QByteArray();
    if(!write("core.opt",options))return "Couldn't prepare multiplayer settings.";
    // RetroArch otherwise prefers config/<core>/<core>.opt over core_options_path
    // and writes it on exit, even with config_save_on_exit disabled. Select the
    // global option-file mode, but point it at this session's private empty file.
    QByteArray bytes="config_save_on_exit = \"false\"\nglobal_core_options = \"true\"\n"
        "auto_overrides_enable = \"false\"\nauto_remaps_enable = \"false\"\n"
        "game_specific_options = \"false\"\nrun_ahead_enabled = \"false\"\nrewind_enable = \"false\"\n"
        "preemptive_frames_enable = \"false\"\ncheevos_enable = \"false\"\n"
        "savestate_auto_save = \"false\"\nsavestate_auto_load = \"false\"\nautosave_interval = \"0\"\n"
        "history_list_enable = \"false\"\nnetplay_nat_traversal = \"false\"\n"
        "netplay_start_as_server = \"false\"\nnetplay_start_as_client = \"false\"\n"
        "netplay_allow_slaves = \"false\"\n"
        "netplay_start_as_spectator = \"false\"\nnetplay_share_digital = \"0\"\n"
        "pause_nonactive = \"false\"\n";
    bytes+=controllers;
#ifdef Q_OS_LINUX
    // The first handheld profile is verified with InputPlumber's virtual pad
    // through udev on both Flip and Odin. Flip's inherited SDL2 driver handled
    // the RetroArch menu but did not establish gameplay input in the paired run.
    // Keep this override session-local: do not rewrite the user's ordinary
    // driver, bindings or per-game configuration while joining a friend.
    bytes+="input_joypad_driver = \"udev\"\n";
#endif
    bytes+="netplay_use_mitm_server = \""+QByteArray(request.host&&request.relay?"true":"false")+"\"\n";
    bytes+="netplay_public_announce = \""+QByteArray(request.host&&request.relay?"true":"false")+"\"\n";
    if(request.host&&request.relay)
        bytes+="netplay_mitm_server = \"custom\"\nnetplay_custom_mitm_server = \""+request.relayEndpoint.toLatin1()+"\"\n";
    // Hosting keeps the invitation password. The relay guest's loopback bridge
    // answers the challenge automatically; no password is written to its config.
    const auto password=request.relay&&request.host?request.password.toUtf8():QByteArray();
    bytes+="netplay_password = \""+password+"\"\nnetplay_spectate_password = \""+password+"\"\n";
    bytes+="core_options_path = \""+path.toUtf8()+"/core.opt\"\nsavefile_directory = \""+path.toUtf8()+"\"\nsavestate_directory = \""+path.toUtf8()+"\"\n";
    if(!write("session.cfg",bytes))return "Couldn't prepare multiplayer settings.";
    const int append=cmd.arguments.indexOf("--appendconfig");
    if(append>=0&&append+1<cmd.arguments.size())cmd.arguments[append+1]+="|"+path+"/session.cfg";
    else {auto content=cmd.arguments.takeLast();cmd.arguments<<"--appendconfig"<<path+"/session.cfg"<<content;}
    auto content=cmd.arguments.takeLast();
    const auto port=request.relay&&!request.host?request.clientPort:request.port;
    cmd.arguments<<netplayControllerArguments(identity);
    cmd.arguments<<"--verbose"<<"--no-patch"<<"--sram-mode"<<"noload-nosave"<<"--nick"<<request.nickname<<"--port"<<QString::number(port);
    if(request.host)cmd.arguments<<"--host";
    else cmd.arguments<<"--connect"<<(request.relay?QString("127.0.0.1"):request.address);
    cmd.arguments<<content;
    cmd.runtimeControls["netplay"]=identity.toVariantMap();
    cmd.runtimeControls["netplayHost"]=request.host;
    // This session enforces noload-nosave and private save/state directories.
    cmd.runtimeControls["temporaryProgress"]=true;
    const auto settled=cmd.settled;
    cmd.settled=[settled,directory](const ProcessOutcome& outcome){if(settled)settled(outcome);directory->remove();};
    return {};
}
}
