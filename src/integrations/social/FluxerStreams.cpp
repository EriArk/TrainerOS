#include "FluxerSession.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>

namespace trainer {
namespace {
QString streamAccount(const QString& identity) {
    static const QRegularExpression valid("^user_([1-9][0-9]{0,19})_([A-Za-z0-9_-]{1,128})$");
    return valid.match(identity).captured(1);
}
}
bool FluxerSession::streamAvailable() const {
#ifdef Q_OS_LINUX
    return voiceAvailable()&&QFileInfo("/var/opt/traineros/integrations/traineros-game-capture").isExecutable();
#else
    return false;
#endif
}
void FluxerSession::clearStreams() {
    voiceStreams_.clear();voiceWatching_.clear();voiceSharing_=false;voiceSharePending_=false;streamStatus_.clear();mediaBlocked_.clear();
}
void FluxerSession::mediaPrivacy() {
    if(voiceState_!="connected")return;
    QSet<QString> blocked;
    for(auto it=relationships_.cbegin();it!=relationships_.cend();++it)if(it.value()["type"]==2)blocked.insert(it.key());
    if(blocked==mediaBlocked_)return;
    mediaBlocked_=blocked;QJsonArray accounts;for(const auto& id:blocked)accounts.append(id);
    voiceWrite({{"op","blocked"},{"accounts",accounts}});
}
void FluxerSession::mediaEvent(const QJsonObject& event) {
    if(voiceChannel_.isEmpty())return;
    const auto type=event["event"].toString();
    if(type=="streams") {
        voiceStreams_.clear();
        const auto rows=event["streams"].toArray();if(rows.size()>32)return;
        QSet<QString> seen;
        for(const auto& row:rows) {
            const auto identity=row.toObject()["identity"].toString(),id=streamAccount(identity);
            if(id.isEmpty()||seen.contains(identity)||relationships_.value(id)["type"]==2)continue;
            seen.insert(identity);
            QString name=label(relationships_.value(id)["user"].toObject());
            if(name.isEmpty())for(const auto& v:channels_.value(voiceChannel_)["recipients"].toArray())
                if(v.toObject()["id"]==id){name=label(v.toObject());break;}
            voiceStreams_.append(QVariantMap{{"identity",identity},{"account",id},{"name",name.isEmpty()?QString("Player"):name}});
        }
    } else if(type=="sharing") {
        voiceSharing_=event["active"].toBool();voiceSharePending_=false;
        if(voiceSharing_)streamStatus_="Sharing your game";
        else if(streamStatus_=="Sharing your game")streamStatus_="Stream stopped";
        voiceStateUpdate();
    } else if(type=="watching") {
        const auto identity=event["identity"].toString();
        if(identity.isEmpty()||!streamAccount(identity).isEmpty())voiceWatching_=identity;
        voiceStateUpdate();
    } else if(type=="stream-error") {
        const auto op=event["operation"].toString();
        streamStatus_=op=="game-audio"?"Game audio unavailable":op=="watch"?"Stream ended or unavailable":"Couldn't share this game";
    }
}
bool FluxerSession::mediaCommand(const QString& operation,const QVariantMap& args) {
    if(operation=="voice-share-stop") {
        voiceWrite({{"op","share"},{"pid",0}});voiceSharePending_=false;return true;
    }
    if(operation=="voice-watch-stop") {
        voiceWatching_.clear();voiceWrite({{"op","watch"}});voiceStateUpdate();return true;
    }
    if(voiceState_!="connected")return false;
    if(operation=="voice-share-start") {
        const auto pid=args["pid"].toLongLong();
        if(!streamAvailable()||voiceSharing_||voiceSharePending_||pid<=1)return true;
        voiceSharePending_=true;streamStatus_="Starting game stream…";
        voiceWrite({{"op","share"},{"pid",pid}});return true;
    }
    if(operation=="voice-watch") {
        const auto identity=args["identity"].toString(),socket=args["socket"].toString();
        if(socket.isEmpty()||socket.size()>512)return true;
        for(const auto& row:voiceStreams_)if(row.toMap()["identity"]==identity) {
            voiceWatching_=identity;streamStatus_.clear();
            voiceWrite({{"op","watch"},{"identity",identity},{"socket",socket}});voiceStateUpdate();break;
        }
        return true;
    }
    return false;
}
}
