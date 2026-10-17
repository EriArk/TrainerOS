#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUuid>
#include <QRegularExpression>

namespace trainer::lobbyDirectory {
// Public provider identities. Credentials are held only by the server service.
inline const QString guild="1559529715193098242";
inline const QString bot="1559529748697190400";
inline const QString invite="kOYY3hzY";
inline const QStringList channels{"1559529986233212933","1559529993002819588","1559530000271540224","1559530002591002624","1559530004931416064","1560946247710285824","1560946247076945921"};
inline const QStringList topics{"choosing","racing","fighting","adventure","other","party-games","competitive"};
// Read the old choosing channel for compatibility; it is no longer a category.
inline QStringList publicationTopics() {auto result=topics;result.removeAll("choosing");result.removeAll("other");result.append("other");return result;}
inline QString topicName(const QString& topic) {
    const QStringList names{"Choosing together","Racing","Fighting","Adventure","Other games","Party Games","Competitive"};
    const auto index=topics.indexOf(topic);return index<0?QString():names[index];
}
inline QString prefix() {return "TrainerOS lobby directory\n```json\n";}
inline QString encode(const QJsonObject& object) {
    const auto value=prefix()+QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact))+"\n```";
    return value.size()<=2000?value:QString();
}
inline QJsonObject decode(const QString& text) {
    if(text.size()>2000||!text.startsWith(prefix())||!text.endsWith("\n```"))return {};
    const auto doc=QJsonDocument::fromJson(text.mid(prefix().size(),text.size()-prefix().size()-4).toUtf8());
    const auto value=doc.object();return value["v"].isDouble()&&value["v"].toInt()==1?value:QJsonObject{};
}
inline QJsonObject listing(const QJsonObject& message,qint64 now) {
    if(!message["webhook_id"].toString().isEmpty()||message["author"].toObject()["id"]!=bot)return {};
    auto value=decode(message["content"].toString());
    const auto id=value["lobby"].toString(),owner=value["owner"].toString();
    if(QUuid(id).isNull()||QUuid(id).toString(QUuid::WithoutBraces)!=id||owner.isEmpty()||owner.size()>20)return {};
    for(const auto c:owner)if(c<'0'||c>'9')return {};
    if(value["expires"].toInteger()<=now||value["expires"].toInteger()>now+300||value["revision"].toInteger()<1)return {};
    if(value["access"]!="public"&&value["access"]!="approval")return {};
    const auto status=value["status"].toString();
    if(!QStringList{"choosing","gathering","playing"}.contains(status))return {};
    const int seats=value["seats"].toInt(-1),available=value["available"].toInt(-1);
    if(seats<0||seats>16||available<0||available>seats)return {};
    if(status=="choosing"&&(seats||available))return {};
    const int streams=value["streams"].toInt(0);if(streams<0||streams>32)return {};
    QJsonObject result{{"lobby",id},{"owner",owner},{"name",value["name"].toString().left(100)},
        {"revision",value["revision"]},{"listedAt",value["listedAt"]},{"expires",value["expires"]},{"access",value["access"]},
        {"status",status},{"seats",seats},{"available",available},{"streams",streams}};
    const auto starts=value["startsAt"].toInteger();const auto members=value["members"].toInt(seats-available);
    if(starts<0||starts>4102444800LL||members<0||members>32||value["title"].toString().size()>80)return {};
    result["title"]=value["title"].toString();result["startsAt"]=starts;result["members"]=members;
    const auto avatar=value["avatar"].toString();
    if(QRegularExpression("^(?:a_)?[0-9a-f]{32}$").match(avatar).hasMatch())result["avatar"]=avatar;
    if(status!="choosing") {
        const auto game=value["game"].toObject();QJsonObject clean;
        for(const auto& key:QStringList{"id","title","platform","mode"}) {
            const auto field=game[key].toString();if(field.isEmpty()||field.size()>160)return {};
            clean[key]=field;
        }
        if(!seats)return {};
        result["game"]=clean;
    }
    return result;
}
}
