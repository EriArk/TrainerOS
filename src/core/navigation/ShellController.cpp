#include "ShellController.h"
#include <algorithm>

namespace trainer {
ShellController::ShellController(LibraryRepository& repo, TrainerRepository& profiles, AdventureAdapter& adapter,
        PlatformService& platform, PokedexReferenceProvider& dexReference, PokedexProgressRepository& dexProgress,
        HallOfFameRepository& archive, AchievementProvider& achievements, QObject* parent)
    : QObject(parent), repository_(repo), adapter_(adapter), platform_(platform),
      keyboard_(this), trainer_(profiles, this), worlds_(repo, adapter, this),
      pokedex_(dexReference, dexProgress, this), hall_(archive, achievements, this),
      libraryManager_(repo, nullptr, this), settings_(this), diagnostics_(this), points_(repo.resumePoints()) {
    connect(&diagnostics_, &DiagnosticsController::closeRequested, this, [this] { service_.clear(); menuOpen_ = true; emit changed(); });
    connect(&diagnostics_, &DiagnosticsController::messageRequested, this, [this](const QString& text) {
        if (service_ != "diagnostics" || menuOpen_) { notice_ = text; emit changed(); }
    });
    connect(&libraryManager_, &LibraryManagementController::changed, this, &ShellController::changed);
    connect(&libraryManager_, &LibraryManagementController::saved, this, &ShellController::refreshLibrary);
    connect(&libraryManager_, &LibraryManagementController::messageRequested, this, [this](const QString& text) { notice_ = text; emit changed(); });
    connect(&libraryManager_, &LibraryManagementController::closeRequested, this, [this] { service_.clear(); menuOpen_ = !libraryFromWorlds_; libraryFromWorlds_ = false; emit changed(); });
    connect(&worlds_, &WorldsController::setupRequested, this, [this](const QString& id) {
        libraryFromWorlds_ = true; service_ = "library"; libraryManager_.beginEdit(id); emit changed();
    });
    connect(&libraryManager_, &LibraryManagementController::textRequested, this, [this](const QString& title, const QString& initial, int limit) {
        textTarget_ = TextTarget::Library; keyboard_.begin(title, initial, limit);
    });
    connect(&settings_, &SettingsController::changed, this, &ShellController::changed);
    connect(&settings_, &SettingsController::messageRequested, this, [this](const QString& message) {
        if (service_ != "settings" || menuOpen_) { notice_ = message; emit changed(); }
    });
    connect(&settings_, &SettingsController::closeRequested, this, [this] { service_.clear(); menuOpen_ = true; emit changed(); });
    connect(&hall_, &HallOfFameController::changed, this, &ShellController::changed);
    connect(&hall_, &HallOfFameController::messageRequested, this, [this](const QString& message) {
        notice_ = message; emit changed();
    });
    connect(&pokedex_, &PokedexController::changed, this, &ShellController::changed);
    connect(&pokedex_, &PokedexController::messageRequested, this, [this](const QString& message) {
        notice_ = message; emit changed();
    });
    connect(&pokedex_, &PokedexController::searchRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::PokedexSearch;
        keyboard_.begin("Pokédex · name or number", initial, 32);
    });
    connect(&worlds_, &WorldsController::changed, this, &ShellController::changed);
    connect(&worlds_, &WorldsController::messageRequested, this, [this](const QString& message) {
        notice_ = message;
        emit changed();
    });
    connect(&worlds_, &WorldsController::homeRequested, this, [this] { goToPage(0); });
    connect(&keyboard_, &TextEntryController::changed, this, &ShellController::changed);
    connect(&trainer_, &TrainerController::changed, this, &ShellController::changed);
    connect(&trainer_, &TrainerController::messageRequested, this, [this](const QString& message) {
        notice_ = message; emit changed();
    });
    connect(&trainer_, &TrainerController::nameRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::TrainerName;
        keyboard_.begin("Trainer name", initial, TrainerController::NameLimit);
    });
    connect(&keyboard_, &TextEntryController::accepted, this, [this](const QString& text) {
        const auto target = textTarget_;
        textTarget_ = TextTarget::None;
        if (target == TextTarget::TrainerName) trainer_.setDraftName(text);
        else if (target == TextTarget::PokedexSearch) pokedex_.applySearch(text);
        else if (target == TextTarget::Library) libraryManager_.applyText(text);
    });
    std::stable_sort(points_.begin(), points_.end(), [](const auto& a, const auto& b) {
        return a.savedAt > b.savedAt;
    });
}
void ShellController::configureServices(FileCatalog* files, PreferencesRepository* preferences) {
    libraryManager_.files()->setCatalog(files); settings_.setRepository(preferences);
}
void ShellController::refreshLibrary() {
    worlds_.refresh(); libraryManager_.refresh();
    const QString selected = drawerFocus_ < points_.size() ? points_[drawerFocus_].id : QString();
    points_ = repository_.resumePoints();
    std::stable_sort(points_.begin(), points_.end(), [](const auto& a, const auto& b) { return a.savedAt > b.savedAt; });
    drawerFocus_ = 0;
    for (int i = 0; i < points_.size(); ++i) if (points_[i].id == selected) drawerFocus_ = i;
    emit changed();
}
int ShellController::focusIndex() const {
    if (!notice_.isEmpty()) return 0;
    if (menuOpen_) return menuFocus_;
    if (keyboard_.isOpen()) return keyboard_.focusIndex();
    if (service_ == "library") return libraryManager_.files()->isOpen() ? libraryManager_.files()->focusIndex() : libraryManager_.focusIndex();
    if (service_ == "settings") return settings_.focusIndex();
    if (service_ == "diagnostics") return diagnostics_.focusIndex();
    if (trainer_.editing()) return trainer_.focusIndex();
    if (page_ == 1) return worlds_.focusIndex();
    if (page_ == 2) return pokedex_.focusIndex();
    if (page_ == 4) return hall_.focusIndex();
    return drawerOpen_ ? drawerFocus_ : pageFocus_[page_];
}
QJsonObject ShellController::navigationState() const {
    const QStringList pages{"home", "worlds", "pokedex", "trainer", "hall"};
    return {{"version", 1}, {"page", pages[page_]},
            {"resume", drawerFocus_ < points_.size() ? points_[drawerFocus_].id : QString()},
            {"worlds", worlds_.navigationState()}, {"pokedex", pokedex_.navigationState()}, {"hall", hall_.navigationState()}};
}
void ShellController::restoreNavigation(const QJsonObject& state) {
    if (state["version"].toInt() != 1) return;
    const QStringList pages{"home", "worlds", "pokedex", "trainer", "hall"};
    goToPage(std::max(0, int(pages.indexOf(state["page"].toString()))));
    worlds_.restoreNavigation(state["worlds"].toObject());
    pokedex_.restoreNavigation(state["pokedex"].toObject());
    hall_.restoreNavigation(state["hall"].toObject());
    drawerFocus_ = 0;
    for (int i = 0; i < points_.size(); ++i) if (points_[i].id == state["resume"].toString()) drawerFocus_ = i;
    emit changed();
}
QVariantMap ShellController::home() const {
    const auto snapshot = repository_.home();
    QString title = "Choose a journey in Worlds", world = "Your journey";
    for (const auto& a : repository_.adventures()) if (a.id == snapshot.activeAdventureId) {
        title = a.title;
        for (const auto& w : repository_.worlds()) if (w.id == a.worldId) world = w.name;
    }
    return {{"trainer", trainer_.exists() ? trainer_.profile()["name"] : "TRAINER"},
            {"hasTrainer", trainer_.exists()}, {"adventure", title}, {"world", world},
            {"badges", snapshot.badges ? QString::number(*snapshot.badges) : "—"},
            {"caught", snapshot.caught ? QString::number(*snapshot.caught) : "—"},
            {"milestone", snapshot.milestone}};
}
QVariantList ShellController::resumePoints() const {
    QVariantList result;
    for (const auto& point : points_) {
        QString title = "Unavailable Adventure";
        QString world = "Unknown World";
        for (const auto& a : repository_.adventures()) if (a.id == point.adventureId) {
            title = a.title;
            for (const auto& w : repository_.worlds()) if (w.id == a.worldId) world = w.name;
        }
        result.append(QVariantMap{{"id", point.id}, {"title", title}, {"world", world},
            {"location", point.location}, {"summary", point.summary},
            {"time", point.savedAt.toUTC().toString("dd MMM · HH:mm 'UTC'")}});
    }
    return result;
}
QStringList ShellController::menuItems() const {
    return {"Settings", "Controller", "Pokémon Center", "Manage Adventures",
            "Desktop / Maintenance Mode", "Steam Gaming Mode", "Exit Development App"};
}
void ShellController::goToPage(int page) {
    keyboard_.cancel();
    textTarget_ = TextTarget::None;
    pokedex_.cancelTransient();
    trainer_.cancel();
    libraryManager_.close(); service_.clear();
    page_ = std::clamp(page, 0, 4); // No wrapping until physical-device testing.
    drawerOpen_ = false;
    menuOpen_ = false;
    notice_.clear();
    emit changed();
}
void ShellController::activate(int index, const QString& area) {
    if (!notice_.isEmpty()) { notice_.clear(); emit changed(); return; }
    if (menuOpen_) menuFocus_ = std::clamp(index, 0, int(menuItems().size()) - 1);
    else if (keyboard_.isOpen()) { keyboard_.activate(index); return; }
    else if (service_ == "library") { libraryManager_.activate(index, area); return; }
    else if (service_ == "settings") { settings_.activate(index); return; }
    else if (service_ == "diagnostics") { diagnostics_.activate(index); return; }
    else if (trainer_.editing()) { trainer_.activate(index); return; }
    else if (page_ == 1) { worlds_.activate(index); return; }
    else if (page_ == 2) {
        if (area.isEmpty()) pokedex_.activate(index);
        else pokedex_.activateControl(area, index);
        return;
    }
    else if (page_ == 4) {
        if (area.isEmpty()) hall_.activate(index);
        else hall_.activateControl(area, index);
        return;
    }
    else if (drawerOpen_) drawerFocus_ = std::clamp(index, 0, std::max(0, int(points_.size()) - 1));
    else pageFocus_[page_] = 0;
    confirm();
    emit changed();
}
void ShellController::confirm() {
    if (!notice_.isEmpty()) { notice_.clear(); return; }
    if (menuOpen_) {
        if (menuFocus_ == 6) { emit exitRequested(); return; }
        if (menuFocus_ == 0 || menuFocus_ == 1 || menuFocus_ == 3) {
            keyboard_.cancel(); textTarget_ = TextTarget::None; trainer_.cancel();
            libraryManager_.close(); menuOpen_ = false;
            service_ = menuFocus_ == 0 ? "settings" : menuFocus_ == 1 ? "diagnostics" : "library";
            if (service_ == "settings") settings_.begin();
            else if (service_ == "diagnostics") diagnostics_.begin();
            else { libraryFromWorlds_ = false; libraryManager_.begin(worlds_.region()["id"].toString()); }
            return;
        }
        notice_ = menuFocus_ >= 4 ? platform_.sessionStatus()
            : "This service is not available in the first prototype yet.";
        return;
    }
    if (drawerOpen_) {
        if (points_.isEmpty()) { drawerOpen_ = false; return; }
        const auto& point = points_.at(drawerFocus_);
        for (const auto& adventure : repository_.adventures()) if (adventure.id == point.adventureId) {
            notice_ = adapter_.capabilities(adventure).directResume
                ? adapter_.resume(adventure, point).message : "Direct resume is unavailable for this Adventure.";
            return;
        }
        notice_ = "This Adventure is unavailable. Your resume point has been kept.";
    } else if (page_ == 0) drawerOpen_ = true;
    else if (page_ == 3) {
        trainer_.beginEdit();
    } else {
        notice_ = "This section is not available in the prototype yet.";
    }
}
void ShellController::dispatch(Action action) {
    if (action == Action::PreviousPage || action == Action::NextPage) {
        goToPage(page_ + (action == Action::NextPage ? 1 : -1));
        return;
    }
    // Global section/system actions outrank the active local layer. Start overlays
    // the keyboard without changing its draft or key focus; Back unwinds it first.
    if (action == Action::SystemMenu) {
        if (notice_.isEmpty()) menuOpen_ = !menuOpen_;
        emit changed();
        return;
    }
    if (notice_.isEmpty() && !menuOpen_) {
        if (keyboard_.isOpen()) { keyboard_.dispatch(action); return; }
        if (service_ == "library") { libraryManager_.dispatch(action); return; }
        if (service_ == "settings") { settings_.dispatch(action); return; }
        if (service_ == "diagnostics") { diagnostics_.dispatch(action); return; }
        if (trainer_.editing()) { trainer_.dispatch(action); return; }
        if (page_ == 1) { worlds_.dispatch(action); return; }
        if (page_ == 2) { pokedex_.dispatch(action); return; }
        if (page_ == 4) { hall_.dispatch(action); return; }
    }
    if (action == Action::Back) {
        if (!notice_.isEmpty()) notice_.clear();
        else if (menuOpen_) menuOpen_ = false;
        else if (drawerOpen_) drawerOpen_ = false;
    } else if (action == Action::ToggleContinue) {
        if (page_ == 0 && !menuOpen_ && notice_.isEmpty()) drawerOpen_ = !drawerOpen_;
    } else if (action == Action::Confirm) confirm();
    else if (notice_.isEmpty()) {
        int* focus = menuOpen_ ? &menuFocus_ : drawerOpen_ ? &drawerFocus_ : &pageFocus_[page_];
        const int count = menuOpen_ ? menuItems().size() : drawerOpen_ ? std::max(1, int(points_.size())) : 1;
        int delta = 0;
        if (menuOpen_) delta = action == Action::Up ? -1 : action == Action::Down ? 1 : 0;
        else if (drawerOpen_) delta = action == Action::Left ? -1 : action == Action::Right ? 1 : 0;
        *focus = std::clamp(*focus + delta, 0, std::max(0, count - 1));
    }
    emit changed();
}
}
