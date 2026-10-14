#pragma once
#include "LobbyCoordinator.h"
#include "LobbyInput.h"
#include <QPointer>
#include <functional>

namespace trainer {
// Trusted, local runtime services own content checks, saves, process admission,
// transport preparation and safe cancellation. Peer metadata never supplies one.
class LobbyRuntimeService : public QObject {
public:
    using Completion=std::function<void(bool,QString)>;
    using QObject::QObject;
    virtual void prepare(QString operation,QJsonObject context,Completion done)=0;
    virtual void launch(QString operation,QString ticket,QJsonObject context,Completion done)=0;
    virtual void cancel(QString operation)=0;
    virtual bool supportsLive(const QJsonObject&) const {return false;}
    virtual bool admitLive(QString,QString,QString,int) {return false;}
    virtual void releaseLive(QString,QString) {}
};
class LobbyRuntime final : public QObject {
    Q_OBJECT
public:
    explicit LobbyRuntime(LobbyCoordinator&,QObject* parent=nullptr);
    ~LobbyRuntime() override {cancel();}
    void configure(QString self,bool online);
    void registerService(QString mode,LobbyRuntimeService* service);
    bool available() const;
    bool busy() const {return busy_;}
    QString status() const {return status_;}
    bool prepare();
    bool finished(QString ticket);
    bool liveSupported() const;
    bool liveAvailable() const;
    bool setLiveJoining(bool enabled);
    bool joinLive();
    LobbyInput* input() {return &input_;}
signals:
    void changed();
private:
    QJsonObject context() const;
    void reconcile();
    void cancel();
    void launch(QString ticket,QJsonObject party);
    LobbyCoordinator& lobby_;
    LobbyInput input_;
    QMap<QString,QPointer<LobbyRuntimeService>> services_;
    QPointer<LobbyRuntimeService> activeService_;
    QString self_,operation_,ticket_,status_;
    QJsonObject preparedContext_;
    QTimer deadline_{this};
    bool online_=false,busy_=false,prepared_=false,playing_=false,live_=false;
};
}
