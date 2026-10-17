#include "SqliteAchievementMoments.h"
#include "SqliteScreens.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QRegularExpression>

namespace trainer {
namespace {
QString failure(){return "Couldn't keep this achievement moment. Check free space.";}
QString key(const AchievementMoment& m) {
    return QString::fromLatin1(QCryptographicHash::hash((m.owner+'\n'+m.account.toCaseFolded()+'\n'+m.set+'\n'+m.achievement+'\n'+m.mode).toUtf8(),QCryptographicHash::Sha256).toHex());
}
}
QString migrateAchievementMoments(QSqlDatabase& db) {
    if(!db.transaction())return failure();
    QSqlQuery q(db);QString error;
    // Retain all existing photos verbatim while extending their source type.
    const QStringList sql{
        "ALTER TABLE screens RENAME TO screens_v15",
        "CREATE TABLE screens(trainer_id TEXT NOT NULL REFERENCES trainer_profile(id) ON DELETE CASCADE,id TEXT NOT NULL,adventure_id TEXT NOT NULL,title TEXT NOT NULL,domain TEXT NOT NULL,registration_revision INTEGER NOT NULL,build_sha256 TEXT NOT NULL,captured_at TEXT NOT NULL,kind TEXT NOT NULL CHECK(kind IN ('exit','achievement')),width INTEGER NOT NULL CHECK(width BETWEEN 1 AND 960),height INTEGER NOT NULL CHECK(height BETWEEN 1 AND 540),jpeg BLOB NOT NULL CHECK(length(jpeg) BETWEEN 1 AND 524288),image_sha256 TEXT NOT NULL,favorite INTEGER NOT NULL CHECK(favorite IN(0,1)),PRIMARY KEY(trainer_id,id))",
        "INSERT INTO screens SELECT * FROM screens_v15", "DROP TABLE screens_v15",
        "CREATE INDEX screens_order ON screens(trainer_id,favorite,captured_at DESC,id DESC)",
        "CREATE TABLE achievement_moments(trainer_id TEXT NOT NULL REFERENCES trainer_profile(id) ON DELETE CASCADE,id TEXT NOT NULL,account TEXT NOT NULL,set_id TEXT NOT NULL,achievement_id TEXT NOT NULL,mode TEXT NOT NULL,adventure_id TEXT NOT NULL,session_id TEXT NOT NULL,content_hash TEXT NOT NULL,observed_at TEXT NOT NULL,captured_at TEXT NOT NULL,capture_id TEXT NOT NULL,title TEXT NOT NULL,badge TEXT NOT NULL,PRIMARY KEY(trainer_id,id))",
        "PRAGMA user_version=16"};
    for(const auto& statement:sql)if(!q.exec(statement)){error=failure();break;}
    if(error.isEmpty()&&!db.commit())error=failure();
    if(!error.isEmpty())db.rollback();return error;
}
QString appendAchievementMoment(QSqlDatabase& db,const QString& owner,const AchievementMoment& m,bool& inserted) {
    inserted=false;
    static const QRegularExpression number("^[1-9][0-9]{0,9}$"),hash("^[0-9a-f]{32}$");
    if(owner!=m.owner || owner.isEmpty() || m.account.isEmpty() || m.account.size()>32
       || !number.match(m.set).hasMatch() || !number.match(m.achievement).hasMatch()
       || !hash.match(m.contentHash).hasMatch() || !QStringList{"Standard","Hardcore"}.contains(m.mode)
       || m.session.isEmpty() || m.session.size()>128 || !m.observedAt.isValid())return failure();
    QSqlQuery q(db);q.prepare("SELECT title,revision FROM adventures WHERE id=?");q.addBindValue(m.adventure);
    if(!q.exec()||!q.next())return failure();
    const auto game=q.value(0).toString();const QString domain="retroachievements";const int revision=q.value(1).toInt();q.finish();
    const auto id=key(m),capture="achievement-"+id;
    if(!db.transaction())return failure();QString error;
    q.prepare("INSERT INTO achievement_moments VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(trainer_id,id) DO NOTHING");
    const bool hasFrame=!m.frame.isNull() && m.frame.sizeInBytes()<=64*1024*1024 && m.capturedAt.isValid();
    for(const auto& v:QVariantList{owner,id,m.account.toCaseFolded(),m.set,m.achievement,m.mode,m.adventure,m.session,m.contentHash,
        m.observedAt.toUTC().toString(Qt::ISODateWithMs),hasFrame?m.capturedAt.toUTC().toString(Qt::ISODateWithMs):QString(),hasFrame?capture:QString(),m.title.left(256),m.badge.left(64)})
        q.addBindValue(v.toString().isNull()?QStringLiteral(""):v.toString());
    if(!q.exec())error=failure();else inserted=q.numRowsAffected()==1;
    if(error.isEmpty() && inserted && hasFrame) {
        const auto frame=m.frame.scaled(960,540,Qt::KeepAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
        QByteArray jpeg;QBuffer buffer(&jpeg);buffer.open(QIODevice::WriteOnly);
        if(!frame.save(&buffer,"JPEG",85))error=failure();
        // RA's content hash is not a SHA-256 build fingerprint. Its provenance
        // stays in achievement_moments; do not mislabel it in exit-media fields.
        else error=appendScreen(db,{capture,owner,domain,m.adventure,revision,QStringLiteral(""),m.capturedAt,jpeg},game,false,"achievement");
        if(error.isEmpty())error=pruneScreens(db,owner);
    }
    if(error.isEmpty()&&!db.commit())error=failure();
    if(!error.isEmpty()){db.rollback();inserted=false;}return error;
}
QVariantList readAchievementMoments(QSqlDatabase& db,const QString& owner) {
    QVariantList result;QSqlQuery q(db);
    q.prepare("SELECT m.account,m.set_id,m.achievement_id,m.mode,m.adventure_id,m.observed_at,m.capture_id,s.id,m.captured_at FROM achievement_moments m LEFT JOIN screens s ON s.trainer_id=m.trainer_id AND s.id=m.capture_id WHERE m.trainer_id=? ORDER BY m.observed_at DESC");q.addBindValue(owner);
    if(!q.exec())return result;
    while(q.next())result.append(QVariantMap{{"account",q.value(0)},{"set",q.value(1)},{"achievement",q.value(2)},{"mode",q.value(3)},
        {"adventure",q.value(4)},{"observedAt",QDateTime::fromString(q.value(5).toString(),Qt::ISODateWithMs)},
        {"capture",q.value(7)},{"expired",!q.value(6).toString().isEmpty()&&q.value(7).isNull()},
        {"source",q.value(7).isNull()?QString():"image://screens/"+owner+"/"+q.value(7).toString()},
        {"capturedAt",QDateTime::fromString(q.value(8).toString(),Qt::ISODateWithMs)}});
    return result;
}
}
