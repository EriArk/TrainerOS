#include "TrainerController.h"
#include "core/input/TextEntryController.h"
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
const QStringList emblems{"compass", "leaf", "spark"};
}
TrainerController::TrainerController(TrainerRepository& repository, QObject* parent)
    : QObject(parent), repository_(repository), picker_(this), profile_(repository.load()) {
    connect(&picker_, &SpeciesPicker::changed, this, &TrainerController::changed);
    connect(&picker_, &SpeciesPicker::selected, this, [this](const QString& id) {
        if (!editing_ || saving_) return;
        draft_.favoritePokemonId = id; error_.clear(); emit changed();
    });
}
void TrainerController::configure(LibraryRepository* library, PokedexReferenceProvider* reference,
        PokedexProgressRepository* journal, HallOfFameRepository* archive) {
    library_ = library; reference_ = reference; journal_ = journal; archive_ = archive;
    picker_.setReference(reference);
}
void TrainerController::refreshOverview() {
    if (!library_ || !reference_ || !journal_ || !archive_) return;
    const auto reference = reference_->load();
    if (reference.success) {
        names_.clear();
        for (const auto& entry : reference.entries) names_.insert(entry.id, entry.name);
    }
    overview_ = trainerOverview(*library_, reference, *journal_, *archive_);
    emit changed();
}
QVariantList TrainerController::overview() const {
    const auto count = [](const std::optional<int>& value) { return value ? QString::number(*value) : QString("—"); };
    const auto seconds = overview_.recordedSeconds;
    const QString time = !seconds ? "—" : *seconds < 60 ? "< 1 min"
        : *seconds < 3600 ? QString::number(*seconds / 60) + " min"
        : QString("%1 h %2 m").arg(*seconds / 3600).arg((*seconds % 3600) / 60);
    return {QVariantMap{{"label", "ADVENTURES"}, {"value", QString::number(overview_.adventures)}},
        QVariantMap{{"label", "WORLDS"}, {"value", QString::number(overview_.worlds)}},
        QVariantMap{{"label", "FAVORITE MARKS"}, {"value", count(overview_.favorites)}},
        QVariantMap{{"label", "MEMORIES"}, {"value", count(overview_.memories)}},
        QVariantMap{{"label", "RECORDED TIME"}, {"value", time}}};
}
QString TrainerController::favoriteLabel(const QString& id) const {
    if (id.isEmpty()) return "Not chosen";
    return names_.value(id, id.left(1).toUpper() + id.mid(1));
}
QVariantMap TrainerController::profile() const {
    const TrainerProfile current = profile_.value_or(TrainerProfile{});
    return {{"id", current.id}, {"name", current.name}, {"emblem", current.emblemId},
            {"favorite", favoriteLabel(current.favoritePokemonId)}};
}
void TrainerController::beginEdit() {
    if (editing_ || saving_) return;
    draft_ = profile_.value_or(TrainerProfile{});
    if (!profile_) {
        draft_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        draft_.createdAt = QDateTime::currentDateTimeUtc();
    }
    error_.clear();
    focus_ = 0;
    editing_ = true;
    emit changed();
}
void TrainerController::cancel() {
    picker_.cancel();
    if (!editing_) return;
    editing_ = false;
    draft_ = {};
    error_.clear();
    emit changed();
}
void TrainerController::setDraftName(const QString& name) {
    if (!editing_ || saving_) return;
    draft_.name = name;
    error_.clear();
    emit changed();
}
void TrainerController::save() {
    TrainerProfile candidate = draft_;
    candidate.name = candidate.name.trimmed();
    if (candidate.name.isEmpty()) error_ = "Give your Trainer a name before saving.";
    else if (TextEntryController::characterCount(candidate.name) > NameLimit)
        error_ = QString("Your Trainer name can have up to %1 characters.").arg(NameLimit);
    else if (std::any_of(candidate.name.begin(), candidate.name.end(), [](QChar c) {
        return c.category() == QChar::Other_Control || c.category() == QChar::Separator_Line
            || c.category() == QChar::Separator_Paragraph;
    })) error_ = "Use a single line for your Trainer name.";
    else error_.clear();
    if (!error_.isEmpty()) { focus_ = 0; return; }
    saving_ = true;
    repository_.saveAsync(candidate, this, [this, candidate](const ProfileWriteResult& result) {
        saving_ = false;
        if (!result.success) {
            error_ = result.error.isEmpty() ? "Couldn't save your Trainer. Try again." : result.error;
            if (!editing_) emit messageRequested(error_);
        } else {
            profile_ = candidate;
            draft_ = {};
            editing_ = false;
            error_.clear();
        }
        emit changed();
    });
}
void TrainerController::reload() {
    if (saving_ || editing_) return;
    profile_ = repository_.load();
    emit changed();
}
void TrainerController::activate(int index) {
    if (picker_.isOpen()) { picker_.activate(index); return; }
    if (editing_ && index == 4) { cancel(); return; }
    if (!editing_ || saving_ || index < 0 || index > 4) return;
    focus_ = index;
    switch (index) {
    case 0: emit nameRequested(draft_.name); break;
    case 1: draft_.emblemId = emblems[(emblems.indexOf(draft_.emblemId) + 1) % emblems.size()]; break;
    case 2: picker_.begin(draft_.favoritePokemonId); break;
    case 3: save(); break;
    case 4: cancel(); return;
    }
    emit changed();
}
void TrainerController::dispatch(Action action, bool vertical) {
    if (!editing_) return;
    if (picker_.isOpen()) { picker_.dispatch(action); return; }
    if (action == Action::Back) { cancel(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (vertical && (action == Action::Up || action == Action::Down)) {
        focus_ = std::clamp(focus_ + (action == Action::Down ? 1 : -1), 0, 4);
        emit changed(); return;
    }
    if (action == Action::Up) focus_ = focus_ >= 3 ? 2 : std::max(0, focus_ - 1);
    if (action == Action::Down) focus_ = focus_ < 3 ? focus_ + 1 : focus_;
    if (action == Action::Right && focus_ == 3) focus_ = 4;
    if (action == Action::Left && focus_ == 4) focus_ = 3;
    emit changed();
}
}
