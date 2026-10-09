#pragma once
#include "core/navigation/AdventureLaunchController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QProcess>
#include <QTemporaryDir>
#include <memory>
#include "core/input/Action.h"
#include <QElapsedTimer>

namespace trainer {
// Opt-in transport to the validated platform helper. No emulator command or
// raw device path reaches QML. A separate helper watchdog owns lease recovery.
class AdventureOverlayService final : public QObject {
    Q_OBJECT
public:
    AdventureOverlayService(ProcessService&, AdventureLaunchController&, AdventureExitPresentation&,
                            const QString& helper, QObject* parent = nullptr);
    ~AdventureOverlayService() override;
    bool canMinimize() const { return active_ && protocol_>=3 && exit_.available(); }
    void minimize();
    void returnToGame(bool options = false);
signals:
    void shellRequested();
    void gameRequested();
    void shellAction(trainer::Action action);
    void handoffFailed(QString message);
private:
    void start();
    void stop();
    void send(const QJsonObject&);
    void receive();
    void lost();
    void shellInput(const QJsonObject&);
    ProcessService& game_;
    AdventureExitController& exit_;
    AdventureExitPresentation& view_;
    AdventureLaunchController& launch_;
    QString helper_;
    std::unique_ptr<QTemporaryDir> temporary_;
    QProcess helperProcess_;
    QTimer heartbeat_, retry_, stopDeadline_, handoffDeadline_;
    QByteArray buffer_;
    bool active_ = false;
    quint64 sentEpoch_ = 0;
    quint64 attempt_ = 0;
    int protocol_ = 0;
    bool shellReady_ = false, returning_ = false, minimizing_ = false;
    QString heldAction_;
    QElapsedTimer repeat_;
};
}
