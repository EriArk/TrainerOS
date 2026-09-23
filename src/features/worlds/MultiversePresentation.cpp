#include "MultiversePresentation.h"
#include <algorithm>

namespace trainer {
MultiversePresentation::MultiversePresentation(bool sample, QObject* parent) : QObject(parent), sample_(sample) {
    if (sample_) entries_ = {{"sample-courier", "gb", "Star Courier", true},
        {"sample-lantern", "gb", "Lantern Valley", false}, {"sample-orbit", "snes", "Orbit Rally", true},
        {"sample-forest", "gc", "Forest of Echoes", false}, {"sample-neon", "ps", "Neon Circuit", true},
        {"sample-cloud", "psp", "Cloud Atlas", false}};
}
QVariantList MultiversePresentation::systems() const {
    QVariantList result;
    // Production has no bound Multiverse ROMs yet. Never show the fixture systems
    // as if they were installed. P3/P4 supplies the real platform/content projection.
    if (!sample_) return result;
    const QList<QStringList> data{{"gb","Game Boy","handheld"},{"snes","Super Nintendo","cartridge"},
        {"ps","PlayStation","disc"},{"dc","Dreamcast","disc"},{"gc","GameCube","cube"},{"psp","PSP","handheld"}};
    for (const auto& system : data) result.append(QVariantMap{{"id",system[0]}, {"name",system[1]}, {"shape",system[2]}});
    return result;
}
QString MultiversePresentation::systemName() const {
    for (const auto& item : systems()) if (item.toMap()["id"] == system_) return item.toMap()["name"].toString();
    return {};
}
QString MultiversePresentation::filterLabel() const { return QStringList{"All titles","Linked only","Missing files"}[filters_.value(system_)]; }
QVariantMap MultiversePresentation::present(const Game& game) const {
    QString system;
    for (const auto& item : systems()) if (item.toMap()["id"] == game.system) system = item.toMap()["name"].toString();
    return {{"id",game.id},{"title",game.title},{"system",system},{"linked",game.linked},
        {"status",game.linked ? "Sample linked entry" : "Sample missing file"}};
}
QList<MultiversePresentation::Game> MultiversePresentation::filtered() const {
    QList<Game> result;
    const int filter = filters_.value(system_);
    for (const auto& game : entries_) if (game.system == system_ && game.title.contains(query(), Qt::CaseInsensitive)
        && (filter != 1 || game.linked) && (filter != 2 || !game.linked)) result.append(game);
    return result;
}
QVariantList MultiversePresentation::games() const {
    QVariantList result; for (const auto& game : filtered()) result.append(present(game)); return result;
}
int MultiversePresentation::focusIndex() const {
    if (route_ == "systems") return systemFocus_;
    if (route_ == "detail") return detailFocus_;
    return std::clamp(positions_.value(system_), 0, std::max(0, int(filtered().size()) - 1));
}
QVariantMap MultiversePresentation::detail() const {
    const auto list = filtered(); return list.isEmpty() ? QVariantMap{} : present(list[std::clamp(positions_.value(system_),0,int(list.size())-1)]);
}
QVariantMap MultiversePresentation::selected() const {
    for (const auto& game : entries_) if (game.id == selected_) return present(game);
    return {};
}
QVariantList MultiversePresentation::choices() const {
    QVariantList result;
    for (const auto& game : entries_) if (game.linked) {
        auto row = present(game);
        row["world"] = row["system"]; row["preview"] = ""; row["previewLabel"] = "Development sample";
        row["location"] = ""; row["time"] = "No play history"; row["summary"] = "Select for Home · no launch in preview";
        result.append(row);
    }
    return result;
}
void MultiversePresentation::select(const QString& id) {
    for (const auto& game : entries_) if (game.id == id && game.linked) { selected_ = id; emit changed(); return; }
}
void MultiversePresentation::applySearch(const QString& text) {
    if (route_ != "games") return;
    queries_[system_] = text.trimmed().left(48); positions_[system_] = 0; emit changed();
}
void MultiversePresentation::activate(int index) {
    if (route_ == "systems") {
        if (index < 0 || index >= systems().size()) return;
        systemFocus_ = index; system_ = systems()[index].toMap()["id"].toString(); route_ = "games";
    } else if (route_ == "games") {
        const auto list = filtered();
        if (list.isEmpty()) {
            if (!query().isEmpty() || filters_.value(system_)) { queries_[system_].clear(); filters_[system_] = 0; }
            else route_ = "systems";
        } else {
            if (index < 0 || index >= list.size()) return;
            positions_[system_] = index; route_ = "detail"; detailFocus_ = list[index].linked ? 0 : 1;
        }
    } else {
        if (index == 0 && detail().value("linked").toBool()) {
            select(detail()["id"].toString()); emit homeRequested();
        } else if (index == 1) route_ = "games";
    }
    emit changed();
}
void MultiversePresentation::dispatch(Action action) {
    if (action == Action::Back) {
        if (route_ == "detail") route_ = "games";
        else if (route_ == "games") route_ = "systems";
        emit changed(); return;
    }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (route_ == "games" && action == Action::Secondary) { emit searchRequested(query()); return; }
    if (route_ == "games" && action == Action::ToggleContinue) { filters_[system_] = (filters_.value(system_) + 1) % 3; positions_[system_] = 0; }
    if (route_ == "systems") {
        const int delta = action == Action::Left ? -1 : action == Action::Right ? 1 : action == Action::Up ? -3 : action == Action::Down ? 3 : 0;
        systemFocus_ = std::clamp(systemFocus_ + delta, 0, std::max(0, int(systems().size()) - 1));
    } else if (route_ == "games") {
        const int delta = action == Action::Up ? -1 : action == Action::Down ? 1 : 0;
        positions_[system_] = std::clamp(focusIndex() + delta,0,std::max(0,int(filtered().size())-1));
    } else if (action == Action::Left && detail().value("linked").toBool()) detailFocus_ = 0;
    else if (action == Action::Right) detailFocus_ = 1;
    emit changed();
}
}
