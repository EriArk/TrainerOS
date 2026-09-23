#include "TrainerPin.h"
#include <QSqlQuery>
#include <QVariant>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>

namespace trainer {
namespace {
QString storageError() { return "PIN access needs attention. Your records have been kept."; }
bool derive(const PinInput& pin, const QByteArray& salt, QByteArray& output) {
    output.resize(32);
    // Version 1 has fixed, bounded parameters: 32 MiB working memory.
    return EVP_PBE_scrypt(pin.data(),pin.size(),reinterpret_cast<const unsigned char*>(salt.constData()),
        salt.size(),32768,8,1,64ULL*1024*1024,reinterpret_cast<unsigned char*>(output.data()),output.size())==1;
}
}
PinInput::~PinInput() { clear(); }
void PinInput::clear() { OPENSSL_cleanse(digits_.data(),digits_.size());size_=0; }
void PinInput::digit(int n) { if(n>=0 && n<=9 && size_<6)digits_[size_++]=char('0'+n); }
void PinInput::erase() { if(size_)digits_[--size_]=0; }
bool PinInput::matches(const PinInput& other) const {
    return CRYPTO_memcmp(digits_.data(),other.digits_.data(),digits_.size())==0 && size_==other.size_;
}
QString migrateTrainerPins(QSqlDatabase& db) {
    QSqlQuery q(db);
    for(const auto& sql:QStringList{
        "CREATE TABLE trainer_access(trainer_id TEXT PRIMARY KEY NOT NULL REFERENCES trainer_owners(id) ON UPDATE CASCADE ON DELETE CASCADE,version INTEGER NOT NULL DEFAULT 0,salt BLOB NOT NULL DEFAULT X'',verifier BLOB NOT NULL DEFAULT X'',failures INTEGER NOT NULL DEFAULT 0 CHECK(failures BETWEEN 0 AND 20),retry_at INTEGER NOT NULL DEFAULT 0)",
        "INSERT INTO trainer_access(trainer_id) SELECT id FROM trainer_owners",
        "CREATE TRIGGER trainer_access_create AFTER INSERT ON trainer_owners BEGIN INSERT INTO trainer_access(trainer_id) VALUES(new.id); END",
        "CREATE TABLE family_access(trainer_id TEXT PRIMARY KEY NOT NULL CHECK(trainer_id='family'),version INTEGER NOT NULL DEFAULT 0,salt BLOB NOT NULL DEFAULT X'',verifier BLOB NOT NULL DEFAULT X'',failures INTEGER NOT NULL DEFAULT 0 CHECK(failures BETWEEN 0 AND 20),retry_at INTEGER NOT NULL DEFAULT 0)",
        "INSERT INTO family_access(trainer_id) VALUES('family')"})
        if(!q.exec(sql))return storageError();
    return {};
}
QString makePinRecord(const SecretPin& pin, PinRecord& record) {
    record={};
    if(!pin || !pin->size())return {};
    if(pin->size()<4 || pin->size()>6)return "Use 4 to 6 digits.";
    record.version=1;record.salt.resize(16);
    if(RAND_bytes(reinterpret_cast<unsigned char*>(record.salt.data()),record.salt.size())!=1
        || !derive(*pin,record.salt,record.verifier))return "Couldn't protect this PIN. Try again.";
    return {};
}
QString writePinRecord(QSqlDatabase& db,const QString& id,const PinRecord& record,bool family) {
    QSqlQuery q(db);
    q.prepare(QString("UPDATE %1 SET version=?,salt=?,verifier=?,failures=0,retry_at=0 WHERE trainer_id=?").arg(family?"family_access":"trainer_access"));
    q.addBindValue(record.version);q.addBindValue(record.salt.isNull()?QByteArray(""):record.salt);
    q.addBindValue(record.verifier.isNull()?QByteArray(""):record.verifier);q.addBindValue(id);
    return q.exec() && q.numRowsAffected()==1 ? QString() : storageError();
}
PinResult verifyTrainerPin(QSqlDatabase& db,const QString& id,const SecretPin& pin,qint64 now,bool family) {
    const QString table=family?"family_access":"trainer_access";
    QSqlQuery q(db);
    q.prepare(QString("SELECT version,salt,verifier,failures,retry_at FROM %1 WHERE trainer_id=?").arg(table));q.addBindValue(id);
    if(!q.exec() || !q.next())return {storageError()};
    const int version=q.value(0).toInt(), failures=q.value(3).toInt();
    const auto salt=q.value(1).toByteArray(),verifier=q.value(2).toByteArray();
    const auto retry=q.value(4).toLongLong();q.finish();
    if(version<0 || version>1 || (version==0 && (!salt.isEmpty() || !verifier.isEmpty()))
        || (version==1 && (salt.size()!=16 || verifier.size()!=32)) || failures<0 || failures>20 || retry<0)
        return {storageError()};
    if(version==0)return {};
    if(retry>now) {
        // A clock rollback must not lock the owner out indefinitely. Still wait
        // the full maximum delay; never reset the failed-attempt counter.
        if(retry-now>300) {
            q.prepare(QString("UPDATE %1 SET retry_at=? WHERE trainer_id=?").arg(table));q.addBindValue(now+300);q.addBindValue(id);
            if(!q.exec())return {storageError()};
        }
        return {QString("Try again in %1 seconds.").arg(std::min<qint64>(300,retry-now))};
    }
    QByteArray actual;
    if(!pin || !derive(*pin,salt,actual))return {"Couldn't check the PIN. Try again."};
    const bool match=CRYPTO_memcmp(actual.constData(),verifier.constData(),32)==0;
    OPENSSL_cleanse(actual.data(),actual.size());
    const int next=match?0:std::min(20,failures+1);
    const int delay=match?0:std::min(300,2 << std::min(next-1,8));
    q.prepare(QString("UPDATE %1 SET failures=?,retry_at=? WHERE trainer_id=?").arg(table));
    q.addBindValue(next);q.addBindValue(match?0:now+delay);q.addBindValue(id);
    if(!q.exec() || q.numRowsAffected()!=1)return {storageError()};
    return {match?QString():QString("That PIN didn't match. Try again in %1 seconds.").arg(delay)};
}
}
