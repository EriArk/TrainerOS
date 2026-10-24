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
    virtual QString preparationProblem() const {return {};}
    virtual bool supportsLive(const QJsonObject&) const {return false;}
    virtual bool supportsRejoin(const QJsonObject&) const {return false;}
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
    bool opening() const {return !ticket_.isEmpty()&&!playing_;}
    void cancelPending();
    QString status() const {return status_;}
    QString problem() const {return problem_;}
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
    QString problem_;
    QJsonObject preparedContext_;
    QJsonObject problemContext_;
    QTimer deadline_{this};
    bool online_=false,busy_=false,prepared_=false,playing_=false,live_=false;
    bool started_=false;
};
}
