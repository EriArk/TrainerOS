#include "RetroArchNetplay.h"
#include "RetroArchConfiguration.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

namespace trainer::retroarch {
namespace {
const QString Content = "a93ea87fc835c530b5135c5294433d15eef6dbf656144b387e89ac19cf864996";
const QString Archive = "9eab8d9bdae35e13a1459d1ccddbfb3efd7192817a09102b9abccbbc6e5f71ea";
bool token(const QString& s,int max) {
    return !s.isEmpty() && s.size()<=max && QRegularExpression("^[A-Za-z0-9_-]+$").match(s).hasMatch();
}
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const RetroArchInstallation& i,const std::atomic_bool& cancel) {
    if(r.adventure.platformId!="snes" || r.integrationConfig["core"]!="snes9x")return {};
    const auto content=fileDigest(r.contentPath,2*1024*1024,cancel);
    if(content!=Content && content!=Archive)return {};
    const auto core=fileDigest(i.cores.value("snes9x"),64*1024*1024,cancel);
    const auto runtime=fileDigest(i.runtimeFile,256*1024*1024,cancel);
    if(core.isEmpty()||runtime.isEmpty())return {};
    return {{"id","runtime.snes9x.contra3-us.v1"},{"label","Contra III · two players"},
        {"content",Content},{"core",core},{"runtime",runtime},{"settings","snes9x-default-no-sram-v1"}};
}
QString prepareNetplay(ProcessCommand& cmd,const AdventureRegistration& r,const RetroArchInstallation& i,
                      const NetplayRequest& request,const std::atomic_bool& cancel) {
    const auto identity=netplayIdentity(r,i,cancel);
    if(identity.isEmpty())return "Multiplayer isn't supported for this game version yet.";
    if(identity!=request.expected)return "Your game or emulator changed. Invite your friend again.";
    if(!token(request.password,32)||!token(request.nickname,32)||!request.port)
        return "This multiplayer invitation is invalid.";
    if(!request.host) {
        // Only an explicit authenticated/accepted invitation supplies an endpoint.
        // No shell command, path or emulator option may enter through this field.
        if(request.address.isEmpty()||request.address.size()>253||!QRegularExpression("^[A-Za-z0-9.-]+$").match(request.address).hasMatch())
            return "This multiplayer address is invalid.";
        if(request.relay && (request.relaySession.size()!=16 || !QRegularExpression("^[A-Za-z0-9+/]{16}$").match(request.relaySession).hasMatch()))
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
        "netplay_mitm_server = \"madrid\"\npause_nonactive = \"false\"\n";
    bytes+="netplay_use_mitm_server = \""+QByteArray(request.host&&request.relay?"true":"false")+"\"\n";
    bytes+="netplay_public_announce = \""+QByteArray(request.host&&request.relay?"true":"false")+"\"\n";
    // Stock RetroArch 1.22.2 always opens its own password keyboard on clients;
    // netplay_password configures hosting, not automatic client authentication.
    // Local play is explicitly a trusted-LAN route. Internet rooms retain a
    // password until the client authentication integration is verified.
    const auto password=request.relay?request.password.toUtf8():QByteArray();
    bytes+="netplay_password = \""+password+"\"\nnetplay_spectate_password = \""+password+"\"\n";
    bytes+="core_options_path = \""+path.toUtf8()+"/core.opt\"\nsavefile_directory = \""+path.toUtf8()+"\"\nsavestate_directory = \""+path.toUtf8()+"\"\n";
    if(!write("session.cfg",bytes))return "Couldn't prepare multiplayer settings.";
    const int append=cmd.arguments.indexOf("--appendconfig");
    if(append>=0&&append+1<cmd.arguments.size())cmd.arguments[append+1]+="|"+path+"/session.cfg";
    else {auto content=cmd.arguments.takeLast();cmd.arguments<<"--appendconfig"<<path+"/session.cfg"<<content;}
    auto content=cmd.arguments.takeLast();
    cmd.arguments<<"--verbose"<<"--no-patch"<<"--sram-mode"<<"noload-nosave"<<"--nick"<<request.nickname<<"--port"<<QString::number(request.port);
    if(request.host)cmd.arguments<<"--host";
    else {cmd.arguments<<"--connect"<<request.address;if(request.relay)cmd.arguments<<"--mitm-session"<<request.relaySession;}
    cmd.arguments<<content;
    cmd.runtimeControls["netplay"]=identity.toVariantMap();
    cmd.runtimeControls["netplayHost"]=request.host;
    const auto settled=cmd.settled;
    cmd.settled=[settled,directory](const ProcessOutcome& outcome){if(settled)settled(outcome);directory->remove();};
    return {};
}
}
