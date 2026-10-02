#include "RetroArchAppearance.h"
#include "RetroArchConfiguration.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QSaveFile>
#include <QDir>
namespace trainer::retroarch {
namespace {
QString key(const QString& id){return "appearance/retroarch/"+QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(),QCryptographicHash::Sha256).toHex())+"/";}
const QStringList Ratios{"Emulator default","Original","4:3","16:9","Fill screen"};
const QStringList Filters{"Emulator default","Crisp pixels","Smooth"};
const QStringList Shaders{"Emulator default","Off","On"};
int value(QSettings& settings,const QString& prefix,const QString& name,int count){return qBound(0,settings.value(prefix+name,0).toInt(),count-1);}
}
QVariantList appearanceActions(const QString& id) {
    QSettings settings;const auto prefix=key(id);
    return {QVariantMap{{"id","ratio"},{"label","Screen · "+Ratios[value(settings,prefix,"ratio",Ratios.size())]}},
        QVariantMap{{"id","filter"},{"label","Pixels · "+Filters[value(settings,prefix,"filter",Filters.size())]}},
        QVariantMap{{"id","shader-setting"},{"label","Shader · "+Shaders[value(settings,prefix,"shader",Shaders.size())]}},
        QVariantMap{{"id","reset-appearance"},{"label","Use emulator defaults"}}};
}
bool changeAppearance(const QString& id,const QString& action) {
    if(id.isEmpty())return false;QSettings settings;const auto prefix=key(id);
    if(action=="reset-appearance"){settings.remove(prefix);settings.sync();return settings.status()==QSettings::NoError;}
    const auto name=action=="shader-setting"?QString("shader"):action;
    const int count=name=="ratio"?Ratios.size():name=="filter"?Filters.size():name=="shader"?Shaders.size():0;
    if(!count)return false;settings.setValue(prefix+name,(value(settings,prefix,name,count)+1)%count);settings.sync();return settings.status()==QSettings::NoError;
}
QString prepareAppearance(ProcessCommand& command,const QString& id,const QString& baseConfig) {
    QSettings settings;const auto prefix=key(id);
    const int ratio=value(settings,prefix,"ratio",Ratios.size()),filter=value(settings,prefix,"filter",Filters.size()),shader=value(settings,prefix,"shader",Shaders.size());
    QByteArray bytes="# TrainerOS per-game appearance; preserves the base configuration\nstdin_cmd_enable = \"true\"\n";
    if(ratio) {
        bytes+="video_force_aspect = \""+QByteArray(ratio==4?"false":"true")+"\"\n";
        if(ratio==1)bytes+="aspect_ratio_index = \"22\"\n";
        if(ratio==2||ratio==3)bytes+="aspect_ratio_index = \"20\"\nvideo_aspect_ratio = \""+QByteArray(ratio==2?"1.333333333":"1.777777778")+"\"\n";
    }
    if(filter)bytes+="video_smooth = \""+QByteArray(filter==2?"true":"false")+"\"\n";
    if(shader)bytes+="video_shader_enable = \""+QByteArray(shader==2?"true":"false")+"\"\n";
    // Share the already-visible base config directory with the RA account layer,
    // including Flatpak launches with a restricted home filesystem.
    const auto dir=QFileInfo(baseConfig).absolutePath();
    if(QFileInfo(dir).isSymLink()||!QDir().mkpath(dir))return "Couldn't save the game's display settings.";
    const auto path=dir+"/.traineros-display-"+prefix.section('/',2,2)+".cfg";
    if(!safePath(path)||path.contains('|')||QFileInfo(path).isSymLink())return "Couldn't save the game's display settings.";
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return "Couldn't save the game's display settings.";
    const int append=command.arguments.indexOf("--appendconfig");
    if(append>=0&&append+1<command.arguments.size())command.arguments[append+1]+="|"+path;
    else {const auto content=command.arguments.takeLast();command.arguments<<"--appendconfig"<<path<<content;}
    const auto base=readSettings(baseConfig);
    command.runtimeControls={{"kind","retroarch"},{"game",id},{"shader",shader?shader==2:enabled(base,"video_shader_enable")}};
    return {};
}
}
