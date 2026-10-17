#pragma once
#include "features/social/LobbyConnection.h"
#include "LobbyGamePreparation.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"

namespace trainer {
class SocialController;
class RemotePlayRuntime final:public LobbyRuntimeService {
    Q_OBJECT
public:
    QString preparationProblem() const override {return problem_;}
    RemotePlayRuntime(LibraryRepository&,RetroArchAdapter&,SocialController&,ProcessService&,
                      AdventureLaunchController&,LobbyGamePreparation&,QObject* parent=nullptr);
    ~RemotePlayRuntime() override;
    // Trusted installation; credentials/endpoints never come from a game row.
    bool configure(const QString& directory);
    QVariantList choices(const QMap<QString,QJsonObject>& profiles);
    std::function<bool(const QJsonObject&)> hostSupported;
    std::function<bool()> allowed;
    std::function<QString()> ownerDevice;
    std::function<AdventureResult(const ProcessCommand&,const QString&)> requestGuestLaunch;
    void prepare(QString,QJsonObject,Completion) override;
    void launch(QString,QString,QJsonObject,Completion) override;
    void cancel(QString) override;
    bool supportsLive(const QJsonObject&) const override;
    bool admitLive(QString,QString,QString,int) override;
    void releaseLive(QString,QString) override;
signals:
    void notice(QString);
private:
    QString problem_;
    friend class RuntimeMultiplayerTests;
    bool validPlayers(const QJsonObject&) const;
    bool hostUnchanged() const;
    void connected(QString);
    void reconcile();
    void capture();
    void startGuestMedia();
    void mediaOutput();
    void disconnectMedia();
    void failed(QString);
    void complete(bool,QString={});
    LibraryRepository& library_;RetroArchAdapter& adapter_;SocialController& social_;
    ProcessService& process_;AdventureLaunchController& lifecycle_;LobbyGamePreparation& preparation_;
    LobbyConnection connection_;QProcess media_{this};QTimer deadline_{this};
    QString root_,operation_,ticket_,game_,device_,config_,connectionId_;QJsonArray ice_;
    QString remotePeer_,admission_;
    QJsonObject context_,profile_;QMap<QString,QJsonObject> games_;
    int revision_=0;QString contentPath_;quint64 account_=0;
    qint64 ownedPid_=0;Completion completion_;QByteArray output_;
    bool host_=false,opening_=false,started_=false,stopping_=false,announced_=false;
    bool mediaReady_=false,failing_=false;
};
}
