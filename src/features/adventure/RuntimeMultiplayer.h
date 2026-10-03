#pragma once
#include "core/input/Action.h"
#include "integrations/adventure/retroarch/RetroArchNetplay.h"
#include "integrations/adventure/retroarch/RetroArchNetplayClient.h"
#include "integrations/social/OnlineLink.h"
#include "platform/network/LocalLinkPeer.h"
#include <QFutureWatcher>
#include <QNetworkAccessManager>

namespace trainer {
class SocialController;
class AdventureLaunchController;
class AdventureExitPresentation;
// One runtime session, composed with existing consent, process and Home services.
class RuntimeMultiplayer final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool incoming READ incoming NOTIFY changed)
    Q_PROPERTY(QString invitation READ invitation NOTIFY changed)
public:
    RuntimeMultiplayer(LibraryRepository&, RetroArchAdapter&, SocialController&,
                       ProcessService&, AdventureLaunchController&, AdventureExitPresentation&);
    void refresh(const RetroArchInstallation&,QString trainer,bool allowed);
    bool canInvite() const;
    bool incoming() const;
    QString invitation() const;
    Q_INVOKABLE void answer(bool accept);
    bool dispatch(Action);
    bool action(const QString&);
signals:
    void changed();
    void notice(QString text);
private:
    void update();
    void show(QString panel);
    void begin(bool host,bool online);
    void frame(const QJsonObject&);
    void send(QJsonObject);
    void launch();
    void fail(QString);
    void pollRelay();
    void output(const QByteArray&);
    LibraryRepository& library_;RetroArchAdapter& adapter_;SocialController& social_;
    ProcessService& process_;AdventureLaunchController& lifecycle_;AdventureExitPresentation& overlay_;
    LocalLinkPeer nearby_{this,47855,47856,"trainerosRuntime"};
    OnlineLink consent_{this};
    QFutureWatcher<QPair<QString,QJsonObject>> scan_{this};
    QNetworkAccessManager network_{this};QTimer timer_{this};
    QString trainer_,identity_,game_,peerId_,peerName_,status_,selection_,onlinePerson_;
    QJsonObject descriptor_;
    retroarch::NetplayRequest request_;
    retroarch::NetplayClient client_{this};
    RetroArchInstallation installation_;
    bool allowed_=false,online_=false,host_=false,active_=false,restarting_=false,launchPending_=false,query_=false,invited_=false,relaySent_=false;
    qint64 deadline_=0;quint64 scanRevision_=0;
    QByteArray output_;
};
}
