#pragma once
#include "features/social/LobbyRuntime.h"
#include <QProcess>
#include <QJsonArray>

namespace trainer {
// Optional native activity adapter to the existing Lobby and secure record helper.
// The application owns its rules; this class owns admission and channel lifetime.
class SdkNetwork final : public LobbyRuntimeService {
    Q_OBJECT
public:
    SdkNetwork(LobbyCoordinator&, LobbyRuntime&, QString python, QString helper,
               QString nearbyAddress={}, QObject* parent=nullptr);
    ~SdkNetwork() override;
    bool available() const;
    QString request(const QString& app,const QString& method,const QJsonObject&,QJsonObject& result);
    void disconnected(const QString& app);
    QVariantList activities() const;
    void setInstalledActivities(const QVariantList&);
    std::function<bool(QString)> launchInstalled;
    void prepare(QString,QJsonObject,Completion) override;
    void launch(QString,QString,QJsonObject,Completion) override;
    void cancel(QString) override;
signals:
    void activitiesChanged();
    void event(QString app,QJsonObject packet);
    void openLobbyRequested();
private:
    LobbyCoordinator& lobby_;
    LobbyRuntime& runtime_;
    QString python_,helper_,nearbyAddress_;
    QMap<QString,QJsonObject> apps_;
    QMap<QString,QJsonObject> installed_;
    QMap<QString,QString> launchIds_;
    QProcess process_;
    QTimer deadline_;
    QByteArray buffer_;
    QString app_,operation_,ticket_,peer_,self_,connection_,identity_,channel_;
    QJsonObject prepared_,binding_,endpoint_;
    QList<QJsonObject> pendingData_;
    Completion completion_;
    bool host_=false,connected_=false,announced_=false,stopping_=false;
    quint64 sent_=0,acked_=0,received_=0,consumed_=0;
    QJsonObject channelState() const;
    bool matches() const;
    void reconcile();
    void completeLaunch();
    void signal(QString,QString,QString,QJsonObject);
    void startHelper(const QJsonObject&);
    bool write(const QJsonObject&);
    void read();
    void end(QString);
    void stop();
};
}
