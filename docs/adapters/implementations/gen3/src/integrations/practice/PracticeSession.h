#pragma once
#include "core/model/GameProgress.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QJsonObject>

namespace trainer {
struct PracticeSource {
    QString trainerId, adventureId, contextRevision, contentRevision, saveRevision;
    bool operator==(const PracticeSource&) const = default;
};
// Host-owned lifecycle. The caller supplies a trusted pinned runtime/worker and
// forwards current source changes (including running/unreadable/missing states).
// This service has no save writer and cannot commit battle results.
class PracticeSession final : public QObject {
    Q_OBJECT
public:
    explicit PracticeSession(QObject* parent=nullptr);
    ~PracticeSession() override;
    bool begin(const QString& node,const QString& worker,const QString& engineRoot,
        const PracticeSource&,const GameProgress&,int first,int second,
        const std::array<int,4>& seed);
    void updateSource(const PracticeSource&,bool available);
    bool choose(int firstSlot,int secondSlot);
    void cancel();
    bool active() const {return process_!=nullptr;}
    QJsonObject state() const {return state_;}
signals:
    void changed();
    void stopped(const QString& reason);
private:
    void receive();
    void stop(const QString& reason);
    void send(const QJsonObject&);
    QProcess* process_=nullptr;
    QTimer deadline_;
    PracticeSource source_;
    QJsonObject start_,state_;
    QByteArray buffer_;
    bool ready_=false,pending_=false,stopping_=false;
    QString reason_;
};
}
