#include "SqliteScreens.h"
#include "features/home/ExitImage.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QSqlQuery>
#include <QUuid>

namespace trainer {
namespace {
QString failure(){return "Couldn't save your photos. Check free space or storage access.";}
QString digest(const QByteArray& bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
QImage decoded(const QByteArray& bytes) {
    if(bytes.isEmpty() || bytes.size()>524288)return {};
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer,"JPEG");const auto size=reader.size();
    if(size.width()<1 || size.height()<1 || size.width()>960 || size.height()>540)return {};
    return frameExitImage(reader.read());
}
}
QString migrateScreens(QSqlDatabase& db) {
    if(!db.transaction())return failure();
    QSqlQuery q(db);QString error;
    for(const auto& sql:QStringList{
        "CREATE TABLE screens(trainer_id TEXT NOT NULL REFERENCES trainer_profile(id) ON DELETE CASCADE,id TEXT NOT NULL,adventure_id TEXT NOT NULL,title TEXT NOT NULL,domain TEXT NOT NULL,registration_revision INTEGER NOT NULL,build_sha256 TEXT NOT NULL,captured_at TEXT NOT NULL,kind TEXT NOT NULL CHECK(kind='exit'),width INTEGER NOT NULL CHECK(width BETWEEN 1 AND 960),height INTEGER NOT NULL CHECK(height BETWEEN 1 AND 540),jpeg BLOB NOT NULL CHECK(length(jpeg) BETWEEN 1 AND 524288),image_sha256 TEXT NOT NULL,favorite INTEGER NOT NULL CHECK(favorite IN(0,1)),PRIMARY KEY(trainer_id,id))",
        "CREATE INDEX screens_order ON screens(trainer_id,favorite,captured_at DESC,id DESC)",
        "CREATE TABLE screen_owners(trainer_id TEXT PRIMARY KEY NOT NULL REFERENCES trainer_profile(id) ON DELETE CASCADE)",
        "PRAGMA user_version=15"})if(!q.exec(sql)){error=failure();break;}
    if(error.isEmpty() && !db.commit())error=failure();
    if(!error.isEmpty())db.rollback();
    return error;
}
QString appendScreen(QSqlDatabase& db,const ExitMedia& media,const QString& title,bool favorite) {
    const auto frame=decoded(media.jpeg);
    if(frame.isNull() || media.sessionId.isEmpty() || !media.capturedAt.isValid())return failure();
    QSqlQuery q(db);
    q.prepare("INSERT INTO screens VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(trainer_id,id) DO NOTHING");
    for(const auto& v:QVariantList{media.trainerId,media.sessionId,media.adventureId,title,media.domain,media.registrationRevision,media.buildSha256,
        media.capturedAt.toUTC().toString(Qt::ISODateWithMs),"exit",frame.width(),frame.height(),media.jpeg,digest(media.jpeg),favorite?1:0})q.addBindValue(v);
    return q.exec()?QString():failure();
}
QString pruneScreens(QSqlDatabase& db,const QString& owner) {
    QSqlQuery q(db);
    q.prepare("DELETE FROM screens WHERE trainer_id=? AND favorite=0 AND id IN (SELECT id FROM screens WHERE trainer_id=? AND favorite=0 ORDER BY captured_at DESC,id DESC LIMIT -1 OFFSET 100)");
    q.addBindValue(owner);q.addBindValue(owner);return q.exec()?QString():failure();
}
QString seedScreens(QSqlDatabase& db,const QString& owner,const QString& directory,const QList<ExitMedia>& media) {
    if(owner.isEmpty())return {};
    QSqlQuery q(db);q.prepare("SELECT 1 FROM trainer_profile WHERE id=?");q.addBindValue(owner);
    if(!q.exec())return failure();
    if(!q.next())return {};
    q.finish();
    q.prepare("SELECT 1 FROM screen_owners WHERE trainer_id=?");q.addBindValue(owner);
    if(!q.exec())return failure();
    if(q.next())return {};
    q.finish();
    QSet<QString> stars;
    QFile file(QDir(directory).filePath("screens/"+digest(owner.toUtf8())+".json"));
    if(file.exists()) {
        if(file.size()>1024*1024 || !file.open(QIODevice::ReadOnly))return failure();
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);
        if(parse.error!=QJsonParseError::NoError || !doc.isArray())return failure();
        for(const auto& value:doc.array())if(value.isString())stars.insert(value.toString());
    }
    if(!db.transaction())return failure();
    QString error;
    for(const auto& item:media) {
        q.prepare("SELECT title FROM adventures WHERE id=?");q.addBindValue(item.adventureId);
        if(!q.exec() || !q.next()){error=failure();break;}
        const auto title=q.value(0).toString();q.finish();
        error=appendScreen(db,item,title,stars.contains(item.sessionId));if(!error.isEmpty())break;
    }
    if(error.isEmpty())error=pruneScreens(db,owner);
    if(error.isEmpty()){q.prepare("INSERT INTO screen_owners VALUES(?)");q.addBindValue(owner);if(!q.exec())error=failure();}
    if(error.isEmpty() && !db.commit())error=failure();
    if(!error.isEmpty())db.rollback();
    return error;
}
QString starScreen(QSqlDatabase& db,const QString& owner,const QString& id,bool favorite) {
    if(!db.transaction())return failure();
    QSqlQuery q(db);QString error;
    q.prepare("UPDATE screens SET favorite=? WHERE trainer_id=? AND id=?");q.addBindValue(favorite?1:0);q.addBindValue(owner);q.addBindValue(id);
    if(!q.exec() || q.numRowsAffected()!=1)error=failure();
    if(error.isEmpty())error=pruneScreens(db,owner);
    if(error.isEmpty() && !db.commit())error=failure();
    if(!error.isEmpty())db.rollback();
    return error;
}
QVariantList readScreens(QSqlDatabase& db,const QString& owner) {
    QVariantList rows;QSqlQuery q(db);
    q.prepare("SELECT id,adventure_id,title,captured_at,width,height,favorite FROM screens WHERE trainer_id=? ORDER BY captured_at DESC,id DESC");q.addBindValue(owner);
    if(!q.exec())return rows;
    while(q.next()) {
        const auto at=QDateTime::fromString(q.value(3).toString(),Qt::ISODateWithMs);if(!at.isValid())continue;
        rows.append(QVariantMap{{"id",q.value(0)},{"adventure",q.value(1)},{"game",q.value(2)},
            {"capturedAt",at},{"day",at.toLocalTime().toString("dd MMM yyyy")},{"clock",at.toLocalTime().toString("HH:mm")},
            {"ratio",q.value(4).toDouble()/qMax(1,q.value(5).toInt())},{"favorite",q.value(6).toBool()},
            {"source","image://screens/"+owner+"/"+q.value(0).toString()},{"caption",q.value(2).toString()+" · Exit · "+at.toLocalTime().toString("dd MMM yyyy · HH:mm")}});
    }
    return rows;
}
QImage readScreenImage(const QString& database,const QString& owner,const QString& id) {
    // Each image request owns a short-lived read-only connection in its thread.
    // No worker connection crosses threads, and no full-image album cache exists.
    const auto name=QUuid::createUuid().toString();QImage image;
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(database);db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000");
        if(db.open()) {
            QSqlQuery q(db);q.prepare("SELECT jpeg,image_sha256 FROM screens WHERE trainer_id=? AND id=? AND length(jpeg) BETWEEN 1 AND 524288");q.addBindValue(owner);q.addBindValue(id);
            if(q.exec() && q.next()){const auto bytes=q.value(0).toByteArray();if(digest(bytes)==q.value(1).toString())image=decoded(bytes);}
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);return image;
}
}
