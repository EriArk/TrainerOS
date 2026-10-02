#include "AdventureExitPresentation.h"

namespace trainer {
using Phase = AdventureExitController::Phase;
AdventureExitPresentation::AdventureExitPresentation(AdventureExitController& exit, QObject* parent)
    : QObject(parent), exit_(exit), phase_(exit.phase()) {
    menuTimer_.setSingleShot(true);
    menuTimer_.setInterval(AdventureExitController::CaptureTimeoutMs);
    connect(&menuTimer_, &QTimer::timeout, this, [this] { menuCaptureCompleted(menuAttempt_, {}); });
    closeTimer_.setSingleShot(true);
    closeTimer_.setInterval(8000);
    connect(&closeTimer_, &QTimer::timeout, this, [this] {
        if (phase_ == Phase::Closing) { slowClose_ = true; emit changed(); }
    });
    connect(&exit_, &AdventureExitController::changed, this, [this] {
        if (!exit_.available() && (menuOpen_ || menuPending_)) dismissMenu();
        if (phase_ != exit_.phase()) {
            phase_ = exit_.phase();
            resetInput();
            slowClose_ = false;
            closeTimer_.stop();
            // Every attempt needs a new proven lease, including retries.
            if (phase_ == Phase::Idle || phase_ == Phase::Capturing) isolated_ = false;
            if (phase_ == Phase::Closing) closeTimer_.start();
        }
        emit changed();
    });
}
bool AdventureExitPresentation::visible() const { return menuOpen_ || phase_ == Phase::Confirming || phase_ == Phase::Closing; }
bool AdventureExitPresentation::confirming() const { return phase_ == Phase::Confirming; }
QVariantList AdventureExitPresentation::menuActions() const {
    if(!panel_.isEmpty())return panelActions_;
    QVariantList result{QVariantMap{{"id","continue"},{"label","Continue"}},QVariantMap{{"id","exit"},{"label","Exit game"}}};
    result.append(extras_);return result;
}
void AdventureExitPresentation::setPanel(QString panel,QString caption,QVariantList actions) {
    const auto selected=menuActions().value(menuFocus_).toMap().value("id");
    const bool same=panel==panel_;panel_=std::move(panel);caption_=std::move(caption);panelActions_=std::move(actions);
    if(!same)menuFocus_=0;
    else for(int i=0;i<panelActions_.size();++i)if(panelActions_[i].toMap().value("id")==selected){menuFocus_=i;break;}
    menuFocus_=qBound(0,menuFocus_,qMax(0,int(menuActions().size())-1));if(!same)resetInput();emit changed();
}
void AdventureExitPresentation::setExtraActions(QVariantList actions) {
    if(extras_==actions)return;
    const auto selected=menuActions().value(menuFocus_).toMap()["id"];
    extras_=std::move(actions);
    if(panel_.isEmpty()) {
        // A new ring must not replace the action currently under the controller.
        menuFocus_=0;
        const auto rows=menuActions();
        for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==selected){menuFocus_=i;break;}
    }
    emit changed();
}
void AdventureExitPresentation::resetInput() {
    ++generation_;
    ready_ = previousConfirm_ = previousBack_ = false;
    previousHome_ = previousUp_ = previousDown_ = false;
}
bool AdventureExitPresentation::requestMenu() {
    if (!exit_.available() || phase_ != Phase::Idle || menuOpen_ || menuPending_) return false;
    menuPending_ = true; menuFocus_ = 0; menuFrame_ = {};panel_.clear();
    const auto token = ++menuAttempt_;
    resetInput(); menuTimer_.start(); emit changed();
    if (menuPending_ && token == menuAttempt_) emit menuCaptureRequested(token);
    return true;
}
void AdventureExitPresentation::menuCaptureCompleted(quint64 token, const QImage& image) {
    if (!menuPending_ || token != menuAttempt_ || !exit_.available() || phase_ != Phase::Idle) return;
    menuTimer_.stop(); menuPending_ = false; menuOpen_ = true;
    menuFrame_ = image.sizeInBytes() <= 64 * 1024 * 1024 ? image.copy() : QImage();
    resetInput(); emit changed();
}
void AdventureExitPresentation::dismissMenu() {
    menuTimer_.stop(); ++menuAttempt_;
    menuPending_ = menuOpen_ = false; menuFrame_ = {};
    resetInput(); emit changed(); emit menuDismissed();
}
void AdventureExitPresentation::activateMenu(int index) {
    if (!menuOpen_ || !ready_ || !exit_.available() || phase_ != Phase::Idle) return;
    const auto id=menuActions().value(index).toMap()["id"].toString();
    if(id.isEmpty()||menuActions().value(index).toMap().value("readOnly").toBool())return;
    if(!panel_.isEmpty()||index>1){if(id=="back")setPanel({}, {}, {});else emit menuActionRequested(id);return;}
    if (index == 0) { dismissMenu(); return; }
    if (index != 1) return;
    menuOpen_ = false; menuFrame_ = {}; resetInput(); emit changed();
    // The separate exit attempt captures a fresh clean frame after this window
    // is hidden. The menu preview is never published as exit history.
    if (!exit_.requestExit()) emit menuDismissed();
}
void AdventureExitPresentation::setInputIsolated(bool value) {
    if (isolated_ == value) return;
    isolated_ = value; resetInput(); emit changed();
}
void AdventureExitPresentation::setWindowFocused(bool value) {
    if (focused_ == value) return;
    focused_ = value; resetInput(); emit changed();
}
void AdventureExitPresentation::updateInput(quint64 generation, const ExitInputSnapshot& input) {
    if (generation != generation_ || (!confirming() && !menuOpen_) || !isolated_ || !focused_) return;
    if (!input.connected) { resetInput(); emit changed(); return; }
    if (!ready_) {
        if (input.allReleased && !input.confirm && !input.back && !input.home && !input.up && !input.down) { ready_ = true; emit changed(); }
        return;
    }
    const bool back = input.back && !previousBack_;
    const bool confirm = input.confirm && !previousConfirm_;
    const bool home = input.home && !previousHome_;
    const bool up = input.up && !previousUp_, down = input.down && !previousDown_;
    previousConfirm_ = input.confirm; previousBack_ = input.back;
    previousHome_ = input.home; previousUp_ = input.up; previousDown_ = input.down;
    // Simultaneous A+B is always the non-destructive choice.
    if (home && menuOpen_) dismissMenu();
    else if (back || home) cancel();
    else if (menuOpen_) {
        // Direction and A together never move onto and activate Exit at once.
        if (up || down) { menuFocus_=qBound(0,menuFocus_+(up?-1:1),qMax(0,int(menuActions().size())-1));emit changed(); }
        else if (confirm && !input.back && !input.home) activateMenu(menuFocus_);
    } else if (confirm && !input.back && !input.home) this->confirm();
}
void AdventureExitPresentation::confirm() { if (ready_ && confirming()) exit_.confirm(); }
void AdventureExitPresentation::cancel() {
    if (!ready_) return;
    if (menuOpen_) {if(!panel_.isEmpty())setPanel({}, {}, {});else dismissMenu();}
    else if (confirming()) exit_.cancel();
}
}
