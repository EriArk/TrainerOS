#pragma once
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>
namespace trainer {
class ExperienceArtProvider {
public:
    virtual ~ExperienceArtProvider()=default;
    virtual QVariantMap image(const QString& entity,const QString& profile) const=0;
};
// Common communication transport consumes opaque adapter activities. It neither
// interprets save formats nor implements a game's trade/battle protocol.
class NativeActivityProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QJsonArray onlineCapabilities() const=0;
    virtual bool beginOnline(QString self,QString peer,QString name,QString activity,bool initiator)=0;
    virtual void receiveOnline(QJsonObject)=0;
    virtual void showOnline()=0;
    virtual void endOnline()=0;
    virtual bool visibleNearby() const=0;
    virtual void setVisibleNearby(bool)=0;
signals:
    void changed();
    void onlineSend(QJsonObject frame);
    void onlineClosed();
};
}
