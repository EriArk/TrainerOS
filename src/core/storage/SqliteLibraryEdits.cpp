#include "SqliteLibrary.h"
#include "LibraryFileMove.h"
#include "core/input/TextEntryController.h"
#include <QSqlQuery>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <algorithm>

namespace trainer {
namespace {
bool labelValid(const QString& text, int limit) {
    return !text.trimmed().isEmpty() && TextEntryController::characterCount(text)<=limit
        && std::none_of(text.begin(),text.end(),[](QChar c){return c.isNull() || c.category()==QChar::Other_Control || c.category()==QChar::Separator_Line || c.category()==QChar::Separator_Paragraph;});
}
QString failure() { return "Couldn't save this change. Check storage and retry."; }
}
QString migrateLibraryEditing(QSqlDatabase& db) {
    if(!db.transaction())return failure();
    QSqlQuery q(db);
    for(const auto& sql:QStringList{
        "ALTER TABLE preferences ADD COLUMN world_editing INTEGER NOT NULL DEFAULT 0 CHECK(world_editing IN(0,1))",
        "CREATE TABLE library_removals(adventure_id TEXT PRIMARY KEY NOT NULL REFERENCES adventures(id), trash_path TEXT NOT NULL)",
        "PRAGMA user_version=12"})
        if(!q.exec(sql)){db.rollback();return failure();}
    if(!db.commit()){db.rollback();return failure();}return {};
}
QString editLibrary(QSqlDatabase& db, const LibraryEdit& edit) {
    if(edit.kind==LibraryEditKind::MoveFile)return moveLibraryFile(db,edit);
    if(edit.id.isEmpty())return "Choose an item first.";
    if(!db.transaction())return failure();
    const auto fail=[&](const QString& error){db.rollback();return error;};
    QSqlQuery q(db);
    if(!q.exec("SELECT 1 FROM play_sessions WHERE outcome='running' LIMIT 1"))return fail(failure());
    if(q.next())return fail("Close the running game before editing your library.");
    q.finish();
    if(edit.kind==LibraryEditKind::RenameWorld) {
        if(!labelValid(edit.text,32))return fail("Use a World name of 1–32 characters.");
        if(!q.exec("SELECT world_editing FROM preferences WHERE slot=1") || !q.next() || !q.value(0).toBool())
            return fail("Enable World editing in Settings first.");
        q.prepare("SELECT name FROM worlds WHERE id=?");q.addBindValue(edit.id);
        if(!q.exec())return fail(failure());
        if(q.next() && q.value(0).toString()!=edit.previousName)return fail("This World changed. Reopen its editor.");
        q.finish();
        q.prepare("INSERT INTO worlds(id,name,sort_order) SELECT ?,?,COALESCE(MAX(sort_order),-1)+1 FROM worlds WHERE true ON CONFLICT(id) DO UPDATE SET name=excluded.name");
        q.addBindValue(edit.id);q.addBindValue(edit.text.trimmed());
        if(!q.exec())return fail(failure());
    } else {
        q.prepare("SELECT a.revision,a.content_path,a.domain,r.adventure_id,r.trash_path FROM adventures a LEFT JOIN library_removals r ON r.adventure_id=a.id WHERE a.id=?");q.addBindValue(edit.id);
        if(!q.exec() || !q.next())return fail("This game is no longer in your library.");
        if(q.value(0).toInt()!=edit.revision)return fail("This game changed. Reopen its menu.");
        const auto path=q.value(1).toString(),domain=q.value(2).toString(),trash=q.value(4).toString();
        const bool removed=!q.value(3).isNull();q.finish();
        if(removed!=(edit.kind==LibraryEditKind::RestoreGame))return fail("This game changed. Reopen its menu.");
        if(edit.kind==LibraryEditKind::RenameGame || edit.kind==LibraryEditKind::MoveGame) {
            if(edit.kind==LibraryEditKind::RenameGame) {
                if(!labelValid(edit.text,96))return fail("Use a game name of 1–96 characters.");
                q.prepare("UPDATE adventures SET title=?,revision=revision+1 WHERE id=?");q.addBindValue(edit.text.trimmed());q.addBindValue(edit.id);
            } else {
                if(domain!="pokemon" || edit.world.id.isEmpty() || !labelValid(edit.world.name,32))return fail("Choose a Pokémon World.");
                q.prepare("INSERT OR IGNORE INTO worlds(id,name,sort_order) SELECT ?,?,COALESCE(MAX(sort_order),-1)+1 FROM worlds");
                q.addBindValue(edit.world.id);q.addBindValue(edit.world.name);
                if(!q.exec())return fail(failure());
                q.prepare("DELETE FROM adventure_worlds WHERE adventure_id=?");q.addBindValue(edit.id);
                if(!q.exec())return fail(failure());
                q.prepare("UPDATE adventures SET world_id=?,revision=revision+1 WHERE id=?");q.addBindValue(edit.world.id);q.addBindValue(edit.id);
            }
            if(!q.exec())return fail(failure());
            // Presentation edits do not change the installed content. Preserve
            // only already-current exit pictures, never resurrect stale captures.
            q.prepare("UPDATE exit_media SET registration_revision=? WHERE adventure_id=? AND registration_revision=?");
            q.addBindValue(edit.revision+1);q.addBindValue(edit.id);q.addBindValue(edit.revision);
            if(!q.exec())return fail(failure());
        } else if(edit.kind==LibraryEditKind::RemoveGame) {
            const QFileInfo file(path);
            if(!file.isAbsolute() || !file.isFile() || file.isSymLink())
                return fail("The ROM is unavailable. Reconnect its storage and retry.");
            // Multi-file installs require a reviewed file set, not recursive
            // deletion of a directory that may also contain saves or other games.
            if(QStringList{"cue","m3u","gdi","ccd","rpx"}.contains(file.suffix().toLower()))
                return fail("This game uses several files. Manage them in Desktop Mode.");
            q.prepare("SELECT content_path FROM adventures WHERE id<>?");
            q.addBindValue(edit.id);
            if(!q.exec())return fail(failure());
            while(q.next()) if(QFileInfo(q.value(0).toString()).canonicalFilePath()==file.canonicalFilePath())
                return fail("Another library entry uses this ROM. Its file has been kept.");
            q.finish();
            // A sibling playlist may use this disc even without its own entry.
            QDirIterator lists(file.absolutePath(), {"*.m3u","*.M3U","*.cue","*.CUE","*.gdi","*.GDI"}, QDir::Files);
            while(lists.hasNext()) {
                QFile list(lists.next());
                if(list.size()>1024*1024 || !list.open(QIODevice::ReadOnly))return fail("Couldn't check this game's related files. Its ROM has been kept.");
                if(QString::fromUtf8(list.readAll()).contains(file.fileName(),Qt::CaseInsensitive))
                    return fail("A disc set uses this ROM. Its file has been kept.");
            }
            // The retained row is an identity/history tombstone, not a backup.
            q.prepare("INSERT INTO library_removals(adventure_id,trash_path) VALUES(?,'')");q.addBindValue(edit.id);
            if(!q.exec())return fail(failure());
            if(!QFile::remove(path))return fail("Couldn't delete the ROM. Its file has been kept.");
            if(!db.commit())return fail("The ROM was deleted, but the library couldn't be updated. Reconnect storage and rescan.");
            return {};
        } else if(edit.kind==LibraryEditKind::RestoreGame) {
            if(trash.isEmpty() && (!QFileInfo(path).isFile() || QFileInfo(path).isSymLink()))
                return fail("This ROM was permanently deleted. Add its file again to play.");
            if(!trash.isEmpty()) {
                if(QFileInfo::exists(trash)) {
                    if(QFileInfo::exists(path))return fail("A file already occupies the original location. Move it aside before restoring.");
                    if(!QDir().mkpath(QFileInfo(path).absolutePath()) || !QFile::rename(trash,path))return fail("Couldn't restore the file. Reconnect its storage and retry.");
                } else if(!QFileInfo::exists(path))return fail("The file is unavailable. Reconnect its storage and retry.");
            }
            q.prepare("DELETE FROM library_removals WHERE adventure_id=?");q.addBindValue(edit.id);
            if(!q.exec())return fail(failure());
        }
    }
    if(!db.commit())return fail(failure());
    return {};
}
}
