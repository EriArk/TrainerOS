#pragma once
#include "features/social/SocialController.h"
#include "integrations/social/FluxerSession.h"

namespace trainer {
class HomeSocialFixture {
public:
    static void isolateProvider(SocialController& social) {
        QObject::disconnect(&social,nullptr,social.session_,nullptr);
        QObject::disconnect(social.session_,nullptr,&social,nullptr);
    }
    static QVariantMap chat(QString channel,QString peer,QString name,qint64 when=1000) {
        return {{"id",channel},{"name",name},{"kind","chats"},{"unread",1},{"lastInteraction",when},
            {"members",QVariantList{QVariantMap{{"id",peer}}}}};
    }
    static void encounter(SocialController& social,QString self,QString peer,qint64 when) {
        social.recentTogether_.observe({{"id","fixture-room"},{"phase","playing"},{"ticket","session-"+QString::number(when)},
            {"game",QJsonObject{{"id","fixture-game"},{"title","Played game"}}},{"members",QJsonArray{
            QJsonObject{{"id",self},{"connected",true},{"seat",1}},QJsonObject{{"id",peer},{"name","Played friend"},{"connected",true},{"seat",2}}}}},when);
    }
    static void publish(SocialController& social, QVariantMap snapshot) {
        social.receive(0, std::move(snapshot));
    }
};
}
