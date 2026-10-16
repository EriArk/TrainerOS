#pragma once
#include "features/social/SocialController.h"

namespace trainer {
class HomeSocialFixture {
public:
    static QVariantMap chat(QString channel,QString peer,QString name,qint64 when=1000) {
        return {{"id",channel},{"name",name},{"kind","chats"},{"unread",1},{"lastInteraction",when},
            {"members",QVariantList{QVariantMap{{"id",peer}}}}};
    }
    static void encounter(SocialController& social,QString self,QString peer,qint64 when) {
        social.recentTogether_.observe({{"id","fixture-room"},{"members",QJsonArray{
            QJsonObject{{"id",self},{"connected",true}},QJsonObject{{"id",peer},{"name","Played friend"},{"connected",true}}}}},when);
    }
    static void publish(SocialController& social, QVariantMap snapshot) {
        social.receive(0, std::move(snapshot));
    }
};
}
