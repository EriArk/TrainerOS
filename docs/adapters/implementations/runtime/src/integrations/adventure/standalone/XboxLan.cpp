// SPDX-License-Identifier: GPL-3.0-or-later
#include "XboxLan.h"
#include "NativeLanConnection.h"
#include "core/model/GamePlayers.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace trainer::xboxlan {
namespace {
constexpr auto Settings="xemu-system-link-owned-v1";
constexpr auto Revision="25e1ee2fd59a451f2ab57e825b4f8a5414220c8b";
constexpr auto Runtime="0a2e4ff856644ae741a7476bc9c9dcd115b681f9faae26444df74ace2d275441";
bool hex(const QString& value){return QRegularExpression("^[a-f0-9]{64}$").match(value).hasMatch();}
QJsonObject bundle(const std::atomic_bool& cancel) {
    const auto root=bundleRoot(),canonical=QFileInfo(root).canonicalFilePath();
    QFile file(root+"/manifest.json");
    if(canonical.isEmpty()||!file.open(QIODevice::ReadOnly)||file.size()>8192)return {};
    const auto m=QJsonDocument::fromJson(file.readAll()).object();
    if(m["protocol"]!=1||m["source"]!=Revision||m["settings"]!=Settings)return {};
    const auto files=m["files"].toObject();
    const QStringList expected{"xemu.AppImage","session.py","bba_endpoint","stream_transport"};
    if(files.size()!=expected.size()||files["xemu.AppImage"]!=Runtime)return {};
    for(const auto& n:expected) {
        const auto p=root+'/'+n;
        if(!QFileInfo(p).canonicalFilePath().startsWith(canonical+'/')||!hex(files[n].toString())
           ||retroarch::fileDigest(p,256LL*1024*1024,cancel)!=files[n].toString()
           ||(n!="session.py"&&!QFileInfo(p).isExecutable()))return {};
    }
    if(!QFileInfo(m["config"].toString()).isFile()||!QFileInfo("/usr/bin/python3").isExecutable())return {};
    return m;
}
}
QString bundleRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/xemu-lan";}
QJsonObject netplayIdentity(const AdventureRegistration& r,const std::atomic_bool& cancel) {
    if(r.adventure.platformId!="xbox"||r.adventure.adapterId!="xemu"||r.adventure.collectionOnly
       ||QFileInfo(r.contentPath).suffix().compare("iso",Qt::CaseInsensitive))return {};
    if(bundle(cancel).isEmpty())return {};
    const auto content=retroarch::fileDigest(r.contentPath,10LL*1024*1024*1024,cancel);
    if(!hex(content))return {};
    return {{"id","runtime.xemu.lan.v1"},{"label",r.adventure.title.left(96)},
        {"content",content},{"runtime",Runtime},{"settings",Settings},{"players",4},
        {"lateJoin",false},{"privateProgress",true}};
}
ProcessCommand command(const AdventureRegistration& r,const QJsonObject& expected,QString connection,bool host,const QStringList& inputPrefix) {
    ProcessCommand cmd;cmd.runtimeControls={{"kind","xemu"},{"game",r.adventure.id},{"netplay",expected.toVariantMap()},{"temporaryProgress",false}};
    cmd.prepare=[r,expected,connection,host,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(!sameMultiplayerGame(netplayIdentity(r,cancel),expected))return "Your Xbox game or emulator changed. Choose the game again.";
        const auto group=NativeLanConnection::read(connection);
        if(!group.valid()||group.mode!="xbox"||(group.seat==1)!=host)return "The Xbox player seats changed. Prepare again.";
        const auto m=bundle(cancel);if(m.isEmpty())return "The Xbox connection package changed.";
        auto stage=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/traineros-xbox-XXXXXX");
        if(!stage->isValid())return "Couldn't prepare private Xbox connection settings.";
        QProcess prepare;prepare.setProcessChannelMode(QProcess::MergedChannels);
        prepare.start("/usr/bin/python3",{"-I",bundleRoot()+"/session.py",m["config"].toString(),stage->path(),group.mac()});
        if(!prepare.waitForFinished(5000)||prepare.exitStatus()!=QProcess::NormalExit||prepare.exitCode()!=0) {
            prepare.kill();prepare.waitForFinished(1000);
            return "Couldn't prepare your Xbox settings. Open Xbox normally once and check its own firmware, HDD and EEPROM.";
        }
        if(cancel)return "Opening cancelled.";
        out.program="/usr/bin/env";out.arguments=inputPrefix;
        out.arguments<<bundleRoot()+"/bba_endpoint"<<connection<<bundleRoot()+"/xemu.AppImage"
            <<"-config_path"<<stage->filePath("xemu.toml")<<"-full-screen"<<"-dvd_path"<<r.contentPath
            <<"-nic"<<("socket,model=nvnet,mac="+group.mac()+",udp=127.0.0.1:TRAINEROS_XBOX_PORT,localaddr=127.0.0.1:0");
        const auto settled=out.settled;
        out.settled=[stage,settled](const ProcessOutcome& value){if(settled)settled(value);stage->remove();};
        return {};
    };return cmd;
}
}
