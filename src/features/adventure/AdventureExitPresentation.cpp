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
    QVariantList result{QVariantMap{{"id","continue"},{"label","Continue"}}};
    result.append(extras_);
    result.append(QVariantMap{{"id","exit"},{"label","Exit game"}});return result;
}
void AdventureExitPresentation::setPanel(QString panel,QString caption,QVariantList actions,QString backAction) {
    const auto selected=menuActions().value(menuFocus_).toMap().value("id");
    const bool same=panel==panel_;panel_=std::move(panel);caption_=std::move(caption);panelActions_=std::move(actions);
    backAction_=std::move(backAction);
    bool selectionLost=false;
    if(!same)menuFocus_=0;
    else {
        selectionLost=selected.isValid();
        for(int i=0;i<panelActions_.size();++i)if(panelActions_[i].toMap().value("id")==selected){menuFocus_=i;selectionLost=false;break;}
        if(selectionLost){menuFocus_=panelActions_.size();panelActions_.append(QVariantMap{{"id",selected},{"label","No longer available"},{"readOnly",true}});resetInput();}
    }
    menuFocus_=qBound(0,menuFocus_,qMax(0,int(menuActions().size())-1));
    if(!selectionLost&&menuActions().value(menuFocus_).toMap()["readOnly"].toBool())
        for(int i=0;i<panelActions_.size();++i)if(!panelActions_[i].toMap()["readOnly"].toBool()){menuFocus_=i;break;}
    if(!same)resetInput();emit changed();
}
void AdventureExitPresentation::setExtraActions(QVariantList actions) {
    const auto selected=menuActions().value(menuFocus_).toMap()["id"].toString();
    bool lost=false;
    if(panel_.isEmpty() && !selected.isEmpty() && selected!="continue" && selected!="exit") {
        lost=true;for(const auto& row:actions)if(row.toMap()["id"]==selected){lost=false;break;}
        if(lost)actions.append(QVariantMap{{"id",selected},{"label","No longer available"},{"readOnly",true}});
    }
    if(extras_==actions)return;
    extras_=std::move(actions);
    if(panel_.isEmpty()) {
        // A new ring must not replace the action currently under the controller.
        menuFocus_=0;
        const auto rows=menuActions();
        for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==selected){menuFocus_=i;break;}
        if(lost)resetInput();
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
void AdventureExitPresentation::handOffToShell() {
    menuTimer_.stop();++menuAttempt_;
    menuPending_=menuOpen_=false;menuFrame_={};panel_.clear();
    resetInput();emit changed(); // Deliberately retain the platform input lease.
}
void AdventureExitPresentation::activateMenu(int index) {
    if (!menuOpen_ || !ready_ || !exit_.available() || phase_ != Phase::Idle) return;
    const auto id=menuActions().value(index).toMap()["id"].toString();
    if(id.isEmpty()||menuActions().value(index).toMap().value("readOnly").toBool())return;
    if(!panel_.isEmpty()){if(id=="back")setPanel({}, {}, {});else emit menuActionRequested(id);return;}
    if(id=="continue")dismissMenu();
    else if(id=="exit")exitFromMenu();
    else emit menuActionRequested(id);
}
void AdventureExitPresentation::activateAction(const QString& id) {
    const auto actions=menuActions();
    for(int i=0;i<actions.size();++i)if(actions[i].toMap()["id"].toString()==id){activateMenu(i);return;}
}
bool AdventureExitPresentation::exitFromMenu() {
    if(!menuOpen_ || !exit_.available() || phase_ != Phase::Idle)return false;
    menuOpen_ = false; menuFrame_ = {}; resetInput(); emit changed();
    // The separate exit attempt captures a fresh clean frame after this window
    // is hidden. The menu preview is never published as exit history.
    if (!exit_.requestExit()) {emit menuDismissed();return false;}
    return true;
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
        if (up || down) {
            const auto rows=menuActions();const int direction=up?-1:1;
            for(int i=menuFocus_+direction;i>=0&&i<rows.size();i+=direction)
                if(!rows[i].toMap()["readOnly"].toBool()){menuFocus_=i;break;}
            emit changed();
        }
        else if (confirm && !input.back && !input.home) activateMenu(menuFocus_);
    } else if (confirm && !input.back && !input.home) this->confirm();
}
void AdventureExitPresentation::confirm() { if (ready_ && confirming()) exit_.confirm(); }
void AdventureExitPresentation::cancel() {
    if (!ready_) return;
    if (menuOpen_) {if(!backAction_.isEmpty()&&!panel_.isEmpty())emit menuActionRequested(backAction_);else if(!panel_.isEmpty())setPanel({}, {}, {});else dismissMenu();}
    else if (confirming()) exit_.cancel();
}
}
