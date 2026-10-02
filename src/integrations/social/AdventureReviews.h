#pragma once
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>

namespace trainer::reviews {
inline bool validIdentity(const QString& id) {
    static const QRegularExpression hash("^[a-f0-9]{64}$");return hash.match(id).hasMatch();
}
inline QString encode(const QString& identity,const QString& text,bool spoiler,const QString& policy) {
    if(!validIdentity(identity)||text.trimmed().isEmpty()||text.size()>800)return {};
    return "TrainerOS review v1\n"+QString::fromUtf8(QJsonDocument(QJsonObject{
        {"adventure",identity},{"text",text.trimmed()},{"spoiler",spoiler},{"completion",policy}}).toJson(QJsonDocument::Compact));
}
inline QJsonObject decode(const QJsonObject& message,const QString& identity) {
    const auto content=message["content"].toString();
    const QString prefix="TrainerOS review v1\n";
    if(!content.startsWith(prefix)||content.size()>6000||message["webhook_id"].isString()
        ||message["author"].toObject()["bot"].toBool())return {};
    auto data=QJsonDocument::fromJson(content.mid(prefix.size()).toUtf8()).object();
    if(!validIdentity(identity)||data["adventure"]!=identity||!data["spoiler"].isBool()
        ||!data["text"].isString()||data["text"].toString().trimmed().isEmpty()||data["text"].toString().size()>800)return {};
    const auto author=message["author"].toObject();
    if(author["id"].toString().isEmpty()||message["id"].toString().isEmpty())return {};
    data["id"]=message["id"];data["author"]=author["id"];
    data["name"]=author["global_name"].toString().isEmpty()?author["username"]:author["global_name"];
    data["date"]=message["edited_timestamp"].isString()?message["edited_timestamp"]:message["timestamp"];
    return data;
}
}
