#include "PcRuntime.h"
#include <QDir>
#include <QFile>
#include <QSet>
#include "platform/process/ProcessService.h"

namespace trainer::pc {
void captureFailure(ProcessCommand& command) {
    auto tail=std::make_shared<QByteArray>();
    command.inspectOutput=[tail](const QByteArray& bytes){*tail=(*tail+bytes).right(4096);return QString();};
    command.finalize=[tail](const ProcessOutcome& result){return result.started&&result.exitCode!=0&&!result.stopped?QString::fromUtf8(*tail).trimmed().right(1200):QString();};
}
bool nativeElf(const QString& path,const QString& architecture) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly))return false;
    const auto h=f.read(20);if(h.size()!=20||!h.startsWith(QByteArray("\x7f" "ELF",4))||h[5]!=1)return false;
    const int machine=static_cast<uchar>(h[18])|(static_cast<uchar>(h[19])<<8);
    return (architecture=="arm64"&&machine==183)||(architecture=="x86_64"&&machine==62);
}
QString runtimeIssue(const QJsonObject& c,const QString& arch) {
    if(c["runner"]=="native")return {};
    if(c["runner"]=="wine") {
        if(!QFileInfo(c["wine"].toString()).isFile())return "Not installed · Wine is missing";
        if(arch!="x86_64")return "Unsupported · standalone Wine needs a verified ARM translation stack";
        if(!nativeElf(c["wine"].toString(),arch))return "Unsupported · Wine architecture does not match this device";
        return {};
    }
    if(c["runner"]!="proton")return "Unsupported · unknown runner";
    const auto root=c["proton"].toString();
    if(root.isEmpty()||!QFileInfo(root+"/proton").isFile())return "Not installed · selected Proton is missing";
    if(!nativeElf(root+"/files/bin/wine",arch))return "Unsupported · no host-native Wine in this Proton build";
    QFile manifest(root+"/toolmanifest.vdf");
    if(!manifest.open(QIODevice::ReadOnly))return "Broken · Proton manifest is missing";
    if(manifest.read(65536).contains("require_tool_appid"))return "Unsupported · this build requires a separate Steam runtime";
    if(arch=="arm64"&&!QFileInfo(root).fileName().contains("aarch64"))return "Unsupported · ARM translation stack is not identified";
    if(!QFileInfo(c["umu"].toString()).isFile())return "Not installed · UMU launcher is missing";
    return {};
}
QVariantList runtimes(const QString& state,const QString& home,const QString& arch) {
    QVariantList result;QSet<QString> seen;
    const auto umu=state+"/runtimes/umu/1.4.4/umu-run";
    const QList<QPair<QString,QString>> roots{{home+"/.local/share/Steam/compatibilitytools.d","Steam / host"},
        {home+"/.steam/root/compatibilitytools.d","Steam / host"},{home+"/.steam/steam/steamapps/common","Steam / host"},
        {state+"/runtimes/proton","TrainerOS cache"},{home+"/.local/share/lutris/runners/wine","Lutris / host"}};
    for(const auto& [root,source]:roots)for(const auto& dir:QDir(root).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name|QDir::Reversed)) {
        const auto path=dir.canonicalFilePath();if(path.isEmpty()||seen.contains(path))continue;
        const bool proton=QFileInfo(path+"/proton").isFile();const auto wine=path+"/bin/wine";
        if(!proton&&!QFileInfo(wine).isFile())continue;seen.insert(path);
        QJsonObject c{{"runner",proton?"proton":"wine"},{"proton",proton?path:QString()},{"wine",proton?QString():wine},{"umu",umu}};
        const auto issue=runtimeIssue(c,arch);
        result.append(QVariantMap{{"title",dir.fileName()},{"subtitle",issue.isEmpty()?source:issue},{"enabled",issue.isEmpty()},{"config",c.toVariantMap()}});
    }
    if(QFileInfo("/usr/bin/wine").isFile()) {
        QJsonObject c{{"runner","wine"},{"wine","/usr/bin/wine"}};const auto issue=runtimeIssue(c,arch);
        result.append(QVariantMap{{"title","System Wine"},{"subtitle",issue.isEmpty()?"Host installation":issue},{"enabled",issue.isEmpty()},{"config",c.toVariantMap()}});
    }
    return result;
}
}
