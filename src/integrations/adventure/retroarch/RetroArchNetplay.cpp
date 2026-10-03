#include "RetroArchNetplay.h"
#include "RetroArchConfiguration.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

namespace trainer::retroarch {
namespace {
struct Profile {const char *platform,*core,*id,*title,*rom,*archive;};
const Profile profiles[] = {
    {"nes","fceumm","runtime.fceumm.pong-homebrew.v1","NES Pong",
     "b9116433d8f5d3293adfe871b47af68198e1596d40eccc2c3a99b14e2ca2afe0",""},
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
QJsonObject netplayProfile(QString platform,QString core,QString content) {
    for(const auto& p:profiles)if(platform==p.platform&&core==p.core&&(content==p.rom||(*p.archive&&content==p.archive)))
        return {{"id",p.id},{"label",p.title},{"content",p.rom},
                {"settings",core+"-default-no-sram-v2"}};
    return {};
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
QJsonObject netplayIdentity(const AdventureRegistration& r,const RetroArchInstallation& i,const std::atomic_bool& cancel) {
    const auto coreId=r.integrationConfig["core"].toString();
    bool candidate=false;for(const auto& p:profiles)if(r.adventure.platformId==p.platform&&coreId==p.core)candidate=true;
    if(!candidate)return {};
    auto identity=netplayProfile(r.adventure.platformId,coreId,fileDigest(r.contentPath,4*1024*1024,cancel));
    if(identity.isEmpty())return {};
    const auto core=fileDigest(i.cores.value(coreId),64*1024*1024,cancel);
    const auto runtime=fileDigest(i.runtimeFile,256*1024*1024,cancel);
    if(core.isEmpty()||runtime.isEmpty())return {};
    identity["core"]=core;identity["runtime"]=runtime;return identity;
}
QString prepareNetplay(ProcessCommand& cmd,const AdventureRegistration& r,const RetroArchInstallation& i,
                      const NetplayRequest& request,const std::atomic_bool& cancel) {
    const auto identity=netplayIdentity(r,i,cancel);
    if(identity.isEmpty())return "Multiplayer isn't supported for this game version yet.";
    if(identity!=request.expected)return "Your game or emulator changed. Invite your friend again.";
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
    if(!write("core.opt",{}))return "Couldn't prepare multiplayer settings.";
    QByteArray bytes="config_save_on_exit = \"false\"\n"
        "auto_overrides_enable = \"false\"\nauto_remaps_enable = \"false\"\n"
        "game_specific_options = \"false\"\nrun_ahead_enabled = \"false\"\nrewind_enable = \"false\"\n"
        "preemptive_frames_enable = \"false\"\ncheevos_enable = \"false\"\n"
        "savestate_auto_save = \"false\"\nsavestate_auto_load = \"false\"\nautosave_interval = \"0\"\n"
        "history_list_enable = \"false\"\nnetplay_nat_traversal = \"false\"\n"
        "netplay_start_as_server = \"false\"\nnetplay_start_as_client = \"false\"\n"
        "netplay_allow_slaves = \"false\"\nnetplay_max_connections = \"1\"\n"
        "netplay_start_as_spectator = \"false\"\nnetplay_share_digital = \"0\"\n"
        "pause_nonactive = \"false\"\n";
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
