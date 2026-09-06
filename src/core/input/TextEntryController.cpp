#include "TextEntryController.h"
#include <QTextBoundaryFinder>
#include <algorithm>
#include <limits>

namespace trainer {
TextEntryController::TextEntryController(QObject* parent) : QObject(parent) {
    for (int i = 0; i < 26; ++i) {
        const QString letter(QChar('A' + i));
        keys_.append({letter, letter, i / 10, i % 10, 1, KeyKind::Character});
    }
    keys_.append({"delete", "Delete", 2, 6, 2, KeyKind::Delete});
    keys_.append({"clear", "Clear", 2, 8, 2, KeyKind::Clear});
    keys_.append({"space", "Space", 3, 0, 6, KeyKind::Space});
    keys_.append({"apply", "Apply", 3, 6, 4, KeyKind::Apply});
    // Column 10 is a physical gap between the letter and number modules.
    for (int i = 0; i < 9; ++i) {
        const auto number = QString::number(i + 1);
        keys_.append({number, number, i / 3, 11 + i % 3, 1, KeyKind::Character});
    }
    keys_.append({"0", "0", 3, 11, 3, KeyKind::Character});
}
int TextEntryController::characterCount(const QString& text) {
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int count = 0;
    while (finder.toNextBoundary() > 0) ++count;
    return count;
}
int TextEntryController::count() const { return characterCount(text_); }
QVariantList TextEntryController::keys() const {
    QVariantList result;
    for (const auto& key : keys_) {
        result.append(QVariantMap{{"id", key.id}, {"label", key.label}, {"row", key.row},
            {"column", key.column}, {"span", key.span}, {"numeric", key.column >= 11}});
    }
    return result;
}
void TextEntryController::begin(const QString& title, const QString& initial, int maximumLength) {
    title_ = title;
    text_ = initial; // Do not silently truncate an existing value.
    maximumLength_ = std::clamp(maximumLength, 1, 256);
    hint_.clear();
    focus_ = 0;
    preferredColumn_ = 0;
    open_ = true;
    emit changed();
}
void TextEntryController::cancel() {
    if (!open_) return;
    open_ = false;
    text_.clear();
    hint_.clear();
    emit changed();
}
void TextEntryController::move(Action action) {
    const auto& current = keys_.at(focus_);
    int target = focus_;
    if (action == Action::Left || action == Action::Right) {
        int distance = std::numeric_limits<int>::max();
        for (int i = 0; i < keys_.size(); ++i) {
            const auto& candidate = keys_.at(i);
            if (candidate.row != current.row) continue;
            const int delta = (candidate.column - current.column) * (action == Action::Right ? 1 : -1);
            if (delta > 0 && delta < distance) { distance = delta; target = i; }
        }
        if (target != focus_) preferredColumn_ = keys_[target].column + keys_[target].span / 2;
    } else if (action == Action::Up || action == Action::Down) {
        const int row = current.row + (action == Action::Down ? 1 : -1);
        int distance = std::numeric_limits<int>::max();
        for (int i = 0; i < keys_.size(); ++i) {
            const auto& candidate = keys_.at(i);
            if (candidate.row != row) continue;
            const int nearest = std::clamp(preferredColumn_, candidate.column,
                                           candidate.column + candidate.span - 1);
            const int delta = std::abs(nearest - preferredColumn_);
            if (delta < distance) { distance = delta; target = i; }
        }
        // Retain the intended column when passing through Space, Apply, Clear or 0.
    }
    focus_ = target;
}
void TextEntryController::activate(int index) {
    if (!open_ || index < 0 || index >= keys_.size()) return;
    if (focus_ != index) {
        focus_ = index;
        preferredColumn_ = keys_[index].column + keys_[index].span / 2;
    }
    const auto& key = keys_.at(focus_);
    hint_.clear();
    if (key.kind == KeyKind::Apply) {
        if (count() > maximumLength_) {
            hint_ = QString("Use up to %1 characters.").arg(maximumLength_);
        } else {
            const QString result = text_;
            open_ = false;
            text_.clear();
            emit changed();
            emit accepted(result);
            return;
        }
    } else if (key.kind == KeyKind::Clear) text_.clear();
    else if (key.kind == KeyKind::Delete) {
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text_);
        finder.toEnd();
        const auto boundary = finder.toPreviousBoundary();
        if (boundary >= 0) text_.truncate(boundary);
    } else if (count() >= maximumLength_) {
        hint_ = QString("Maximum %1 characters reached.").arg(maximumLength_);
    } else {
        text_.append(key.kind == KeyKind::Space ? " " : key.label);
    }
    emit changed();
}
void TextEntryController::dispatch(Action action) {
    if (!open_) return;
    if (action == Action::Back) cancel();
    else if (action == Action::Confirm) activate(focus_);
    else if (action <= Action::Right) { move(action); emit changed(); }
    // The shell owns Start and L1/R1. Y has no text-entry meaning.
}
}
