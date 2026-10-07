#pragma once
#include "core/input/Action.h"
#include "GameParty.h"
#include "integrations/adventure/retroarch/RetroArchNetplay.h"
#include "integrations/adventure/retroarch/RetroArchNetplayClient.h"
#include "integrations/adventure/standalone/PpssppNetplay.h"
#include "integrations/adventure/standalone/DolphinNetplay.h"
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
    Q_PROPERTY(QVariantMap party READ party NOTIFY changed)
    Q_PROPERTY(bool incoming READ incoming NOTIFY changed)
    Q_PROPERTY(QString invitation READ invitation NOTIFY changed)
    Q_PROPERTY(QStringList onlineGames READ onlineGames NOTIFY availabilityChanged)
public:
    RuntimeMultiplayer(LibraryRepository&, RetroArchAdapter&, StandaloneAdapter&, StandaloneAdapter&, SocialController&,
                       ProcessService&, AdventureLaunchController&, AdventureExitPresentation&);
    void refresh(const RetroArchInstallation&,QString trainer,bool allowed);
    QVariantMap party() const {return party_.state().toVariantMap();}
    Q_INVOKABLE void leaveParty();
    QString menuLabel() const;
    bool canInvite() const;
    QStringList onlineGames() const { return onlineGames_; }
    bool incoming() const;
    QString invitation() const;
    Q_INVOKABLE void answer(bool accept);
    bool dispatch(Action);
    bool action(const QString&, bool fromSocial=false);
signals:
    void changed();
    void availabilityChanged();
    void notice(QString text);
private:
    void startParty(bool host,const QJsonObject& endpoint);
    void update();
    void show(QString panel);
    void prepareHost();
    void resolveRelay();
    void resolvePspRelay();
    bool psp() const { return descriptor_["id"].toString().startsWith("runtime.ppsspp."); }
    bool dolphin() const { return descriptor_["id"].toString().startsWith("runtime.dolphin."); }
    void frame(const QJsonObject&);
    void send(QJsonObject);
    void launch();
    void fail(QString);
    void pollRelay();
    void output(const QByteArray&);
    LibraryRepository& library_;RetroArchAdapter& adapter_;SocialController& social_;
    StandaloneAdapter& ppsspp_;StandaloneAdapter& dolphin_;
    dolphin::NetplayRequest dolphinRequest_;
    ProcessService& process_;AdventureLaunchController& lifecycle_;AdventureExitPresentation& overlay_;
    LocalLinkPeer nearby_{this,47855,47856,"trainerosRuntime",true};
    GameParty party_{this};
    bool partySession_=false,companyOverride_=false;
    QFutureWatcher<QMap<QString,QJsonObject>> scan_{this};
    QNetworkAccessManager network_{this};QTimer timer_{this};
    QString trainer_,identity_,game_,status_,transportPeer_,lastRequest_;
    QJsonObject descriptor_;
    QMap<QString,QJsonObject> profiles_,games_;
    QStringList onlineGames_;
    retroarch::NetplayRequest request_;
    retroarch::NetplayClient client_{this};
    RetroArchInstallation installation_;
    bool allowed_=false,online_=false,host_=false,active_=false,restarting_=false,launchPending_=false,query_=false,relaySent_=false;
    qint64 deadline_=0;quint64 scanRevision_=0;
    QByteArray output_;
    bool socialSurface_=false;
    QString socialPanel_;
};
}
