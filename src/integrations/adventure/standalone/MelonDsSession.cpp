#include "MelonDsSession.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace trainer::melonds {
namespace {
constexpr qint64 Limit=64*1024*1024;
QString digest(const QByteArray& bytes) {return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
bool realDirectory(const QString& path) {
    const QFileInfo info(path);
    return info.isAbsolute()&&info.isDir()&&!info.isSymLink()&&info.canonicalFilePath()==QDir::cleanPath(path);
}
bool writeNew(const QString& path,const QByteArray& bytes) {
    QFile file(path);
    return !QFileInfo(path).isSymLink()&&file.open(QIODevice::WriteOnly|QIODevice::NewOnly)
        &&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)&&file.write(bytes)==bytes.size()&&file.flush();
}
QByteArray read(const QString& path,qint64 maximum) {
    QFile file(path);const QFileInfo info(path);
    if(!info.isFile()||info.isSymLink()||info.size()<1||info.size()>maximum||!file.open(QIODevice::ReadOnly))return {};
    const auto bytes=file.read(maximum+1);
    return file.error()==QFile::NoError&&bytes.size()==info.size()?bytes:QByteArray();
}
}
QString prepareOwnedSession(ProcessCommand& command,const AdventureRegistration& record,const QString& backupRoot,
                            const SaveTargetResolver& resolve,const QString& directory,int owner,const QString& content) {
    if(owner<0||owner>1||!realDirectory(directory)||!QDir(directory).entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden).isEmpty())
        return "Couldn't prepare a private DS session.";
    if(!QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))return "Couldn't protect the session folder.";
    const auto seed=captureRuntimeSave(backupRoot,record,resolve);
    if(!seed.error.isEmpty())return seed.error;
    if(seed.target.contentRevision!=content)return "Your DS game changed. Invite again.";
    QJsonArray seats{QJsonObject{{"peer",true}},QJsonObject{{"peer",true}}};
    seats[owner]=seed.existed?QJsonObject{{"bytes",seed.bytes.size()},{"sha256",digest(seed.bytes)}}:QJsonObject{{"fresh",true}};
    if(seed.existed&&!writeNew(QDir(directory).filePath(QString("seat-%1.sav").arg(owner)),seed.bytes))
        return "Couldn't prepare your game save. Your original file was kept.";
    const QJsonObject manifest{{"version",2},{"ownerSeat",owner},{"content",content},{"seats",seats}};
    if(!writeNew(QDir(directory).filePath("session.json"),QJsonDocument(manifest).toJson(QJsonDocument::Compact)))
        return "Couldn't prepare the DS session. Your original save was kept.";
    const auto previous=command.finalize;
    command.finalize=[previous,record,backupRoot,resolve,directory,owner,content,seed](const ProcessOutcome& outcome)->QString {
        if(previous){const auto error=previous(outcome);if(!error.isEmpty())return error;}
        if(!outcome.started)return {};
        const QString kept="The DS session did not return verified progress. Your original save and session folder were kept.";
        if(outcome.crashed||outcome.stopped||outcome.exitCode!=0||!realDirectory(directory))return kept;
        QJsonParseError parse;
        const auto doc=QJsonDocument::fromJson(read(QDir(directory).filePath("owner-return.json"),4096),&parse);
        const auto receipt=doc.object();const auto size=receipt["bytes"].toInteger(-1),frame=receipt["frame"].toInteger(-1);
        if(parse.error!=QJsonParseError::NoError||!doc.isObject()||receipt.size()!=7
           ||receipt["version"]!=QJsonValue(1)||receipt["ownerSeat"]!=QJsonValue(owner)||receipt["content"]!=content
           ||frame<0||frame>0xffffffffLL||receipt["frame"]!=QJsonValue(frame)
           ||size<0||size>Limit||receipt["bytes"]!=QJsonValue(size)||!receipt["hasSave"].isBool())return kept;
        const auto path=QDir(directory).filePath("owner-return.sav");
        if(!receipt["hasSave"].toBool()) {
            return !seed.existed&&size==0&&receipt["sha256"]==digest({})&&!QFileInfo::exists(path)&&!QFileInfo(path).isSymLink()?QString():kept;
        }
        const auto bytes=read(path,Limit);
        if(bytes.isEmpty()||bytes.size()!=size||receipt["sha256"]!=digest(bytes))return kept;
        return returnRuntimeSave(backupRoot,record,seed,bytes,resolve);
    };
    return {};
}
}
