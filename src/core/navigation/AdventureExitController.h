#pragma once
#include <QObject>
#include <QImage>
#include <QTimer>

namespace trainer {
// Resolved by the exact title/integration, never inferred from its platform.
enum class AdventureSavePolicy { Unknown, ManualConfirm, VerifiedAutosave };

// Protocol for a still-running game. The platform must opt in only after it
// can capture gameplay, isolate overlay input and close that owned game cleanly.
// It must not implement gracefulExitRequested with ProcessService::stop().
class AdventureExitController final : public QObject {
    Q_OBJECT
public:
    enum class Phase { Idle, Capturing, Confirming, Closing };
    Q_ENUM(Phase)
    static constexpr int CaptureTimeoutMs = 2500;
    explicit AdventureExitController(QObject* parent = nullptr);
    Phase phase() const { return phase_; }
    bool verifiedAutosave() const { return policy_ == AdventureSavePolicy::VerifiedAutosave; }
    QString captureError() const { return captureError_; }
    bool available() const { return available_; }
    QImage capturedFrame() const { return pendingFrame_; }
    quint64 attempt() const { return attempt_; }
    void setAvailable(bool available);
    void beginSession(AdventureSavePolicy policy);
    void endSession(bool cleanExit);
    bool requestExit();
    void captureCompleted(quint64 attempt, const QImage& frame, const QString& error = {});
    void gracefulExitFailed(quint64 attempt, const QString& error);
    bool confirm();
    bool cancel();
signals:
    void changed();
    void captureRequested(quint64 attempt);
    void confirmationRequested();
    void returnToGameRequested();
    void gracefulExitRequested(quint64 attempt);
    void failed(const QString& error);
    // Emitted only after the owned process actually exits cleanly. This is not
    // proof of an in-game save or a durable media write. Null frame means no new
    // image: consumers must retain previous valid media. userConfirmed records
    // permission to exit, not proof of a completed manual save or autosave.
    void completed(quint64 attempt, const QImage& frame, bool userConfirmed);
private:
    void close();
    void clearAttempt();
    Phase phase_ = Phase::Idle;
    AdventureSavePolicy policy_ = AdventureSavePolicy::Unknown;
    bool available_ = false, sessionActive_ = false, userConfirmed_ = false, closeRequested_ = false;
    quint64 attempt_ = 0;
    QImage pendingFrame_;
    QString captureError_;
    QTimer captureTimer_;
};
}
