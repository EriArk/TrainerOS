#include "ScreenScraper.h"
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QImage>

namespace trainer::scraper {
QString storeMedia(const QString& directory,const QString& gameId,const QByteArray& bytes,bool video,const Cancellation& cancel) {
    const auto root=QFileInfo(directory).canonicalFilePath();bool numeric=false;const auto id=gameId.toLongLong(&numeric);
    if(root.isEmpty() || !numeric || id<=0 || bytes.isEmpty() || bytes.size()>(video?64:8)*1024*1024 || (cancel && cancel->load()))return {};
    QString extension;
    if(video) {
        // Normalized ScreenScraper videos are MP4. Playback validates codecs;
        // arbitrary HTML/error documents must never become media files.
        if(bytes.size()<24 || bytes.mid(4,4)!="ftyp")return {};
        extension="mp4";
    } else {
        QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);
        const auto size=reader.size();const auto format=reader.format();
        if(!QList<QByteArray>{"png","jpeg","webp"}.contains(format) || !size.isValid() || qint64(size.width())*size.height()>16*1024*1024 || reader.read().isNull())return {};
        extension=format=="jpeg"?"jpg":QString::fromLatin1(format);
    }
    const auto folder=QDir(root).filePath(video?"videos":"images");
    if(!QDir().mkpath(folder) || !QFileInfo(folder).canonicalFilePath().startsWith(root+'/'))return {};
    const auto digest=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();
    const auto path=QDir(folder).filePath("ss-"+QString::number(id)+"-"+QString::fromLatin1(digest)+'.'+extension);
    if(QFileInfo(path).isSymLink())return {};
    if(QFileInfo::exists(path)) {
        QFile existing(path);if(!existing.open(QIODevice::ReadOnly) || existing.size()!=bytes.size() || existing.readAll()!=bytes)return {};
        return path;
    }
    QSaveFile output(path);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || (cancel && cancel->load()) || !output.commit())return {};
    return path;
}
QString writeGamelist(const QString& directory,const Fingerprint& file,const Game& game,
                      const QMap<QString,QString>& localMedia,bool refresh,const Cancellation& cancel) {
    const auto stopped=[&]{return cancel && cancel->load();};
    const auto root=QFileInfo(directory).canonicalFilePath();
    if(root.isEmpty() || !QFileInfo(root).isDir() || !file.unchanged() || !file.path.startsWith(root+'/'))
        return "The game file changed or is outside this system folder.";
    bool numeric=false;const auto id=game.id.toLongLong(&numeric);
    if(!numeric || id<=0 || game.system<=0 || game.system!=systemId(QFileInfo(root).fileName()))return "Choose a game from this system first.";
    const QSet<QString> mediaTags{"image","marquee","screenshot","fanart","video"};
    QMap<QString,QString> media;
    for(auto it=localMedia.cbegin();it!=localMedia.cend();++it) {
        const QFileInfo info(it.value());const auto canonical=info.canonicalFilePath();
        if(!mediaTags.contains(it.key()) || !info.isFile() || !info.isReadable() || !canonical.startsWith(root+'/'))
            return "Downloaded media must stay inside this system folder.";
        media[it.key()]="./"+QDir(root).relativeFilePath(canonical);
    }
    QLockFile lock(QDir(root).filePath(".traineros-scrape.lock"));
    if(!lock.tryLock(0))return "This system is already being updated.";
    const auto path=QDir(root).filePath("gamelist.xml");
    if(QFileInfo(path).isSymLink())return "The game list is a link; its target was kept unchanged.";
    const bool existed=QFileInfo::exists(path);QByteArray original;
    if(existed) {
        QFile input(path);
        if(!input.open(QIODevice::ReadOnly) || input.size()>32*1024*1024)return "Couldn't read the existing game list.";
        original=input.readAll();
    }
    QDomDocument doc;
    if(existed) {
        if(original.toUpper().contains("<!DOCTYPE") || !doc.setContent(original) || doc.documentElement().tagName()!="gameList")
            return "The existing game list is invalid; it was kept unchanged.";
    } else doc.appendChild(doc.createElement("gameList"));
    auto games=doc.documentElement();QDomElement target;
    for(auto node=games.firstChildElement("game");!node.isNull();node=node.nextSiblingElement("game")) {
        const auto entryPath=node.firstChildElement("path");
        if(!entryPath.nextSiblingElement("path").isNull())return "A game has ambiguous file paths; the list was kept unchanged.";
        const auto canonical=QFileInfo(QDir(root).absoluteFilePath(entryPath.text())).canonicalFilePath();
        if(canonical==file.path) {
            if(!target.isNull())return "The game has duplicate entries; the list was kept unchanged.";
            target=node;
        }
    }
    const bool added=target.isNull();
    if(added) {target=doc.createElement("game");games.appendChild(target);}
    auto fields=game.fields;
    const QSet<QString> allowed{"name","desc","genre","players","releasedate","developer","publisher"};
    for(auto it=fields.begin();it!=fields.end();)it=allowed.contains(it.key())?std::next(it):fields.erase(it);
    for(auto it=media.cbegin();it!=media.cend();++it)fields[it.key()]=it.value();
    if(added)fields["path"]="./"+QDir(root).relativeFilePath(file.path);
    for(auto it=fields.cbegin();it!=fields.cend();++it) {
        if(it.value().isEmpty())continue;
        auto node=target.firstChildElement(it.key());
        if(!node.isNull() && !node.nextSiblingElement(it.key()).isNull())return "A metadata field is duplicated; the list was kept unchanged.";
        // Names may have been intentionally edited in Batocera or TrainerOS.
        if(!node.isNull() && !node.text().isEmpty() && (!refresh || it.key()=="name"))continue;
        if(node.isNull()){node=doc.createElement(it.key());target.appendChild(node);}
        while(!node.firstChild().isNull())node.removeChild(node.firstChild());
        node.appendChild(doc.createTextNode(it.value().left(it.key()=="desc"?8192:1024)));
    }
    if(stopped())return "Cancelled.";
    // Catch other applications changing the XML while the proposal was built.
    QFile current(path);
    if(existed!=current.exists() || (existed && (!current.open(QIODevice::ReadOnly) || current.size()!=original.size() || current.readAll()!=original)))
        return "The game list changed; refresh before retrying.";
    current.close();
    if(!file.unchanged())return "The game file changed; refresh before retrying.";
    const auto bytes=doc.toByteArray(2);
    if(bytes.size()>32*1024*1024)return "The game list is too large.";
    QSaveFile output(path);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size())return "Couldn't save the game list.";
    if(stopped())return "Cancelled.";
    return output.commit()?QString():QString("Couldn't replace the game list; the original was kept.");
}
}
