#include "RetroArchRemotePlay.h"
#include "RetroArchConfiguration.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

namespace trainer::retroarch {
QByteArray remotePlayPortSettings(const RemotePlayRequest& request) {
    const auto name=request.ownerDevice;
    // RetroArch also accepts VID:PID rules; those would match both the virtual
    // Xbox pad and a real Xbox pad. Use an exact name, no alternative syntax.
    if(request.players<2||request.players>4||(request.snesMultitap&&request.players<3)
       ||name.isEmpty()||name.size()>128||name.startsWith("TrainerOS Remote P")
       ||name.contains(QRegularExpression("[\\x00-\\x1f\\x7f\"\\\\]"))
       ||QRegularExpression("^[0-9a-fA-F]{4}:[0-9a-fA-F]{4}").match(name).hasMatch())return {};
    QByteArray bytes="config_save_on_exit = \"false\"\n"
           "auto_overrides_enable = \"false\"\n"
           "input_remap_binds_enable = \"false\"\n"
           "input_joypad_driver = \"udev\"\n"
           "input_autodetect_enable = \"true\"\n"
           // 1.22.2 checks for a spare assignment slot BEFORE matching a
           // reservation. Keep its full device table, then disable unused core
           // ports below. A two-entry table bypasses reservation with extra pads.
           "input_max_users = \"16\"\n"
           "input_player1_device_reservation_type = \"2\"\n"
           "input_player1_reserved_device = \""+name.toUtf8()+"\"\n"
           "input_player2_device_reservation_type = \"2\"\n"
           "input_player2_reserved_device = \"TrainerOS Remote P2\"\n"
           "input_player1_joypad_index = \"0\"\n"
           "input_player2_joypad_index = \"1\"\n"
           "input_player1_analog_dpad_mode = \"0\"\n"
           "input_player2_analog_dpad_mode = \"1\"\n"
           "input_libretro_device_p1 = \"1\"\n"
           "input_libretro_device_p2 = \""+QByteArray(request.snesMultitap?"257":"1")+"\"\n"
           "input_hotkey_follows_player1 = \"true\"\n"
           "all_users_control_menu = \"false\"\n"
           "input_menu_toggle_gamepad_combo = \"0\"\n"
           "input_quit_gamepad_combo = \"0\"\n";
    for(int port=3;port<=16;++port) {
        const auto prefix="input_player"+QByteArray::number(port);
        const bool enabled=port<=request.players;
        bytes+=prefix+"_device_reservation_type = \""+(enabled?"2":"0")+"\"\n"+prefix+"_reserved_device = \""
            +(enabled?QByteArray("TrainerOS Remote P")+QByteArray::number(port):QByteArray())+"\"\n"
            +prefix+"_joypad_index = \""+QByteArray::number(port-1)+"\"\n"
            +prefix+"_analog_dpad_mode = \"1\"\n"
            +"input_libretro_device_p"+QByteArray::number(port)+" = \""+(enabled?"1":"0")+"\"\n";
    }
    return bytes;
}
QString prepareRemotePlay(ProcessCommand& cmd,const RetroArchInstallation& install,
                          const RemotePlayRequest& request,const std::atomic_bool& cancel) {
    const auto bytes=remotePlayPortSettings(request);
    if(bytes.isEmpty()||cmd.runtimeControls.contains("netplay")||cmd.runtimeControls.contains("remotePlay"))
        return "Choose a local controller before preparing Remote Play.";
    // Bound to the actually reviewed Linux udev build, not any binary with a
    // matching version string. Other backends need their own verified recipe.
    if(install.runtimeFile.isEmpty()||fileDigest(install.runtimeFile,512LL*1024*1024,cancel)!="8d57b47c88d416c05145187c8c906b7ab0013edb7e45af69d9b6bb841aea4a24")
        return "This installed runtime has no verified Remote Play controller binding.";
    const auto base=QFileInfo(install.configFile).absolutePath();
    if(cancel||!safePath(base)||QFileInfo(base).isSymLink()||cmd.arguments.isEmpty())return "Remote Play preparation cancelled.";
    auto directory=std::make_shared<QTemporaryDir>(base+"/.traineros-remoteplay-XXXXXX");
    if(!directory->isValid())return "Couldn't reserve the guest controller.";
    const auto config=directory->filePath("ports.cfg");
    QSaveFile file(config);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return "Couldn't reserve the guest controller.";
    const int append=cmd.arguments.indexOf("--appendconfig");
    if(append>=0) {
        if(append+1>=cmd.arguments.size())return "Incomplete game configuration.";
        cmd.arguments[append+1]+="|"+config;
    } else {auto content=cmd.arguments.takeLast();cmd.arguments<<"--appendconfig"<<config<<content;}
    cmd.runtimeControls["remotePlay"]=QVariantMap{{"input","retroarch-udev-ports-v2"},{"ports",request.players}};
    const auto settled=cmd.settled;
    cmd.settled=[settled,directory](const ProcessOutcome& outcome){if(settled)settled(outcome);directory->remove();};
    return {};
}
}
