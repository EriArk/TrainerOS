#include "ShellController.h"
#include "features/home/PlayHistoryController.h"
#include "ResumePresentation.h"
#include <QSet>
#include <algorithm>

namespace trainer {
ShellController::ShellController(LibraryRepository& repo, TrainerRepository& profiles, AdventureAdapter& adapter,
        PlatformService& platform, PokedexReferenceProvider& dexReference, PokedexProgressRepository& dexProgress,
        HallOfFameRepository& archive, AchievementProvider& achievements, QObject* parent)
    : QObject(parent), repository_(repo), adapter_(adapter), platform_(platform),
      keyboard_(this), trainer_(profiles, this), worlds_(repo, adapter, this),
      pokedex_(dexReference, dexProgress, this), hall_(archive, achievements, this),
      libraryManager_(repo, nullptr, this), settings_(this), diagnostics_(this) {
    hall_.editor()->setLibrary(&repo);
    trainer_.configure(&repo, &dexReference, &dexProgress, &archive);
    connect(trainer_.picker(), &SpeciesPicker::searchRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::TrainerFavorite; keyboard_.begin("Find your favorite · name / number", initial, 48);
    });
    connect(pokedex_.journal(), &PokedexJournalEditor::noteRequested, this, [this](const QString& initial) {
        textTarget_=TextTarget::PokedexNote;keyboard_.begin("Your field note · optional",initial,160);
    });
    connect(hall_.editor(), &ArchiveEditor::textRequested, this, [this](const QString& title, const QString& initial, int limit) {
        textTarget_ = TextTarget::Archive; keyboard_.begin(title, initial, limit);
    });
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
    connect(&worlds_, &WorldsController::searchRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::WorldsSearch;
        keyboard_.begin("Worlds · title, version or platform", initial, 48);
    });
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
        else if (target == TextTarget::WorldsSearch) worlds_.applySearch(text);
        else if (target == TextTarget::Library) libraryManager_.applyText(text);
        else if (target == TextTarget::Archive) hall_.editor()->applyText(text);
        else if (target == TextTarget::PokedexNote) pokedex_.journal()->applyNote(text);
        else if (target == TextTarget::TrainerFavorite) trainer_.picker()->applySearch(text);
    });
    refreshContinue();
}
void ShellController::configureServices(FileCatalog* files, PreferencesRepository* preferences) {
    libraryManager_.files()->setCatalog(files); settings_.setRepository(preferences);
}
void ShellController::refreshLibrary() {
    worlds_.refresh(); libraryManager_.refresh();
    if (page_ == 3) trainer_.refreshOverview();
    const QString selected = drawerFocus_ < points_.size() ? points_[drawerFocus_].id : QString();
    refreshContinue();
    drawerFocus_ = 0;
    for (int i = 0; i < points_.size(); ++i) if (points_[i].id == selected) drawerFocus_ = i;
    emit changed();
}
void ShellController::refreshContinue() {
    points_.clear();
    auto states = repository_.resumePoints();
    // Repeated observations replace a card, not its identity. Recency remains
    // the source's save time, never the time a background scan rediscovered it.
    std::stable_sort(states.begin(), states.end(), [](const auto& a, const auto& b) { return a.observedAt > b.observedAt; });
    QSet<QString> ids;
    QSet<QString> represented;
    for (const auto& point : states) {
        if (point.id.isEmpty() || ids.contains(point.id)) continue;
        ids.insert(point.id);
        points_.append({point.id, point.adventureId, point.savedAt, point, {}});
        represented.insert(point.adventureId);
    }
    for (const auto& session : repository_.recentSessions()) {
        if (represented.contains(session.adventureId)) continue;
        represented.insert(session.adventureId);
        points_.append({"recent:" + session.adventureId, session.adventureId, session.startedAt, {}, session});
    }
    std::stable_sort(points_.begin(), points_.end(), [](const auto& a, const auto& b) { return a.recordedAt > b.recordedAt; });
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
    return drawerOpen_ ? drawerFocus_ : 0;
}
QJsonObject ShellController::navigationState() const {
    const QStringList pages{"home", "worlds", "pokedex", "trainer", "hall"};
    return {{"version", 1}, {"page", pages[page_]},
            {"homeAdventure", homeAdventureId_}, {"homeResume", homeResumeId_},
            {"homeResumeSource", homeResumeSource_.toJson()},
            {"resume", drawerFocus_ < points_.size() ? points_[drawerFocus_].id : QString()},
            {"worlds", worlds_.navigationState()}, {"pokedex", pokedex_.navigationState()}, {"hall", hall_.navigationState()}};
}
void ShellController::restoreNavigation(const QJsonObject& state) {
    if (state["version"].toInt() != 1) return;
    const QStringList pages{"home", "worlds", "pokedex", "trainer", "hall"};
    goToPage(std::max(0, int(pages.indexOf(state["page"].toString()))));
    homeAdventureId_ = state["homeAdventure"].toString(); homeResumeId_ = state["homeResume"].toString();
    homeResumeSource_ = ResumeSource::fromJson(state["homeResumeSource"].toObject());
    worlds_.restoreNavigation(state["worlds"].toObject());
    pokedex_.restoreNavigation(state["pokedex"].toObject());
    hall_.restoreNavigation(state["hall"].toObject());
    drawerFocus_ = 0;
    for (int i = 0; i < points_.size(); ++i) if (points_[i].id == state["resume"].toString()) drawerFocus_ = i;
    emit changed();
}
std::optional<Adventure> ShellController::homeAdventure() const {
    const auto adventures = repository_.adventures();
    for (const auto& a : adventures) if (a.id == homeAdventureId_ && !a.collectionOnly) return a;
    const auto latest = repository_.home().activeAdventureId;
    for (const auto& a : adventures) if (a.id == latest && !a.collectionOnly) return a;
    return {};
}
std::optional<ResumePoint> ShellController::homeResumePoint(const QString& adventureId) const {
    for (const auto& point : points_) if (point.id == homeResumeId_ && point.adventureId == adventureId) return point.resumePoint;
    return {};
}
ResumeAvailability ShellController::homeResumeAvailability(const Adventure& adventure) const {
    const auto point = homeResumePoint(adventure.id);
    if (!point) return ResumeAvailability::Missing;
    if (!homeResumeSource_.complete() || point->source != homeResumeSource_) return ResumeAvailability::Stale;
    return adapter_.resumeAvailability(adventure, *point);
}
QVariantMap ShellController::home() const {
    const auto snapshot = repository_.home();
    QString title = "Choose a journey in Worlds", world = "Your journey";
    const auto adventure = homeAdventure();
    QString action = "Explore Worlds", actionHint = "Worlds", milestone = snapshot.milestone;
    std::optional<int> badges, caught;
    std::optional<qint64> seconds;
    if (adventure) {
        title = adventure->title;
        for (const auto& w : repository_.worlds()) if (w.id == adventure->worldId) world = w.name;
        badges = adventure->badges; caught = adventure->caught;
        if (adventure->id == snapshot.activeAdventureId) {
            if (!badges) badges = snapshot.badges;
            if (!caught) caught = snapshot.caught;
        }
        seconds = repository_.recordedSeconds(adventure->id);
        const auto caps = adapter_.capabilities(*adventure);
        const auto resumeStatus = homeResumeAvailability(*adventure);
        action = resumeStatus == ResumeAvailability::Exact ? "Resume Adventure" : caps.launch ? "Start Adventure" : "Set up Adventure";
        actionHint = resumeStatus == ResumeAvailability::Exact ? "Resume" : caps.launch ? "Play" : "Set up";
        if (repository_.editable()) {
            milestone = "Your selected Adventure · Y to choose another";
            for (const auto& recent : repository_.recentSessions()) if (recent.adventureId == adventure->id) {
                milestone = "Last opened " + recent.startedAt.toLocalTime().toString("dd MMM · HH:mm");
                if (recent.outcome == PlaySessionOutcome::Interrupted) milestone += " · Session interrupted";
                break;
            }
        }
        if (!homeResumeId_.isEmpty() && adventure->id == homeAdventureId_)
            milestone = resumeLabel(resumeStatus);
    }
    return {{"trainer", trainer_.exists() ? trainer_.profile()["name"] : "TRAINER"},
            {"hasTrainer", trainer_.exists()}, {"adventure", title}, {"world", world},
            {"adventureId", adventure ? adventure->id : QString()}, {"action", action}, {"actionHint", actionHint},
            {"badges", badges ? QString::number(*badges) : "—"}, {"caught", caught ? QString::number(*caught) : "—"},
            {"recordedTime", seconds ? recordedDuration(*seconds) : "—"}, {"milestone", milestone}};
}
QVariantList ShellController::resumePoints() const {
    QVariantList result;
    const auto adventures = repository_.adventures();
    const auto worlds = repository_.worlds();
    for (const auto& point : points_) {
        QString title = "Unavailable Adventure";
        QString world = "Unknown World";
        auto status = ResumeAvailability::Incompatible;
        for (const auto& a : adventures) if (a.id == point.adventureId) {
            title = a.title;
            if (point.resumePoint) status = adapter_.resumeAvailability(a, *point.resumePoint);
            for (const auto& w : worlds) if (w.id == a.worldId) world = w.name;
        }
        QString summary = "Select for Home · Start there to play";
        QString location;
        if (point.resumePoint) {
            if (status == ResumeAvailability::Exact) { location = point.resumePoint->location; summary = point.resumePoint->summary; }
            else summary = "Select for Home · choose a save in Adventure";
        }
        if (point.session) {
            if (point.session->outcome == PlaySessionOutcome::Interrupted) summary = "Interrupted · duration not recorded";
            else if (point.session->outcome == PlaySessionOutcome::Failed) summary = "Ended with an error · you can retry";
            else if (point.session->elapsedSeconds) summary = recordedDuration(*point.session->elapsedSeconds) + " · Last session";
        }
        result.append(QVariantMap{{"id", point.id}, {"title", title}, {"world", world},
            {"location", location}, {"summary", summary}, {"previewLabel", point.resumePoint ? resumeLabel(status) : "Recent Adventure"},
            {"time", point.recordedAt.toLocalTime().toString("dd MMM · HH:mm")},
            {"preview", point.resumePoint && !point.resumePoint->previewKey.isEmpty() ? "image://moments/" + point.resumePoint->previewKey : QString()}});
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
    hall_.editor()->cancel();
    trainer_.cancel();
    libraryManager_.close(); service_.clear();
    page_ = std::clamp(page, 0, 4); // No wrapping until physical-device testing.
    if (page_ == 3) trainer_.refreshOverview();
    drawerOpen_ = false;
    menuOpen_ = false;
    notice_.clear();
    emit changed();
}
void ShellController::activate(int index, const QString& area) {
    if (area == "continue" && page_ == 0) { dispatch(Action::ToggleContinue); return; }
    if (!notice_.isEmpty()) { notice_.clear(); emit changed(); return; }
    if (menuOpen_) menuFocus_ = std::clamp(index, 0, int(menuItems().size()) - 1);
    else if (keyboard_.isOpen()) { keyboard_.activate(index); return; }
    else if (service_ == "library") { libraryManager_.activate(index, area); return; }
    else if (service_ == "settings") { settings_.activate(index); return; }
    else if (service_ == "diagnostics") { diagnostics_.activate(index); return; }
    else if (trainer_.editing()) { trainer_.activate(index); return; }
    else if (page_ == 1) {
        if (area == "worlds-search") worlds_.dispatch(Action::Secondary);
        else if (area == "worlds-filter") worlds_.dispatch(Action::ToggleContinue);
        else worlds_.activate(index);
        return;
    }
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
            homeAdventureId_ = adventure.id; homeResumeId_ = point.resumePoint ? point.id : QString();
            homeResumeSource_ = point.resumePoint ? point.resumePoint->source : ResumeSource{};
            drawerOpen_ = false;
            return;
        }
        notice_ = "This Adventure is unavailable. Its history has been kept.";
    } else if (page_ == 0) {
        const auto adventure = homeAdventure();
        if (!adventure) { goToPage(1); return; }
        const auto caps = adapter_.capabilities(*adventure);
        const auto point = homeResumePoint(adventure->id);
        const auto status = homeResumeAvailability(*adventure);
        if (!homeResumeId_.isEmpty() && adventure->id == homeAdventureId_ && status != ResumeAvailability::Exact && status != ResumeAvailability::LaunchOnly) {
            homeResumeId_.clear(); homeResumeSource_ = {};
            notice_ = "That saved moment is no longer ready to resume. Home now opens the Adventure so you can choose a save there.";
        } else if (point && status == ResumeAvailability::Exact) {
            // A cached display is not authorization to load a replacement.
            const auto current = repository_.resumePoints();
            const auto found = std::find_if(current.cbegin(), current.cend(), [&](const auto& p) {
                return p.id == point->id && p.adventureId == adventure->id && p.source == homeResumeSource_
                    && adapter_.resumeAvailability(*adventure, p) == ResumeAvailability::Exact;
            });
            if (found == current.cend()) {
                refreshLibrary();
                homeResumeId_.clear(); homeResumeSource_ = {};
                notice_ = "That saved moment changed. Home now opens the Adventure so you can choose a save there.";
                return;
            }
            emit homeLaunchPressed();
            const auto result = adapter_.resume(*adventure, *found);
            if (!result.inProgress) notice_ = result.message;
        } else if (caps.launch) {
            emit homeLaunchPressed();
            const auto result = adapter_.launch(*adventure);
            if (!result.inProgress) notice_ = result.message;
        } else if (repository_.editable()) {
            libraryFromWorlds_ = true; service_ = "library"; libraryManager_.beginEdit(adventure->id);
        } else notice_ = "This Adventure needs play setup.";
    }
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
    else if (notice_.isEmpty() && (menuOpen_ || drawerOpen_)) {
        // Page actions use fixed physical buttons. Only an open list moves focus.
        int* focus = menuOpen_ ? &menuFocus_ : &drawerFocus_;
        const int count = menuOpen_ ? menuItems().size() : std::max(1, int(points_.size()));
        int delta = 0;
        if (menuOpen_) delta = action == Action::Up ? -1 : action == Action::Down ? 1 : 0;
        else if (drawerOpen_) delta = action == Action::Left ? -1 : action == Action::Right ? 1 : 0;
        *focus = std::clamp(*focus + delta, 0, std::max(0, count - 1));
    }
    emit changed();
}
}
