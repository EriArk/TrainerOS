#include "RetroArchBezels.h"
#include "RetroArchConfiguration.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDateTime>
#include <algorithm>
#include <QtMath>

namespace trainer::retroarch {
namespace {
struct System { const char* id; const char* pack; const char* fallback; double aspect; };
const System systems[]{
    {"gba","GBA","Nintendo-Game-Boy-Advance",1.5},
    {"gb","GB","Nintendo-Game-Boy",10.0/9}, {"gbc","GBC","Nintendo-Game-Boy-Color",10.0/9},
    {"nes","NES","Nintendo-Entertainment-System",4.0/3},
    {"snes","SNES","Super-Nintendo-Entertainment-System",4.0/3},
    {"n64","N64","Nintendo-64",4.0/3}, {"psx","PSX","Sony-PlayStation",4.0/3},
    {"megadrive","MegaDrive","Sega-Mega-Drive",4.0/3},
    {"mastersystem","MasterSystem","Sega-Master-System",4.0/3},
    {"gamegear","GameGear","Sega-Game-Gear",4.0/3},
    {"pcengine","PCEngine","NEC-PC-Engine",4.0/3},
    {"sega32x","Sega32X","Sega-32X",4.0/3}, {"segacd","SegaCD","Sega-CD",4.0/3}
};
QString titleKey(QString name) {
    name=name.normalized(QString::NormalizationForm_D).toLower();
    name.remove(QRegularExpression("[\\x{0300}-\\x{036f}]"));
    // Strip known dump metadata only; unknown hack/subtitle tags remain significant.
    name.remove(QRegularExpression("\\((?:usa|europe|world|japan|en|fr|de|es|it|rev[ .0-9a-z]*)(?:[, +]+(?:usa|europe|world|japan|en|fr|de|es|it))*\\)"));
    name.remove(QRegularExpression("\\((?:sgb enhanced|gb compatible|gbc enhanced|gba enhanced)\\)"));
    name.remove(QRegularExpression("\\[!\\]|\\bversion\\b"));
    name.replace('&',"and");name.remove(QRegularExpression("[^a-z0-9]"));return name;
}
QImage readImage(const QString& path) {
    const QFileInfo file(path);
    if(!file.isFile()||file.isSymLink()||file.size()>32*1024*1024)return {};
    QImageReader reader(path);const auto size=reader.size();
    if(size.isEmpty()||size.width()>8192||size.height()>8192||qint64(size.width())*size.height()>20000000)return {};
    return reader.read();
}
}
AutomaticBezel automaticBezel(const BezelGame& game,const QStringList& roots,const QString& cache,QSize display,int ratio) {
    const System* system=nullptr;for(const auto& item:systems)if(game.platform==item.id){system=&item;break;}
    if(!system||display.isEmpty()||display.width()>4096||display.height()>4096||ratio==4)return {};
    const double aspect=ratio==2?4.0/3:ratio==3?16.0/9:system->aspect;
    QSize picture=display;
    if(double(display.width())/display.height()>aspect)picture.setWidth(qCeil(display.height()*aspect));
    else picture.setHeight(qCeil(display.width()/aspect));
    if(picture==display)return {};
    const QRect clear((display.width()-picture.width())/2,(display.height()-picture.height())/2,picture.width(),picture.height());
    struct Candidate { QString path; int rank; bool game; };
    QList<Candidate> candidates;
    const auto stem=QFileInfo(game.contentPath).completeBaseName();
    const auto romKey=titleKey(stem),catalogueKey=titleKey(game.catalogueTitle);
    for(const auto& root:roots) {
        if(!safePath(root))continue;
        const QDir folder(QDir(root).filePath("GameBezels/"+QString(system->pack)));
        const auto files=folder.entryInfoList({"*.png","*.PNG"},QDir::Files|QDir::Readable|QDir::NoSymLinks,QDir::Name);
        int visited=0;
        for(const auto& file:files) {
            if(++visited>8192)break;
            const auto name=file.completeBaseName(),normalized=titleKey(name);
            const int rank=name.compare(stem,Qt::CaseInsensitive)==0?0:normalized==romKey?10:
                !catalogueKey.isEmpty()&&normalized==catalogueKey?20:100;
            if(rank<100)candidates.append({file.filePath(),rank+(name.contains("USA")?0:1),true});
        }
        candidates.append({QDir(root).filePath(QString(system->fallback)+".png"),100,false});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.rank<b.rank;});
    for(const auto& candidate:candidates) {
        if(!safePath(candidate.path))continue;
        const QFileInfo source(candidate.path);if(!source.isFile())continue;
        // One replaceable output per source/display policy; mtime/size invalidate
        // its stamp, without accumulating a new PNG on every pack update.
        const auto identity=candidate.path+QString("/%1/%2/%3").arg(display.width()).arg(display.height()).arg(aspect,0,'f',6);
        const auto hash=QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex());
        const auto imagePath=QDir(cache).filePath(hash+".png"),configPath=QDir(cache).filePath(hash+".cfg");
        const auto stamp=QByteArray("# TrainerOS margin artwork v1 ")+QByteArray::number(source.size())+" "+QByteArray::number(source.lastModified().toMSecsSinceEpoch())+"\n";
        const auto config=stamp+"overlays = 1\noverlay0_overlay = \""+imagePath.toUtf8()+"\"\noverlay0_full_screen = true\noverlay0_descs = 0\n";
        QFile existing(configPath);
        if(!source.isSymLink()&&QFileInfo(imagePath).isFile()&&!QFileInfo(imagePath).isSymLink()&&existing.open(QIODevice::ReadOnly)&&existing.readAll()==config)
            return {configPath,candidate.path,candidate.game?"game":"system",clear};
        const auto original=readImage(candidate.path);if(original.isNull())continue;
        if(!safePath(cache)||QFileInfo(cache).isSymLink()||!QDir().mkpath(cache)||QFileInfo(imagePath).isSymLink()||QFileInfo(configPath).isSymLink())return {};
        QImage output(display,QImage::Format_ARGB32_Premultiplied);output.fill(Qt::transparent);
        QPainter painter(&output);painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(output.rect(),original);painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(clear,Qt::transparent);painter.end();
        QSaveFile png(imagePath);if(!png.open(QIODevice::WriteOnly)||!output.save(&png,"PNG")||!png.commit())return {};
        QSaveFile cfg(configPath);if(!cfg.open(QIODevice::WriteOnly)||cfg.write(config)!=config.size()||!cfg.commit())return {};
        return {configPath,candidate.path,candidate.game?"game":"system",clear};
    }
    return {};
}
}
