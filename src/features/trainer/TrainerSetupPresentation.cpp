#include "TrainerSetupPresentation.h"
#include "core/input/TextEntryController.h"
#include <algorithm>

namespace trainer {
void TrainerSetupPresentation::moveTo(const QString& stage, int focus) {
    stage_ = stage; focus_ = focus; error_.clear(); emit changed();
}
void TrainerSetupPresentation::begin() { close(); moveTo("menu"); }
void TrainerSetupPresentation::close() {
    name_.clear(); pin_.clear(); firstPin_.clear(); pinChosen_ = false;
    emblem_ = "compass"; favorite_ = "Not chosen"; moveTo("menu");
}
QString TrainerSetupPresentation::title() const {
    if (stage_ == "menu") return "Meet your Trainers";
    if (stage_ == "chooser") return "Who's playing?";
    if (stage_ == "welcome") return "Your story starts here";
    if (stage_ == "identity") return "Make your Trainer card";
    if (stage_ == "pin") return "Choose a PIN";
    if (stage_ == "repeat") return "Once more";
    if (stage_ == "unlock") return "Welcome back, Sky";
    if (stage_ == "review") return "Ready for your journey?";
    return "Rehearsal complete";
}
QString TrainerSetupPresentation::description() const {
    if (stage_ == "menu") return "Try the upcoming registration and player selection screens.";
    if (stage_ == "chooser") return "Sample Trainer cards. Your actual profile stays unchanged.";
    if (stage_ == "welcome") return "Choose a name, an emblem and a favorite companion.";
    if (stage_ == "identity") return "Your name is required. A favorite is up to you.";
    if (stage_ == "pin") return "Try 4–6 digits, or skip. This preview does not lock anything.";
    if (stage_ == "repeat") return "Enter the same digits again. B returns to the first entry.";
    if (stage_ == "unlock") return "Sample PIN: 1234. No personal records are unlocked.";
    if (stage_ == "review") return "Review your sample card. Finish ends the preview without saving.";
    return "No profile was created or switched. No PIN was stored.";
}
QVariantList TrainerSetupPresentation::rows() const {
    QVariantList result;
    const auto add = [&](const QString& label, const QString& detail = QString()) {
        result.append(QVariantMap{{"label", label}, {"detail", detail}});
    };
    if (stage_ == "menu") { add("Registration", "Welcome, identity, optional PIN and review"); add("Choose a Trainer", "Sample cards and PIN entry"); add("Back to settings"); }
    else if (stage_ == "chooser") { add("River", "Sample Trainer · no PIN"); add("Sky", "Sample Trainer · PIN"); add("Add Trainer", "Rehearse registration"); add("Back"); }
    else if (stage_ == "welcome") { add("Let's begin"); add("Back"); }
    else if (stage_ == "identity") {
        add("Name", name_.isEmpty() ? "A · Enter your name" : name_);
        add("Emblem", emblem_); add("Favorite", favorite_); add("Continue");
    } else if (stage_ == "review") { add("Finish preview"); add("Edit card"); add("Change PIN choice"); }
    else if (stage_ == "done") add("Back to preview menu");
    return result;
}
void TrainerSetupPresentation::applyName(const QString& value) {
    if (stage_ != "identity") return;
    name_ = value.trimmed(); error_.clear(); emit changed();
}
void TrainerSetupPresentation::back() {
    pin_.clear(); error_.clear();
    if (stage_ == "menu") { close(); emit closeRequested(); }
    else if (stage_ == "identity") moveTo("welcome");
    else if (stage_ == "pin") { firstPin_.clear(); moveTo("identity", 3); }
    else if (stage_ == "repeat") { firstPin_.clear(); moveTo("pin"); }
    else if (stage_ == "unlock") moveTo("chooser", 1);
    else if (stage_ == "review") { firstPin_.clear(); moveTo("pin"); }
    else { close(); }
}
void TrainerSetupPresentation::activate(int index) {
    if (keypad()) {
        if (index < 0 || index > (stage_ == "pin" ? 13 : 12)) return;
        focus_ = index; error_.clear();
        if (index < 9 && pin_.size() < 6) pin_ += QChar('1' + index);
        else if (index == 10 && pin_.size() < 6) pin_ += '0';
        else if (index == 9) pin_.chop(1);
        else if (index == 11) pin_.clear();
        else if (index == 13) { pin_.clear(); firstPin_.clear(); pinChosen_ = false; moveTo("review"); return; }
        else if (index == 12) {
            if (pin_.size() < 4) error_ = "Use 4–6 digits, or go back.";
            else if (stage_ == "pin") { firstPin_ = pin_; pin_.clear(); moveTo("repeat"); return; }
            else if (stage_ == "repeat") {
                if (pin_ != firstPin_) { error_ = "Those digits didn't match. Try again."; pin_.clear(); focus_ = 0; }
                else { pin_.clear(); firstPin_.clear(); pinChosen_ = true; moveTo("review"); return; }
            } else if (pin_ == "1234") { pin_.clear(); moveTo("done"); return; }
            else { pin_.clear(); error_ = "Try the sample PIN: 1234."; focus_ = 0; }
        }
        emit changed(); return;
    }
    if (index < 0 || index >= rows().size()) return;
    focus_ = index;
    if (stage_ == "menu") { if (index == 2) back(); else moveTo(index == 0 ? "welcome" : "chooser"); }
    else if (stage_ == "chooser") {
        if (index == 0) moveTo("done"); else if (index == 1) { pin_.clear(); moveTo("unlock"); }
        else if (index == 2) { close(); moveTo("welcome"); } else back();
    } else if (stage_ == "welcome") { if (index == 0) moveTo("identity"); else back(); }
    else if (stage_ == "identity") {
        if (index == 0) emit nameRequested(name_);
        else if (index == 1) { const QStringList items{"compass", "leaf", "spark"}; emblem_ = items[(items.indexOf(emblem_) + 1) % items.size()]; }
        else if (index == 2) { const QStringList items{"Not chosen", "Bulbasaur", "Charmander", "Squirtle"}; favorite_ = items[(items.indexOf(favorite_) + 1) % items.size()]; }
        else if (name_.isEmpty() || TextEntryController::characterCount(name_) > 24
            || std::any_of(name_.begin(), name_.end(), [](QChar c) { return !c.isPrint(); })) {
            error_ = "Choose a name of 1–24 characters."; focus_ = 0;
        } else { pin_.clear(); firstPin_.clear(); moveTo("pin"); }
        emit changed();
    } else if (stage_ == "review") {
        if (index == 0) moveTo("done"); else if (index == 1) moveTo("identity"); else moveTo("pin");
    } else close();
}
void TrainerSetupPresentation::dispatch(Action action) {
    if (action == Action::Back) { back(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (keypad()) {
        const int last = stage_ == "pin" ? 13 : 12;
        if (action == Action::Up) focus_ = focus_ >= 12 ? 10 : std::max(0, focus_ - 3);
        if (action == Action::Down) focus_ = focus_ >= 9 ? 12 : focus_ + 3;
        if (action == Action::Left) focus_ = focus_ >= 12 ? 12 : focus_ % 3 ? focus_ - 1 : focus_;
        if (action == Action::Right) focus_ = focus_ >= 12 ? last : focus_ % 3 < 2 ? focus_ + 1 : focus_;
    } else {
        if (action == Action::Up) focus_ = std::max(0, focus_ - 1);
        if (action == Action::Down) focus_ = std::min(int(rows().size()) - 1, focus_ + 1);
    }
    emit changed();
}
}
