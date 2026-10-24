#pragma once
#include <QObject>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QTimer>
#include <functional>

namespace trainer::retroarch {
// Scores travel through the existing authenticated lobby runtime channel.
// Peers supply scores only, never arbitrary probe addresses or relay endpoints.
class NetplayRelaySelector final : public QObject {
    Q_OBJECT
public:
    explicit NetplayRelaySelector(QObject* parent=nullptr);
    ~NetplayRelaySelector() override;
    void start(QString self,QString owner,QStringList players,QString preference);
    void stop();
    void receive(const QString& peer,const QJsonObject& scores);
    static QString choose(const QStringList& players,const QMap<QString,QJsonObject>& scores,const QString& preference);
signals:
    void report(QString peer,QJsonObject scores);
    void selected(QString region,QString endpoint);
    void failed(QString message);
private:
    friend class NetplayRelayTests;
    void probe(QString region,quint64 generation);
    void result(QString region,QString endpoint,int score,quint64 generation);
    void decide();
    QNetworkAccessManager network_{this};
    QTimer deadline_{this},repeat_{this};
    QObject* probes_=nullptr;
    QString self_,owner_,preference_;
    QStringList players_,pending_;
    QJsonObject local_;
    QMap<QString,QJsonObject> reports_;
    QMap<QString,QString> endpoints_;
    QMap<QString,int> attempts_;
    quint64 generation_=0;
    bool complete_=false;
    // The Libretro tunnel directory serves HTTP; port 443 is not available.
    QString directory_="http://lobby.libretro.com/tunnel?name=";
};
}
