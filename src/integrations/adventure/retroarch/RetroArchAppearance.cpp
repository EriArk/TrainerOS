#include "RetroArchAppearance.h"
#include "RetroArchConfiguration.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QSaveFile>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QSet>
#include <QImageReader>
#include <QtMath>
#include <algorithm>
namespace trainer::retroarch {
namespace {
QString key(const QString& id){return "appearance/retroarch/"+QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(),QCryptographicHash::Sha256).toHex())+"/";}
const QStringList Ratios{"Emulator default","Original","4:3","16:9","Fill screen"};
const QStringList Filters{"Emulator default","Crisp pixels","Smooth"};
const QStringList Shaders{"Emulator default","Off","On"};
int value(QSettings& settings,const QString& prefix,const QString& name,int count){return qBound(0,settings.value(prefix+name,0).toInt(),count-1);}
void removeSetting(QByteArray& bytes,const QByteArray& name) {
    QByteArray clean;for(const auto& line:bytes.split('\n'))if(!line.startsWith(name+" =")&&!line.isEmpty())clean+=line+'\n';bytes=clean;
}
QString assetId(const QString& path){return QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(),QCryptographicHash::Sha256).toHex());}
QString megaName(const QString& path) {
    if(!path.contains("/Mega_Bezel/Presets/Base_CRT_Presets/"))return {};
    const auto name=QFileInfo(path).fileName();
    if(name=="MBZ__4__STD-NO-REFLECT__GDV-MINI.slangp")return "Mega Bezel - Light";
    if(name=="MBZ__3__STD__GDV-MINI.slangp")return "Mega Bezel - Reflections";
    return {};
}
QString assetName(const QString& path){if(!megaName(path).isEmpty())return megaName(path);auto name=QFileInfo(path).completeBaseName();name.replace('_',' ');return name;}
QString hostPath(const QString& path,const QString& runtime) {
    const auto marker=runtime.indexOf("/files/");
    return path.startsWith("/app/")&&marker>=0&&runtime.contains("/org.libretro.RetroArch/")
        ?runtime.left(marker)+"/files"+path.mid(4):path;
}
// A border must leave the entire game visible. Fit the picture into its clear
// rectangular opening, rather than drawing an opaque frame over gameplay.
QRect borderViewport(const QString& path,const QString& runtime,QSize display) {
    if(display.isEmpty())return {};
    QFile preset(hostPath(path,runtime));if(!preset.open(QIODevice::ReadOnly)||preset.size()>1024*1024)return {};
    const auto text=QString::fromUtf8(preset.readAll());
    const auto match=QRegularExpression("(?m)^overlay0_overlay\\s*=\\s*\"?([^\"\\r\\n]+?)\"?\\s*$").match(text);
    if(!match.hasMatch()||!text.contains(QRegularExpression("(?m)^overlay0_full_screen\\s*=\\s*\"?true\"?\\s*$")))return {};
    const auto picture=QFileInfo(path).dir().absoluteFilePath(match.captured(1).trimmed());
    QImageReader reader(hostPath(picture,runtime));const auto size=reader.size();
    if(size.isEmpty()||size.width()>8192||size.height()>8192||qint64(size.width())*size.height()>20000000)return {};
    const auto image=reader.read().convertToFormat(QImage::Format_ARGB32);
    if(image.isNull())return {};
    const int cx=image.width()/2,cy=image.height()/2;
    auto clear=[&](int x,int y){return qAlpha(reinterpret_cast<const QRgb*>(image.constScanLine(y))[x])<=8;};
    if(!clear(cx,cy))return {};
    int left=cx,right=cx,top=cy,bottom=cy;
    while(left>0&&clear(left-1,cy))--left;while(right+1<image.width()&&clear(right+1,cy))++right;
    while(top>0&&clear(cx,top-1))--top;while(bottom+1<image.height()&&clear(cx,bottom+1))++bottom;
    // Trim antialiased corners, then reject openings that are not rectangular.
    left+=2;right-=2;top+=2;bottom-=2;
    // Handhelds need a large picture, not a miniature screen in a console mockup.
    if(right-left<image.width()/2||bottom-top<image.height()*0.85)return {};
    for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x)if(!clear(x,y))return {};
    return QRect(qCeil(double(left)*display.width()/image.width()),qCeil(double(top)*display.height()/image.height()),
        qFloor(double(right-left+1)*display.width()/image.width()),qFloor(double(bottom-top+1)*display.height()/image.height()));
}
// Built on the launch worker, never by a QML delegate or a navigation event.
QVariantList assets(const Settings& base,const QString& config,const QString& runtime,const QString& family) {
    const bool shader=family=="shader";
    const auto driver=base.value("video_driver");
    const auto suffix=driver=="vulkan"||driver=="glcore"||driver=="d3d11"||driver=="d3d12"?"*.slangp":driver=="gl"?"*.glslp":"";
    if(shader&&QString(suffix).isEmpty())return {};
    const auto local=QFileInfo(config).dir().filePath(shader?"shaders":"overlays/borders");
    auto shared=configuredPath(base,shader?"video_shader_dir":"overlay_directory");
    if(!shader&&!shared.isEmpty())shared=QDir(shared).filePath("borders");
    QString appRoot;
    const auto marker=runtime.indexOf("/files/");
    if(marker>=0&&runtime.contains("/org.libretro.RetroArch/"))appRoot=runtime.left(marker)+"/files";
    QVariantList result;QSet<QString> seen,seenMega;
    const QStringList common{"crt-pi","crt-easymode","scanlines","lcd-grid-v2","sharp-bilinear"};
    for(const auto& root:QStringList{shared,local}) {
        if(!safePath(root))continue;
        const auto host=root.startsWith("/app/")&&!appRoot.isEmpty()?appRoot+root.mid(4):root;
        if(!QDir(host).exists())continue;
        QDirIterator it(host,{shader?QString(suffix):QString("*.cfg")},QDir::Files|QDir::Readable|QDir::NoSymLinks,QDirIterator::Subdirectories);
        int visited=0;
        while(it.hasNext()&&visited++<4096) {
            const QFileInfo file(it.next());
            if(file.size()<=0||file.size()>1024*1024)continue;
            // Offer the handheld's presets and a small useful stock selection.
            // A matching file extension alone does not make every upstream
            // desktop effect suitable for a mobile GPU.
            const bool mega=!megaName(file.filePath()).isEmpty();
            if(mega&&seenMega.contains(megaName(file.filePath())))continue;
            if(shader&&root!=local&&!common.contains(file.completeBaseName())&&!mega)continue;
            // These existing device presets depict a complete miniature handheld.
            // Leave the owner's files intact, but don't offer them in this picker.
            if(shader&&QStringList{"gba-lcd","ngpc-lcd","nds-lcd","psp-lcd"}.contains(file.completeBaseName()))continue;
            const auto relative=QDir(host).relativeFilePath(file.filePath());
            auto category=QFileInfo(relative).path();category.remove(QRegularExpression("^shaders_(slang|glsl)/"));
            // Do not mistake another bezel package's internal CRT sub-preset for
            // the standalone stock effect with the same basename.
            if(shader&&root!=local&&!mega&&category.contains('/'))continue;
            const auto path=QDir(root).filePath(relative);
            if(!safePath(path)||seen.contains(path))continue;
            if(!shader) {
                QFile preset(file.filePath());if(!preset.open(QIODevice::ReadOnly))continue;
                const auto text=QString::fromUtf8(preset.readAll());
                // Borders only: do not accidentally select a virtual gamepad.
                if(!text.contains(QRegularExpression("(?m)^overlays\\s*=\\s*\"?1\"?\\s*$"))
                    ||!text.contains(QRegularExpression("(?m)^overlay0_descs\\s*=\\s*\"?0\"?\\s*$")))continue;
                if(borderViewport(path,runtime,QSize(1000,1000)).isEmpty())continue;
            }
            seen.insert(path);
            if(mega)seenMega.insert(megaName(path));
            // Keep the device's own presets within immediate controller reach.
            const int rank=mega?0:root==local?1:2;
            result.append(QVariantMap{{"id",family+":"+assetId(path)},{"label",assetName(path)},
                {"detail",mega?QString("Large picture, built-in frame"):root==local?QString("Device preset"):category=="."?QString():category},
                {"path",path},{"order",rank}});
        }
    }
    std::sort(result.begin(),result.end(),[](const QVariant& a,const QVariant& b){
        const auto left=a.toMap(),right=b.toMap();
        if(left["order"]!=right["order"])return left["order"].toInt()<right["order"].toInt();
        const auto compared=left["label"].toString().localeAwareCompare(right["label"].toString());
        return compared?compared<0:left["path"].toString()<right["path"].toString();
    });
    return result;
}
QVariantMap selectedAsset(const QVariantList& choices,const QString& path){for(const auto& item:choices)if(item.toMap()["path"]==path)return item.toMap();return {};}
}
QVariantList appearanceActions(const QString& id) {
    QSettings settings;const auto prefix=key(id);
    QVariantList actions{QVariantMap{{"id","ratio"},{"label","Screen · "+Ratios[value(settings,prefix,"ratio",Ratios.size())]}},
        QVariantMap{{"id","filter"},{"label","Pixels · "+Filters[value(settings,prefix,"filter",Filters.size())]}},
        QVariantMap{{"id","choose-shader"},{"label","Shader · "+(settings.value(prefix+"shaderPath").toString().isEmpty()?Shaders[value(settings,prefix,"shader",Shaders.size())]:assetName(settings.value(prefix+"shaderPath").toString()))}},
        QVariantMap{{"id","choose-bezel"},{"label","Frame · "+(settings.value(prefix+"bezelPath").toString().isEmpty()?(settings.value(prefix+"bezelOff").toBool()?QString("Off"):settings.value(prefix+"bezelDefault").toBool()?QString("Emulator default"):QString("Automatic")):assetName(settings.value(prefix+"bezelPath").toString()))}},
        QVariantMap{{"id","reset-appearance"},{"label","Use emulator defaults"}}};
    // Automatic artwork occupies margins, including beside Mega Bezel.
    return actions;
}
QVariantList appearanceChoices(const QString& id,const QString& family,const QVariantMap& runtime) {
    if(id.isEmpty()||(family!="shader"&&family!="bezel"))return {};
    QSettings settings;const auto prefix=key(id);const auto path=settings.value(prefix+family+"Path").toString();
    const bool off=family=="shader"?value(settings,prefix,"shader",Shaders.size())==1:settings.value(prefix+"bezelOff").toBool();
    QVariantList rows{QVariantMap{{"id",family+":default"},{"label","Emulator default"},{"detail",path.isEmpty()&&!off&&(family=="shader"||settings.value(prefix+"bezelDefault").toBool())?"Selected":""}},
        QVariantMap{{"id",family+":off"},{"label","Off"},{"detail",path.isEmpty()&&off?"Selected":""}}};
    if(family=="bezel")rows.prepend(QVariantMap{{"id","bezel:auto"},{"label","Automatic"},{"detail",path.isEmpty()&&!off&&!settings.value(prefix+"bezelDefault").toBool()?"Selected":"Game artwork, then system"}});
    for(const auto& item:runtime[family+"Choices"].toList()){auto row=item.toMap();if(row["path"]==path)row["detail"]="Selected";row.remove("path");rows.append(row);}
    return rows;
}
bool chooseAppearance(const QString& id,const QString& action,const QVariantMap& runtime) {
    const auto family=action.section(':',0,0),choice=action.section(':',1);
    if(id.isEmpty()||(family!="shader"&&family!="bezel"))return false;
    QString path;
    if(choice!="default"&&choice!="off"&&!(family=="bezel"&&choice=="auto")) {
        for(const auto& item:runtime[family+"Choices"].toList())if(item.toMap()["id"]==action)path=item.toMap()["path"].toString();
        if(!safePath(path))return false;
    }
    QSettings settings;const auto prefix=key(id);settings.setValue(prefix+family+"Path",path);
    if(family=="shader")settings.setValue(prefix+"shader",choice=="off"?1:choice=="default"?0:2);
    else {settings.setValue(prefix+"bezelOff",choice=="off");settings.setValue(prefix+"bezelDefault",choice=="default");}
    settings.sync();return settings.status()==QSettings::NoError;
}
bool changeAppearance(const QString& id,const QString& action) {
    if(id.isEmpty())return false;QSettings settings;const auto prefix=key(id);
    if(action=="reset-appearance"){settings.remove(prefix);settings.setValue(prefix+"bezelDefault",true);settings.sync();return settings.status()==QSettings::NoError;}
    const auto name=action=="shader-setting"?QString("shader"):action;
    const int count=name=="ratio"?Ratios.size():name=="filter"?Filters.size():name=="shader"?Shaders.size():0;
    if(!count)return false;settings.setValue(prefix+name,(value(settings,prefix,name,count)+1)%count);settings.sync();return settings.status()==QSettings::NoError;
}
QString prepareAppearance(ProcessCommand& command,const QString& id,const QString& baseConfig,const QString& runtimeFile,QSize displaySize,const BezelGame& game) {
    QSettings settings;const auto prefix=key(id);
    const int ratio=value(settings,prefix,"ratio",Ratios.size()),filter=value(settings,prefix,"filter",Filters.size()),shader=value(settings,prefix,"shader",Shaders.size());
    QByteArray bytes="# TrainerOS per-game appearance; preserves the base configuration\nstdin_cmd_enable = \"true\"\n";
    if(ratio) {
        bytes+="video_force_aspect = \""+QByteArray(ratio==4?"false":"true")+"\"\n";
        if(ratio==1)bytes+="aspect_ratio_index = \"22\"\n";
        if(ratio==2||ratio==3)bytes+="aspect_ratio_index = \"20\"\nvideo_aspect_ratio = \""+QByteArray(ratio==2?"1.333333333":"1.777777778")+"\"\n";
    }
    if(filter)bytes+="video_smooth = \""+QByteArray(filter==2?"true":"false")+"\"\n";
    const auto base=readSettings(baseConfig);
    const auto shaders=assets(base,baseConfig,runtimeFile,"shader"),bezels=assets(base,baseConfig,runtimeFile,"bezel");
    const auto shaderPath=settings.value(prefix+"shaderPath").toString(),bezelPath=settings.value(prefix+"bezelPath").toString();
    QString notice;
    bool megaActive=false;
    if(!shaderPath.isEmpty()) {
        if(!selectedAsset(shaders,shaderPath).isEmpty()) {
            QString appliedShader=shaderPath;
            if(!megaName(shaderPath).isEmpty()) {
                // Reference the installed upstream preset so package updates remain
                // effective. Only the shell-owned framing parameters live here.
                appliedShader=QFileInfo(baseConfig).dir().filePath(".traineros-mega-"+assetId(shaderPath)+".slangp");
                if(QFileInfo(appliedShader).isSymLink())return "Couldn't save the game's display settings.";
                QSaveFile preset(appliedShader);
                const auto content=QByteArray("#reference \"")+shaderPath.toUtf8()+"\"\nHSM_INT_SCALE_MODE = \"0\"\nHSM_NON_INTEGER_SCALE = \"96\"\nHSM_CURVATURE_MODE = \"0\"\nHSM_ASPECT_RATIO_MODE = \""+QByteArray::number(ratio==2?2:ratio==3?4:ratio==4?6:0)+"\"\n";
                if(!preset.open(QIODevice::WriteOnly)||preset.write(content)!=content.size()||!preset.commit())return "Couldn't save the game's display settings.";
                removeSetting(bytes,"aspect_ratio_index");removeSetting(bytes,"video_force_aspect");
                bytes+="aspect_ratio_index = \"24\"\nvideo_force_aspect = \"false\"\nvideo_scale_integer = \"false\"\ninput_overlay_enable = \"false\"\n";
                megaActive=true;
            }
            bytes+="video_shader_enable = \"true\"\nauto_shaders_enable = \"false\"\n";
            const auto content=command.arguments.takeLast();command.arguments<<"--set-shader"<<appliedShader<<content;
        } else notice="Selected shader unavailable; using emulator default.";
    } else if(shader)bytes+="video_shader_enable = \""+QByteArray(shader==2?"true":"false")+"\"\nauto_shaders_enable = \"false\"\n";
    if(!megaActive&&!bezelPath.isEmpty()) {
        const auto viewport=selectedAsset(bezels,bezelPath).isEmpty()?QRect():borderViewport(bezelPath,runtimeFile,displaySize);
        if(!viewport.isEmpty()) {
            bytes+="input_overlay_enable = \"true\"\ninput_overlay = \""+bezelPath.toUtf8()+"\"\ninput_overlay_hide_when_gamepad_connected = \"false\"\ninput_overlay_opacity = \"1.0\"\ninput_overlay_auto_scale = \"false\"\ninput_overlay_scale_landscape = \"1.0\"\ninput_overlay_aspect_adjust_landscape = \"0.0\"\ninput_overlay_x_offset_landscape = \"0.0\"\ninput_overlay_y_offset_landscape = \"0.0\"\n";
            bytes+="aspect_ratio_index = \"23\"\nvideo_scale_integer = \"false\"\nvideo_force_aspect = \"true\"\nvideo_viewport_bias_x = \"0.5\"\nvideo_viewport_bias_y = \"0.5\"\n";
            // RetroArch custom x/y are offsets from the biased (centered)
            // viewport, not absolute framebuffer coordinates.
            for(const auto& entry:QList<QPair<QByteArray,int>>{{"x",viewport.x()-(displaySize.width()-viewport.width())/2},{"y",viewport.y()-(displaySize.height()-viewport.height())/2},{"width",viewport.width()},{"height",viewport.height()}})
                bytes+="custom_viewport_"+entry.first+" = \""+QByteArray::number(entry.second)+"\"\n";
        }
        else notice+=(notice.isEmpty()?QString():QString(" "))+"Selected frame unavailable; using emulator default.";
    } else if(!megaActive&&settings.value(prefix+"bezelOff").toBool())bytes+="input_overlay_enable = \"false\"\n";
    AutomaticBezel automatic;
    if(bezelPath.isEmpty()&&!settings.value(prefix+"bezelOff").toBool()&&!settings.value(prefix+"bezelDefault").toBool()) {
        const auto local=QFileInfo(baseConfig).dir().filePath("overlays");
        const auto shared=hostPath(configuredPath(base,"overlay_directory"),runtimeFile);
        automatic=automaticBezel(game,{local,shared},QFileInfo(baseConfig).dir().filePath(".traineros-bezels"),displaySize,ratio);
        if(!automatic.config.isEmpty()) {
            removeSetting(bytes,"input_overlay_enable");
            if(!megaActive) {
                // A stretched inherited viewport must not put gameplay beneath
                // the side artwork. Fit at full height/width, without cropping.
                removeSetting(bytes,"aspect_ratio_index");removeSetting(bytes,"video_force_aspect");
                bytes+="aspect_ratio_index = \"23\"\nvideo_force_aspect = \"true\"\nvideo_scale_integer = \"false\"\nvideo_viewport_bias_x = \"0.5\"\nvideo_viewport_bias_y = \"0.5\"\ncustom_viewport_x = \"0\"\ncustom_viewport_y = \"0\"\n";
                bytes+="custom_viewport_width = \""+QByteArray::number(automatic.viewport.width())+"\"\ncustom_viewport_height = \""+QByteArray::number(automatic.viewport.height())+"\"\n";
            }
            bytes+="input_overlay_enable = \"true\"\ninput_overlay = \""+automatic.config.toUtf8()+"\"\ninput_overlay_hide_when_gamepad_connected = \"false\"\ninput_overlay_opacity = \"1.0\"\ninput_overlay_auto_scale = \"false\"\ninput_overlay_scale_landscape = \"1.0\"\ninput_overlay_aspect_adjust_landscape = \"0.0\"\ninput_overlay_x_offset_landscape = \"0.0\"\ninput_overlay_y_offset_landscape = \"0.0\"\n";
        }
    }
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
    command.runtimeControls={{"kind","retroarch"},{"game",id},{"shader",shader?shader==2:enabled(base,"video_shader_enable")},
        {"automaticBezelSource",automatic.source},{"automaticBezelMatch",automatic.match},{"shaderChoices",shaders},{"bezelChoices",megaActive?QVariantList():bezels},{"appearanceNotice",notice}};
    return {};
}
}
