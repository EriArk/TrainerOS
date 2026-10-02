#pragma once
#include "core/navigation/AdventureExitController.h"
#include <QVariantList>

namespace trainer {
// A snapshot from the exclusive overlay input provider, not the game's muted
// virtual pad. allReleased includes every button, both sticks and triggers.
struct ExitInputSnapshot {
    bool connected = false;
    bool allReleased = false;
    bool confirm = false;
    bool back = false;
    bool home = false;
    bool up = false;
    bool down = false;
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
    Q_PROPERTY(bool menuOpen READ menuOpen NOTIFY changed)
    Q_PROPERTY(int menuFocus READ menuFocus NOTIFY changed)
    Q_PROPERTY(QString gameTitle READ gameTitle NOTIFY changed)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY changed)
    Q_PROPERTY(QVariantList menuActions READ menuActions NOTIFY changed)
    Q_PROPERTY(QString panel READ panel NOTIFY changed)
    Q_PROPERTY(bool menuCanSelect READ menuCanSelect NOTIFY changed)
    Q_PROPERTY(QString menuCaption READ menuCaption NOTIFY changed)
public:
    explicit AdventureExitPresentation(AdventureExitController&, QObject* parent = nullptr);
    bool visible() const;
    bool confirming() const;
    bool ready() const { return ready_; }
    bool captureFailed() const { return !exit_.captureError().isEmpty(); }
    bool slowClose() const { return slowClose_; }
    bool autosave() const { return exit_.verifiedAutosave(); }
    QString frameKey() const { return menuOpen_ ? "menu-" + QString::number(menuAttempt_) : QString::number(exit_.attempt()); }
    QImage frame() const { return menuOpen_ ? menuFrame_ : exit_.capturedFrame(); }
    bool hasFrame() const { return !frame().isNull(); }
    bool menuOpen() const { return menuOpen_; }
    int menuFocus() const { return menuFocus_; }
    QString gameTitle() const { return gameTitle_; }
    QVariantList menuActions() const;
    QString menuCaption() const{return panel_.isEmpty()?QString("In game"):caption_;}
    QString panel() const{return panel_;}
    bool menuCanSelect() const{return !menuActions().value(menuFocus_).toMap().value("readOnly").toBool();}
    void setExtraActions(QVariantList actions);
    void setPanel(QString panel,QString caption,QVariantList actions,QString backAction = {});
    void setGameTitle(const QString& title) { gameTitle_ = title; emit changed(); }
    bool requestMenu();
    void menuCaptureCompleted(quint64 token, const QImage&);
    Q_INVOKABLE void activateMenu(int index);
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
    void menuCaptureRequested(quint64 token);
    void menuDismissed();
    void menuActionRequested(QString action);
private:
    void resetInput();
    void dismissMenu();
    AdventureExitController& exit_;
    AdventureExitController::Phase phase_;
    bool isolated_ = false, focused_ = false, ready_ = false;
    bool previousConfirm_ = false, previousBack_ = false, slowClose_ = false;
    quint64 generation_ = 0;
    quint64 menuAttempt_ = 0;
    bool menuOpen_ = false, menuPending_ = false;
    bool previousHome_ = false, previousUp_ = false, previousDown_ = false;
    int menuFocus_ = 0;
    QString gameTitle_;
    QString panel_,caption_,backAction_;
    QVariantList extras_,panelActions_;
    QImage menuFrame_;
    QTimer menuTimer_;
    QTimer closeTimer_;
};
}
