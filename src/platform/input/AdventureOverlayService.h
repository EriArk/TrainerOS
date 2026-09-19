#pragma once
#include "core/navigation/AdventureLaunchController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QProcess>
#include <QTemporaryDir>
#include <memory>

namespace trainer {
// Opt-in transport to the validated platform helper. No emulator command or
// raw device path reaches QML. A separate helper watchdog owns lease recovery.
class AdventureOverlayService final : public QObject {
    Q_OBJECT
public:
    AdventureOverlayService(ProcessService&, AdventureLaunchController&, AdventureExitPresentation&,
                            const QString& helper, QObject* parent = nullptr);
    ~AdventureOverlayService() override;
private:
    void start();
    void stop();
    void send(const QJsonObject&);
    void receive();
    void lost();
    ProcessService& game_;
    AdventureExitController& exit_;
    AdventureExitPresentation& view_;
    QString helper_;
    std::unique_ptr<QTemporaryDir> temporary_;
    QProcess helperProcess_;
    QTimer heartbeat_, retry_, stopDeadline_;
    QByteArray buffer_;
    bool active_ = false;
    quint64 sentEpoch_ = 0;
    quint64 attempt_ = 0;
};
}
