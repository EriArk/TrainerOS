#pragma once
#include <QObject>
#include <QJsonObject>
#include <QTimer>
#include <functional>

namespace trainer {
// Ordered two-person handoff. The accepted GameParty supplies authenticated
// sender slots; the reviewed native observer supplies ownership acknowledgements.
// This coordinates existing emulator roles, never emulates game input or saves.
class HotseatHandoff final : public QObject {
    Q_OBJECT
public:
    explicit HotseatHandoff(QObject* parent=nullptr);
    void begin(QString session,int slot);
    void stop();
    void pass();
    void receive(int sender,const QJsonObject& control);
    void observed(quint64 sequence,int nativeOwner,bool initial);
    bool active() const { return !session_.isEmpty(); }
    bool ready() const { return active() && phase_==Idle; }
    bool canPass() const { return ready() && owner_==slot_; }
    int owner() const { return owner_; }
    quint64 epoch() const { return epoch_; }
    // true requests playing, false requests spectating. Acceptance of this
    // local command is not native acknowledgement. Never retry an uncertain toggle.
    std::function<bool(bool)> requestNativeRole;
signals:
    void outgoing(int targetSlot,QJsonObject control);
    void changed();
    void failed(QString reason);
private:
    friend class HotseatHandoffTests;
    enum Phase { Waiting=0, Releasing=1, Acquiring=2, Idle=3 };
    void send(QString action,QJsonObject extra={});
    void publish();
    void startPass();
    void command(bool playing);
    void fail();
    void acknowledge(quint64 sequence,int owner,bool initial);
    void applyPhase(const QJsonObject&);
    QString session_;
    int slot_=0,owner_=0,next_=0,nativeOwner_=0;
    quint64 epoch_=0,observedSequence_=0,commandEpoch_=0;
    Phase phase_=Waiting;
    bool commanded_=false;
    QTimer timer_{this};
};
}
