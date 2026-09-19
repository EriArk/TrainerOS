#pragma once
#include "core/navigation/AdventureExitController.h"

namespace trainer {
// A snapshot from the exclusive overlay input provider, not the game's muted
// virtual pad. allReleased includes every button, both sticks and triggers.
struct ExitInputSnapshot {
    bool connected = false;
    bool allReleased = false;
    bool confirm = false;
    bool back = false;
};

// Presentation only: owns neither game processes nor the platform input lease.
class AdventureExitPresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(bool confirming READ confirming NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool captureFailed READ captureFailed NOTIFY changed)
    Q_PROPERTY(bool slowClose READ slowClose NOTIFY changed)
    Q_PROPERTY(bool autosave READ autosave NOTIFY changed)
    Q_PROPERTY(QString frameKey READ frameKey NOTIFY changed)
public:
    explicit AdventureExitPresentation(AdventureExitController&, QObject* parent = nullptr);
    bool visible() const;
    bool confirming() const;
    bool ready() const { return ready_; }
    bool captureFailed() const { return !exit_.captureError().isEmpty(); }
    bool slowClose() const { return slowClose_; }
    bool autosave() const { return exit_.verifiedAutosave(); }
    QString frameKey() const { return QString::number(exit_.attempt()); }
    // A provider must tag asynchronous snapshots with the current generation.
    // Focus/lease loss and each phase change invalidate previous snapshots.
    quint64 inputGeneration() const { return generation_; }
    void setInputIsolated(bool);
    Q_INVOKABLE void setWindowFocused(bool);
    void updateInput(quint64 generation, const ExitInputSnapshot&);
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void cancel();
signals:
    void changed();
private:
    void resetInput();
    AdventureExitController& exit_;
    AdventureExitController::Phase phase_;
    bool isolated_ = false, focused_ = false, ready_ = false;
    bool previousConfirm_ = false, previousBack_ = false, slowClose_ = false;
    quint64 generation_ = 0;
    QTimer closeTimer_;
};
}
