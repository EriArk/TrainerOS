#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace trainer::communityIdentity {
// Ordinary, owner-authored Fluxer content. This is a discovery hint, never a
// client capability, session approval, save authority or verified-binary claim.
inline QString content(const QString& guild) {
    const QJsonObject manifest{{"kind","org.traineros.community"},{"version",1},{"guild_id",guild}};
    return "A gathering place for TrainerOS players. Welcome!\n\n```json\n"
        +QString::fromUtf8(QJsonDocument(manifest).toJson(QJsonDocument::Compact))+"\n```";
}
inline bool matches(const QJsonObject& message,const QString& guild,const QString& owner,const QString& channel) {
    if(guild.isEmpty()||owner.isEmpty()||channel.isEmpty()||message["author"].toObject()["id"]!=owner
        ||message["channel_id"]!=channel||message["type"].toInt(-1)!=0||message.contains("webhook_id"))return false;
    const auto text=message["content"].toString();
    if(text.size()>2000||!text.endsWith("\n```"))return false;
    const auto start=text.indexOf("\n```json\n");
    if(start<0)return false;
    const auto data=QJsonDocument::fromJson(text.mid(start+9,text.size()-start-13).toUtf8()).object();
    return data["kind"]=="org.traineros.community"&&data["version"].isDouble()
        &&data["version"].toDouble()==1&&data["guild_id"]==guild;
}
}
