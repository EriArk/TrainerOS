#include "PS2Netplay.h"
#include "RetroArchConfiguration.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>

namespace trainer::ps2 {
namespace {
AdventureRegistration networkRecord(AdventureRegistration record) {
    record.integrationConfig["core"]="pcsx2";
    return record;
}
bool candidate(const AdventureRegistration& record) {
    return record.adventure.platformId=="ps2" && !record.adventure.collectionOnly
        && QStringList{"iso","chd"}.contains(QFileInfo(record.contentPath).suffix().toLower());
}
}
QString bundleRoot() {return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/ps2-netplay";}
RetroArchInstallation installation(const RetroArchInstallation& ordinary,const std::atomic_bool& cancel,const QString& root) {
    const auto canonical=QFileInfo(root).canonicalFilePath();
    if(!retroarch::safePath(root))return {};
    QFile manifest(root+"/manifest.json");
    if(canonical.isEmpty()||QFileInfo(manifest).isSymLink()||!manifest.open(QIODevice::ReadOnly)||manifest.size()>65536)return {};
    const auto value=QJsonDocument::fromJson(manifest.readAll()).object();
    // Only a reviewed runtime package is admitted. The current native candidate
    // is intentionally not installed here: its paired speed/unload gate failed.
    if(value["protocol"]!=1||value["status"]!="reviewed"||value["settings"]!="lrps2-two-digital-private-bios-no-cards-v1"
        ||value["coreSource"]!="5ad55b00aa2331b4c4cc7d990c6c32cab3a88359"
        ||value["retroarchSource"]!="69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576"||value["stallFrames"]!=8)return {};
    const auto files=value["files"].toObject();
    const QRegularExpression sha("^[a-f0-9]{64}$");
    for(const auto& name:{"retroarch","pcsx2_libretro.so","pcsx2_libretro.info"}) {
        const auto file=root+'/'+name,expected=files[name].toString();
        if(!sha.match(expected).hasMatch()||!QFileInfo(file).canonicalFilePath().startsWith(canonical+'/')
            ||retroarch::fileDigest(file,512LL*1024*1024,cancel)!=expected)return {};
    }
    // Known failed candidate: changing only its manifest cannot promote it.
    if(QStringList{"3c6a3783ff3ef751fa2dde565c4c739def6788f3ad79e4ae6c8eb8141dde4e36",
        "961b738c09552baab78e86a75ecdc98e8fe7160d2e39718497530e79ad764d0b",
        "649afa38eb59c0c860024f3c185b87c8af50b5df637cfe46a4c738aedf512ec9"}.contains(files["pcsx2_libretro.so"].toString()))return {};
    const auto bios=value["biosFile"].toString();
    if(!retroarch::safePath(bios)||!QFileInfo(bios).isAbsolute()||QFileInfo(bios).size()!=4*1024*1024
        ||!QFileInfo(root+"/retroarch").isExecutable()||!QFileInfo(ordinary.configFile).isFile())return {};
    auto result=ordinary;
    result.runtimeFile=root+"/retroarch";result.cores={{"pcsx2",root+"/pcsx2_libretro.so"}};
    result.ps2Bios=bios;result.ps2Hotseat=value["hotseatReviewed"].toBool(false);
    result.saveBackups=false;result.saves.reset();result.resumeDirectory.clear();
    return result;
}
QJsonObject netplayIdentity(const AdventureRegistration& record,const RetroArchInstallation& ordinary,const std::atomic_bool& cancel,const QString& root) {
    if(!candidate(record))return {};
    const auto installed=installation(ordinary,cancel,root);
    if(installed.ps2Bios.isEmpty())return {};
    return retroarch::netplayIdentity(networkRecord(record),installed,cancel);
}
ProcessCommand command(const AdventureRegistration& record,const RetroArchInstallation& ordinary,const retroarch::NetplayRequest& request,const QString& root) {
    ProcessCommand result;
    result.runtimeControls={{"kind","retroarch"},{"game",record.adventure.id},{"netplay",request.expected.toVariantMap()},{"temporaryProgress",true}};
    result.prepare=[record,ordinary,request,root](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        const auto installed=installation(ordinary,cancel,root);
        if(!candidate(record)||installed.ps2Bios.isEmpty())return "PlayStation 2 multiplayer isn't ready on this device.";
        QStringList inputPrefix;const int end=ordinary.prefixArguments.indexOf("--");
        if(end>=0)inputPrefix=ordinary.prefixArguments.mid(0,end+1);
        out.program="/usr/bin/env";out.arguments={"QT_QPA_PLATFORM=xcb"};out.arguments+=inputPrefix;
        out.arguments<<"/usr/bin/flatpak"<<"run"<<"--filesystem="+root
            <<"--filesystem="+QFileInfo(installed.ps2Bios).absolutePath()
            <<"--command="+installed.runtimeFile<<"org.libretro.RetroArch"
            <<"--config"<<installed.configFile<<"--libretro"<<installed.cores.value("pcsx2")<<record.contentPath;
        const auto error=retroarch::prepareNetplay(out,networkRecord(record),installed,request,cancel);
        if(!error.isEmpty())return error;
        // Core metadata belongs to this exact bundle, including frontend status
        // queries. Do not rely on a similarly named installed ordinary core.
        const int append=out.arguments.indexOf("--appendconfig");
        if(append<0||append+1>=out.arguments.size())return "Couldn't prepare PlayStation 2 settings.";
        const auto config=out.arguments[append+1].split('|').last();
        QFile file(config);
        const auto metadata="libretro_info_path = \""+root.toUtf8()+"\"\n";
        if(!file.open(QIODevice::Append)||file.write(metadata)!=metadata.size()||!file.flush())return "Couldn't prepare PlayStation 2 metadata.";
        return {};
    };
    return result;
}
}
