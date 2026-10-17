#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace trainer::lobbyChatIdentity {
inline QJsonObject decode(const QString& text) {
    if(!text.startsWith("traineros:lobby-chat:v1:")||text.size()>1024)return {};
    const auto data=QJsonDocument::fromJson(text.mid(QStringLiteral("traineros:lobby-chat:v1:").size()).toUtf8()).object();
    if(data["v"]!=1||QUuid(data["lobby"].toString()).isNull()||data["channel"].toString().isEmpty()||data["creator"].toString().isEmpty())return {};
    return data;
}
inline QString content(const QString& lobby,const QString& channel,const QString& creator) {
    return "traineros:lobby-chat:v1:"+QString::fromUtf8(QJsonDocument(QJsonObject{{"v",1},{"lobby",lobby},{"channel",channel},{"creator",creator}}).toJson(QJsonDocument::Compact));
}
inline QJsonObject identity(const QJsonObject& message,const QString& channel) {
    const auto data=decode(message["content"].toString());
    return message["pinned"].toBool()&&message["channel_id"]==channel&&data["channel"]==channel&&message["author"].toObject()["id"]==data["creator"] ? data : QJsonObject{};
}
}
