#include "LibraryFileMove.h"
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSqlQuery>
#include <QStorageInfo>

namespace trainer {
namespace {
QString intentPath(const QSqlDatabase& db) {return db.databaseName()+".library-move.json";}
QString recoveryError() {return "A file move needs recovery. Reconnect the game storage and restart TrainerOS.";}
bool writeFile(const QString& path,const QByteArray& bytes) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
}
QByteArray readFile(const QString& path) {
    QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();
}
// A system root may be a mounted/linked library, but paths below it must not
// traverse links or escape into another system. A single new leaf is allowed.
bool inside(const QString& root,const QString& path,bool newLeaf=false) {
    const QFileInfo file(path);
    if(file.isSymLink())return false;
    auto resolved=file.canonicalFilePath();
    if(resolved.isEmpty() && newLeaf) {
        const auto parent=QFileInfo(file.absolutePath()).canonicalFilePath();
        if(parent.isEmpty())return false;
        resolved=QDir(parent).filePath(file.fileName());
    }
    if(resolved.isEmpty())return false;
    const auto relative=QDir(root).relativeFilePath(resolved);
    return relative!=".." && !relative.startsWith("../") && !QDir::isAbsolutePath(relative);
}
bool matches(const QString& path,const QJsonObject& item) {
    const QFileInfo f(path);
    return f.isFile() && !f.isSymLink() && f.size()==item["size"].toVariant().toLongLong()
        && f.lastModified().toMSecsSinceEpoch()==item["modified"].toVariant().toLongLong();
}
bool parseList(const QByteArray& bytes,QDomDocument& doc) {
    if(bytes.contains("<!DOCTYPE") || bytes.contains("<!ENTITY"))return false;
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
    const bool parsed=bool(doc.setContent(bytes));
#else
    const bool parsed=doc.setContent(bytes,false);
#endif
    return parsed && doc.documentElement().tagName()=="gameList";
}
}

QString recoverLibraryFileMove(QSqlDatabase& db) {
    const auto intent=intentPath(db);
    if(!QFileInfo::exists(intent))return {};
    if(QFileInfo(intent).isSymLink() || QFileInfo(intent).size()>48*1024*1024)return recoveryError();
    const auto doc=QJsonDocument::fromJson(readFile(intent));const auto job=doc.object();
    const auto root=job["root"].toString(),id=job["id"].toString();
    const auto moves=job["files"].toArray();const auto revision=job["revision"].toInt();
    if(job["version"].toInt()!=1 || id.isEmpty() || moves.isEmpty() || moves.size()>32
        || !QFileInfo(root).isDir() || QFileInfo(root).canonicalFilePath()!=root)return recoveryError();
    QSqlQuery q(db);q.prepare("SELECT content_path,revision FROM adventures WHERE id=?");q.addBindValue(id);
    if(!q.exec() || !q.next())return recoveryError();
    const auto first=moves.first().toObject();
    const bool committed=q.value(0).toString()==first["to"].toString() && q.value(1).toInt()==revision+1;
    if(!committed && (q.value(0).toString()!=first["from"].toString() || q.value(1).toInt()!=revision))return recoveryError();
    q.finish();
    // Preflight the complete set before changing anything. Never overwrite a
    // replacement left by another program, even while rolling back.
    for(const auto& value:moves) {
        const auto item=value.toObject();const auto from=item["from"].toString(),to=item["to"].toString();
        if(!inside(root,from,true) || !inside(root,to,true) || from==to)return recoveryError();
        const bool old=QFileInfo::exists(from),next=QFileInfo::exists(to);
        if(old==next || !matches(old?from:to,item) || (committed && !next))return recoveryError();
    }
    const auto xmlFiles=job["xmlFiles"].toArray();if(xmlFiles.size()>2)return recoveryError();
    for(const auto& value:xmlFiles) {
        const auto item=value.toObject();const auto path=item["path"].toString();
        if(QFileInfo(path).fileName()!="gamelist.xml" || !inside(root,path,true))return recoveryError();
        const auto before=QByteArray::fromBase64(item["before"].toString().toLatin1());
        const auto after=QByteArray::fromBase64(item["after"].toString().toLatin1()),current=readFile(path);
        if(after.isEmpty() || (current.isEmpty() && QFileInfo::exists(path)) || (current!=before && current!=after))return recoveryError();
    }
    if(!committed)for(qsizetype i=moves.size();i>0;--i) {
        const auto item=moves[i-1].toObject();const auto from=item["from"].toString(),to=item["to"].toString();
        if(QFileInfo::exists(to) && !QFile::rename(to,from))return recoveryError();
    }
    for(const auto& value:xmlFiles) {
        const auto item=value.toObject();const auto path=item["path"].toString();
        const auto wanted=QByteArray::fromBase64(item[committed?"after":"before"].toString().toLatin1());
        if(wanted.isEmpty()) {if(QFileInfo::exists(path) && !QFile::remove(path))return recoveryError();}
        else if(readFile(path)!=wanted && !writeFile(path,wanted))return recoveryError();
    }
    if(!QFile::remove(intent))return recoveryError();
    return {};
}

QString moveLibraryFile(QSqlDatabase& db,const LibraryEdit& edit) {
    const auto pending=recoverLibraryFileMove(db);if(!pending.isEmpty())return pending;
    if(!db.transaction())return "Couldn't open the library for this move.";
    const auto fail=[&](const QString& error){db.rollback();const auto recovery=recoverLibraryFileMove(db);return recovery.isEmpty()?error:recovery;};
    QSqlQuery q(db);
    if(!q.exec("SELECT 1 FROM play_sessions WHERE outcome='running' LIMIT 1"))return fail("Couldn't check the running game.");
    if(q.next())return fail("Close the running game before moving files.");
    q.finish();
    q.prepare("SELECT a.content_path,a.revision,a.platform_id,a.config FROM adventures a LEFT JOIN library_removals r ON r.adventure_id=a.id WHERE a.id=? AND r.adventure_id IS NULL");q.addBindValue(edit.id);
    if(!q.exec() || !q.next() || q.value(1).toInt()!=edit.revision)return fail("This game changed. Reopen its menu.");
    const auto source=q.value(0).toString(),platform=q.value(2).toString();
    auto config=QJsonDocument::fromJson(q.value(3).toByteArray()).object();q.finish();
    if(edit.relocated)config=edit.relocated->integrationConfig;
    if(!edit.retainedSaveBase.isEmpty()) {
        config["librarySaveBase"]=edit.retainedSaveBase;config["librarySaveSortCore"]=edit.retainedSaveSortCore;
    }
    const QFileInfo file(source);const auto root=QFileInfo(edit.storageRoot).canonicalFilePath();
    auto destination=QDir::cleanPath(edit.text);
    if(root.isEmpty() || !QFileInfo(root).isDir() || !file.isAbsolute() || !file.isFile() || file.isSymLink()
        || !inside(root,source) || !QDir::isAbsolutePath(destination) || !inside(root,destination,true))
        return fail("Choose a platform folder inside your ROM library.");
    if(QStringList{"cue","m3u","gdi","ccd","rpx"}.contains(file.suffix().toLower())
        || QStringList{"fbneo","mame","neogeo","naomi","atomiswave"}.contains(platform))
        return fail("This game may share files with other games. Move its set in Desktop Mode.");
    if(QFileInfo(destination).exists() && !QFileInfo(destination).isDir())return fail("The destination isn't a folder.");
    const auto target=QDir(destination).filePath(file.fileName());
    if(QDir::cleanPath(source)==target)return fail("This game is already in that folder.");
    const auto device=QStorageInfo(file.absolutePath());
    const auto targetDevice=QStorageInfo(QFileInfo(destination).exists()?destination:QFileInfo(destination).absolutePath());
    if(!device.isValid() || !targetDevice.isValid() || device.device()!=targetDevice.device())return fail("Choose a folder on the same storage device.");
    q.prepare("SELECT content_path FROM adventures WHERE id<>?");q.addBindValue(edit.id);
    if(!q.exec())return fail("Couldn't check related games.");
    while(q.next()) {
        const QFileInfo other(q.value(0).toString());
        if(other.canonicalFilePath()==file.canonicalFilePath() || QDir::cleanPath(other.absoluteFilePath())==target)
            return fail("Another library entry uses this file or destination.");
    }q.finish();
    QDirIterator lists(root,{"*.m3u","*.M3U","*.cue","*.CUE","*.gdi","*.GDI"},QDir::Files,QDirIterator::Subdirectories);
    while(lists.hasNext()) {
        QFile list(lists.next());
        if(list.size()>1024*1024 || !list.open(QIODevice::ReadOnly))return fail("Couldn't check the game's disc sets.");
        if(QString::fromUtf8(list.readAll()).contains(file.fileName(),Qt::CaseInsensitive))return fail("A disc set uses this file. Move the set in Desktop Mode.");
    }
    QStringList files{source};
    // Adjacent battery saves and automatic patches follow the unchanged ROM
    // basename. Central emulator/Trainer save directories remain in place.
    const QStringList extensions{"sav","srm","dsv","rtc","eep","fla","sra","ips","bps","ups"};
    for(const auto& adjacent:QDir(file.absolutePath()).entryInfoList(QDir::Files|QDir::Hidden)) {
        if(!extensions.contains(adjacent.suffix().toLower()))continue;
        if(!edit.retainedSaveBase.isEmpty() && !QStringList{"ips","bps","ups"}.contains(adjacent.suffix().toLower()))continue;
        if(adjacent.completeBaseName()!=file.completeBaseName() && adjacent.completeBaseName()!=file.fileName())continue;
        if(adjacent.isSymLink())return fail("A linked companion file needs to be moved in Desktop Mode.");
        // Two editions with the same stem can share a save/patch: don't steal it.
        for(const auto& sibling:QDir(file.absolutePath()).entryInfoList(QDir::Files))
            if(sibling.completeBaseName()==file.completeBaseName() && sibling.filePath()!=source
                && !extensions.contains(sibling.suffix().toLower()))return fail("Another file shares this game's save name. Move them in Desktop Mode.");
        files.append(adjacent.filePath());
    }
    QJsonArray moves;
    for(const auto& path:files) {
        const QFileInfo f(path);const auto to=QDir(destination).filePath(f.fileName());
        if(QFileInfo::exists(to) || QFileInfo(to).isSymLink())return fail("That folder already contains a file with this name. Nothing was moved.");
        moves.append(QJsonObject{{"from",path},{"to",to},{"size",f.size()},{"modified",f.lastModified().toMSecsSinceEpoch()}});
    }
    QJsonObject job{{"version",1},{"id",edit.id},{"revision",edit.revision},{"root",root},{"files",moves}};
    const auto oldSystem=edit.relocated?QDir(root).filePath(QDir(root).relativeFilePath(file.canonicalFilePath()).section('/',0,0)):root;
    const auto newSystem=edit.relocated?QDir(root).filePath(QDir(root).relativeFilePath(destination).section('/',0,0)):root;
    const auto xml=QDir(oldSystem).filePath("gamelist.xml");QJsonArray xmlFiles;
    if(QFileInfo::exists(xml)) {
        if(QFileInfo(xml).isSymLink() || QFileInfo(xml).size()>8*1024*1024)return fail("The game list needs to be checked in Desktop Mode.");
        const auto before=readFile(xml);QDomDocument doc;
        if(!parseList(before,doc))return fail("Couldn't read gamelist.xml. Nothing was moved.");
        bool changed=false;QDomDocument targetDoc;QByteArray targetBefore;
        const auto targetXml=QDir(newSystem).filePath("gamelist.xml");
        if(oldSystem!=newSystem) {
            if(QFileInfo::exists(targetXml)) {
                if(QFileInfo(targetXml).isSymLink() || QFileInfo(targetXml).size()>8*1024*1024)return fail("The destination game list needs to be checked.");
                targetBefore=readFile(targetXml);
                if(!parseList(targetBefore,targetDoc))return fail("Couldn't read the destination game list.");
            } else targetDoc.appendChild(targetDoc.createElement("gameList"));
            for(auto game=targetDoc.documentElement().firstChildElement("game");!game.isNull();game=game.nextSiblingElement("game"))
                if(QDir::cleanPath(QDir(newSystem).absoluteFilePath(game.firstChildElement("path").text().trimmed()))==target)
                    return fail("The destination game list already has this file. Check it in Desktop Mode.");
        }
        for(auto game=doc.documentElement().firstChildElement("game");!game.isNull();) {
            const auto next=game.nextSiblingElement("game");
            auto path=game.firstChildElement("path");const auto text=path.text().trimmed();
            if(text.isEmpty() || QFileInfo(QDir(oldSystem).absoluteFilePath(text)).canonicalFilePath()!=file.canonicalFilePath()){game=next;continue;}
            while(!path.firstChild().isNull())path.removeChild(path.firstChild());
            path.appendChild(doc.createTextNode("./"+QDir(newSystem).relativeFilePath(target)));changed=true;
            if(oldSystem!=newSystem) {
                const QStringList tags{"image","screenshot","thumbnail","marquee","fanart","titleshot","video","manual","magazine","map","bezel","cartridge","boxart","boxback","wheel","mix"};
                for(const auto& tag:tags) {
                    auto media=game.firstChildElement(tag);const auto value=media.text().trimmed();
                    if(value.isEmpty() || QDir::isAbsolutePath(value) || value.startsWith("~/") || value.contains("://"))continue;
                    const auto adjusted=QDir(newSystem).relativeFilePath(QDir(oldSystem).absoluteFilePath(value));
                    while(!media.firstChild().isNull())media.removeChild(media.firstChild());
                    media.appendChild(doc.createTextNode(adjusted));
                }
                targetDoc.documentElement().appendChild(targetDoc.importNode(game,true));doc.documentElement().removeChild(game);
            }
            game=next;
        }
        if(changed) {
            xmlFiles.append(QJsonObject{{"path",xml},{"before",QString::fromLatin1(before.toBase64())},{"after",QString::fromLatin1(doc.toByteArray(2).toBase64())}});
            if(oldSystem!=newSystem)xmlFiles.append(QJsonObject{{"path",targetXml},{"before",QString::fromLatin1(targetBefore.toBase64())},{"after",QString::fromLatin1(targetDoc.toByteArray(2).toBase64())}});
        }
    }
    job["xmlFiles"]=xmlFiles;
    if(!QDir().mkpath(destination))return fail("Couldn't create the destination folder.");
    if(!writeFile(intentPath(db),QJsonDocument(job).toJson(QJsonDocument::Compact)))return fail("Couldn't prepare the move. Nothing was moved.");
    for(const auto& value:moves) {
        const auto item=value.toObject();
        if(!matches(item["from"].toString(),item) || !QFile::rename(item["from"].toString(),item["to"].toString()))
            return fail("Couldn't move these files. The original location was restored.");
    }
    for(const auto& value:xmlFiles) {
        const auto item=value.toObject();const auto path=item["path"].toString();
        if(readFile(path)!=QByteArray::fromBase64(item["before"].toString().toLatin1())
            || !writeFile(path,QByteArray::fromBase64(item["after"].toString().toLatin1())))return fail("Couldn't update the game list.");
    }
    q.prepare("UPDATE adventures SET content_path=?,config=?,revision=revision+1 WHERE id=?");
    q.addBindValue(target);q.addBindValue(QJsonDocument(config).toJson(QJsonDocument::Compact));q.addBindValue(edit.id);
    if(!q.exec())return fail("Couldn't save the new game location.");
    if(edit.relocated) {
        const auto& adventure=edit.relocated->adventure;
        q.prepare("UPDATE adventures SET platform_id=?,adapter_id=?,catalogue_id=? WHERE id=?");
        q.addBindValue(adventure.platformId);q.addBindValue(adventure.adapterId);q.addBindValue(adventure.catalogueId.isNull()?QString(""):adventure.catalogueId);q.addBindValue(edit.id);
        if(!q.exec())return fail("Couldn't update the game's platform.");
    }
    q.prepare("UPDATE exit_media SET content_path=?,registration_revision=? WHERE adventure_id=? AND content_path=? AND registration_revision=?");
    for(const auto& v:QVariantList{target,edit.revision+1,edit.id,source,edit.revision})q.addBindValue(v);
    if(!q.exec() || !db.commit())return fail("Couldn't finish the move.");
    // A leftover intent is harmless: startup recognizes the committed new path.
    return recoverLibraryFileMove(db);
}
}
