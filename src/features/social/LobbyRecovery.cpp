#include "LobbyCoordinator.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QUuid>

namespace trainer {
void LobbyCoordinator::setStorage(QString file) {
    if(file_==file)return;
    file_=std::move(file);storageError_.clear();
    if(file_.isEmpty()||self_.isEmpty())return;
    QFile input(file_);if(!input.exists()) {save();return;}
    const auto fail=[this]{storageError_="Lobby recovery is unavailable. Existing history was kept.";emit notice(storageError_);emit changed();};
    if(QFileInfo(file_).isSymLink()||input.size()>256*1024||!input.open(QIODevice::ReadOnly)) {fail();return;}
    const auto record=QJsonDocument::fromJson(input.readAll()).object();
    input.close(); // Atomic replacement must not race our own reader on Windows.
    if(record["v"]!=1||record["self"]!=self_||!record["state"].isObject()) {fail();return;}
    const auto revision=record["publicationRevision"].toInteger();
    if(revision<0||revision>9007199254740990LL) {fail();return;}
    publicationRevision_=revision;
    const auto topic=record["publicationTopic"].toString();if(!topic.isEmpty()&&topic.size()<=32)publicationTopic_=topic;
    const auto s=record["state"].toObject();
    if(s.isEmpty())return;
    const auto host=s["owner"].toString();bool member=false;
    for(const auto& value:s["members"].toArray())if(value.toObject()["id"]==self_)member=true;
    if(!validState(s,host)||QUuid(s["id"].toString()).isNull()||!member) {fail();return;}
    if(!state().isEmpty())return;
    loading_=true;
    const auto boots=record["boots"].toObject(),serials=record["serials"].toObject();
    for(const auto& value:s["members"].toArray()) {
        const auto peer=value.toObject()["id"].toString();const auto boot=boots[peer].toString();
        const auto serial=serials[peer].toInteger();
        if(!QUuid(boot).isNull()&&serial>0&&serial<=9007199254740991LL) {peerBoots_[peer]=boot;peerSerials_[peer]=quint64(serial);}
    }
    if(host==self_) {
        session_.restore(s);session_.connected(self_,online_);
        for(const auto& value:s["members"].toArray())if(value.toObject()["id"]!=self_)
            seen_[value.toObject()["id"].toString()]=QDateTime::currentSecsSinceEpoch();
    } else {
        remote_=s;host_=host;restoring_=true;connection_="Restoring lobby membership…";
        // A restored snapshot is only a hint until the host answers this boot's probe.
        remote_["ticket"]=QString();remote_["phase"]=s["game"].toObject()["id"].toString().isEmpty()?"choosing":"gathering";
        QJsonArray members;for(const auto& value:s["members"].toArray()) {
            auto m=value.toObject();m["connected"]=false;m["prepared"]=false;m["ready"]=false;members.append(m);
        }
        remote_["members"]=members;hostSeen_=QDateTime::currentSecsSinceEpoch();
    }
    loading_=false;save();emit changed();if(online_)reconnect();
}
void LobbyCoordinator::save() {
    if(file_.isEmpty()||self_.isEmpty()||loading_||!storageError_.isEmpty())return;
    const auto dir=QFileInfo(file_).absolutePath();
    const QJsonObject snapshot=hosting()?session_.state():remote_;
    QJsonObject boots,serials;
    for(const auto& value:snapshot["members"].toArray()) {
        const auto peer=value.toObject()["id"].toString();
        if(peerBoots_.contains(peer)) {boots[peer]=peerBoots_[peer];serials[peer]=qint64(peerSerials_[peer]);}
    }
    // Empty means an explicit leave/close. Never persist pending invitations or join requests.
    const auto bytes=QJsonDocument(QJsonObject{{"v",1},{"self",self_},{"state",snapshot},{"boots",boots},{"serials",serials},
        {"publicationRevision",publicationRevision_},{"publicationTopic",publicationTopic_}}).toJson(QJsonDocument::Compact);
    QSaveFile output(file_);
    bool ok=!QFileInfo(file_).isSymLink()&&!QFileInfo(dir).isSymLink()&&QDir().mkpath(dir);
    if(ok) {QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);ok=output.open(QIODevice::WriteOnly);}
    if(ok) {output.setPermissions(QFile::ReadOwner|QFile::WriteOwner);ok=output.write(bytes)==bytes.size()&&output.commit();}
    if(!ok) {storageError_="Couldn't save lobby recovery. Leave the lobby before restarting.";emit notice(storageError_);}
}
qint64 LobbyCoordinator::nextPublicationRevision() {
    if(!storageError_.isEmpty()||publicationRevision_>=9007199254740990LL)return 0;
    ++publicationRevision_;save();return storageError_.isEmpty()?publicationRevision_:0;
}
void LobbyCoordinator::reconnect() {
    if(!online_)return;
    if(hosting()) {broadcast();return;}
    if(host_.isEmpty())return;
    if(probe_.isEmpty())probe_=QUuid::createUuid().toString(QUuid::WithoutBraces);
    send(host_,restoring_||!joining_.isEmpty()?"join":"ping",{{"request",joiningRequest_},{"restore",restoring_},{"probe",probe_}});
}
bool LobbyCoordinator::remove(const QString& peer) {
    if(!hosting()||peer==self_)return false;
    // The removed player must receive an explicit outcome even though it no longer
    // belongs to the broadcast roster. Future restore requests are rejected.
    if(!session_.remove(self_,peer))return false;
    seen_.remove(peer);send(peer,"removed");save();return true;
}
}
