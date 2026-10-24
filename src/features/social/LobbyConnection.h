#pragma once
#include "LobbyCoordinator.h"
#include <QProcess>
#include <QTimer>
#include <QJsonArray>
#include <QTemporaryDir>
#include <memory>

namespace trainer {
// One accepted owner/guest connection. Owns signaling and a private transport child,
// never the game process, save files or permission to inject a controller.
class LobbyConnection final:public QObject {
    Q_OBJECT
public:
    explicit LobbyConnection(LobbyCoordinator&,QObject* parent=nullptr);
    ~LobbyConnection() override;
    bool start(QString self,QString ticket,QString helper,QJsonArray ice={},QString mode="game",QString guest={});
    void stop();
    QString id() const {return connection_;}
    QString peer() const {return peer_;}
    bool active() const {return !ticket_.isEmpty();}
    bool announceMedia();
signals:
    void connected(QString config);
    void ended(QString reason);
    void mediaReady();
private:
    void reconcile();
    void receive(QString peer,QString ticket,QString connection,const QJsonObject&);
    void launch();
    void read();
    void write(QJsonObject);
    bool matches() const;
    void fail(QString reason);
    LobbyCoordinator& lobby_;QProcess process_{this};QTimer deadline_{this};
    QString self_,peer_,ticket_,connection_,identity_,helper_,config_,admission_,guest_;
    QString mode_="game";
    QJsonObject context_,pendingDescription_;QJsonArray ice_,pendingCandidates_;QByteArray buffer_;
    int candidatesSent_=0,candidatesReceived_=0;
    std::unique_ptr<QTemporaryDir> directory_;
    bool initialized_=false;
    bool host_=false,ready_=false,descriptionSent_=false,descriptionReceived_=false,stopping_=false;
};
}
