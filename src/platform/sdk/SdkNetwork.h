#pragma once
#include "features/social/LobbyRuntime.h"
#include <QProcess>
#include <QJsonArray>
#include <memory>

namespace trainer {
// Optional native activity adapter to the existing Lobby and secure record helper.
// The application owns its rules; this class owns admission and channel lifetime.
class SdkNetwork final : public LobbyRuntimeService {
    Q_OBJECT
public:
    static constexpr int MaxPlayers=16;
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
    struct Link {
        QProcess process;
        QString peer,connection,identity,channel;
        QByteArray buffer;
        QList<QJsonObject> pending;
        bool connected=false;
        quint64 sent=0,acked=0,received=0,consumed=0;
    };
    QMap<QString,std::shared_ptr<Link>> links_;
    QTimer deadline_;
    QString app_,operation_,ticket_,self_;
    QJsonObject prepared_,binding_;
    Completion completion_;
    bool host_=false,announced_=false,stopping_=false;
    bool allConnected() const;
    void addLink(const QString&);
    QJsonObject channelState() const;
    bool matches() const;
    void reconcile();
    void completeLaunch();
    void signal(QString,QString,QString,QJsonObject);
    void startHelper(Link&,const QJsonObject&);
    bool write(Link&,const QJsonObject&);
    void read(const QString&);
    void end(QString);
    void stop();
};
}
