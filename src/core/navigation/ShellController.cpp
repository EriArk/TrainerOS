#include "ShellController.h"
#include "features/home/PlayHistoryController.h"
#include "ResumePresentation.h"
#include "features/home/BadgeAssets.h"
#include <QSet>
#include <QSignalBlocker>
#include <algorithm>
#include <bit>

namespace trainer {
void ShellController::configureProgress(GameProgressProvider* provider) {
    if (progress_) disconnect(progress_, nullptr, this, nullptr);
    progress_ = provider;
    if (progress_) connect(progress_, &GameProgressProvider::changed, this, [this] {
        refreshParty();
        const auto adventure = homeAdventure();
        pokedex_.setSaveProgress(currentAdventureId(), adventure ? adventure->title : QString(),
            progress_->adventureId(), progress_->snapshot());
        emit changed();
    });
    if (progress_) {
        refreshParty();
        const auto adventure = homeAdventure();
        pokedex_.setSaveProgress(currentAdventureId(), adventure ? adventure->title : QString(),
            progress_->adventureId(), progress_->snapshot());
    }
    emit changed();
}
ShellController::ShellController(LibraryRepository& repo, TrainerRepository& profiles, AdventureAdapter& adapter,
        PlatformService& platform, PokedexReferenceProvider& dexReference, PokedexProgressRepository& dexProgress,
        HallOfFameRepository& archive, AchievementProvider& achievements, QObject* parent)
    : QObject(parent), repository_(repo), adapter_(adapter), platform_(platform),
      keyboard_(this), trainer_(profiles, this), worlds_(repo, adapter, this), multiverse_(repo, adapter, this),
      pokedex_(dexReference, dexProgress, this), hall_(archive, achievements, this),
      libraryManager_(repo, nullptr, this), libraryTools_(repo,this), settings_(this), device_(this), diagnostics_(this), center_(repo,this), party_(!repo.editable(),this) {
    connect(&libraryTools_, &LibraryToolsController::changed,this,&ShellController::changed);
    connect(&libraryTools_, &LibraryToolsController::saved,this,&ShellController::refreshLibrary);
    connect(&libraryTools_, &LibraryToolsController::textRequested,this,[this](const QString& title,const QString& initial,int limit){
        textTarget_=TextTarget::LibraryTools;keyboard_.begin(title,initial,limit);
    });
    connect(&settings_, &SettingsController::trashRequested,this,[this]{libraryTools_.beginTrash();});
    connect(&party_, &PartyPresentation::changed, this, &ShellController::changed);
    connect(&center_, &SaveCenterController::changed, this, [this] {
        if (center_.confirming()) party_.openSaves();
    });
    connect(&multiverse_, &MultiversePresentation::changed, this, &ShellController::changed);
    connect(&multiverse_, &MultiversePresentation::searchRequested, this, [this](const QString& text) {
        textTarget_ = TextTarget::MultiverseSearch; keyboard_.begin("Multiverse · find a title", text, 48);
    });
    connect(&multiverse_, &MultiversePresentation::homeRequested, this, [this] {
        multiverseHome_ = true; goToPage(0);
    });
    connect(&settings_, &SettingsController::trainerRequested, this, [this](int index) {
        trainerSettingsAction(index);
    });
    connect(&trainerSetup_, &TrainerSetupPresentation::changed, this, &ShellController::changed);
    connect(&trainerSetup_, &TrainerSetupPresentation::closeRequested, this, [this] {
        if (trainerChooserFromPower_) {
            service_.clear(); menuOpen_ = true; powerMenu_ = true; menuFocus_ = 2;
        } else service_ = "settings";
        emit changed();
    });
    connect(&trainerSetup_, &TrainerSetupPresentation::nameRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::SetupName; keyboard_.begin(trainerSetup_.live()?"Trainer name":"Sample Trainer name", initial, 24);
    });
    connect(&settings_, &SettingsController::deviceRequested, this, [this](int index) { device_.activate(index + 2); emit changed(); });
    connect(&settings_, &SettingsController::controllerRequested, this, [this] { service_ = "diagnostics"; diagnostics_.begin(); emit changed(); });
    connect(&device_, &DeviceController::changed, this, &ShellController::changed);
    connect(this, &ShellController::changed, this, [this] { device_.setMonitoring(menuOpen_ || service_ == "device" || service_ == "settings"); });
    connect(&device_, &DeviceController::closeRequested, this, [this] { service_ = "settings"; emit changed(); });
    connect(&device_, &DeviceController::messageRequested, this, &ShellController::showNotice);
    connect(&device_, &DeviceController::powerRequested, this, [this](const QString& mode) {
        mode_ = mode;
        notice_ = mode == "reboot" ? "Restart your handheld? Your Trainer data will be saved first."
                                   : "Turn off your handheld? Your Trainer data will be saved first.";
        emit changed();
    });
    connect(&center_, &SaveCenterController::changed, this, &ShellController::changed);
    connect(&center_, &SaveCenterController::closeRequested, this, [this]{service_.clear();menuOpen_=true;menuFocus_=2;emit changed();});
    connect(&center_, &SaveCenterController::searchRequested, this, [this](const QString& initial){textTarget_=TextTarget::CenterSearch;keyboard_.begin("Find an Adventure",initial,64);});
    connect(&center_, &SaveCenterController::messageRequested, this, &ShellController::showNotice);
    connect(&center_, &SaveCenterController::restored, this, [this](const QString& id){
        if(homeAdventureId_==id){homeResumeId_.clear();homeResumeSource_={};}
        emit changed();
    });
    hall_.editor()->setLibrary(&repo);
    if (!repo.editable()) hall_.enableSampleJourney();
    hall_.showJourney();
    connect(hall_.account(), &AchievementAccountController::textRequested, this,
        [this](const QString& title, const QString& initial, int limit, bool secret) {
            textTarget_ = TextTarget::AchievementAccount; keyboard_.begin(title, initial, limit, secret);
        });
    trainer_.configure(&repo, &dexReference, &dexProgress, &archive);
    connect(trainer_.picker(), &SpeciesPicker::searchRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::TrainerFavorite; keyboard_.begin("Find your favorite · name / number", initial, 48);
    });
    connect(hall_.editor(), &ArchiveEditor::textRequested, this, [this](const QString& title, const QString& initial, int limit) {
        textTarget_ = TextTarget::Archive; keyboard_.begin(title, initial, limit);
    });
    connect(&diagnostics_, &DiagnosticsController::closeRequested, this, [this] { service_ = "settings"; emit changed(); });
    connect(&diagnostics_, &DiagnosticsController::messageRequested, this, [this](const QString& text) {
        if (service_ != "diagnostics" || menuOpen_) { notice_ = text; emit changed(); }
    });
    connect(&libraryManager_, &LibraryManagementController::changed, this, &ShellController::changed);
    connect(&libraryManager_, &LibraryManagementController::saved, this, &ShellController::refreshLibrary);
    connect(&libraryManager_, &LibraryManagementController::messageRequested, this, [this](const QString& text) { notice_ = text; emit changed(); });
    connect(&libraryManager_, &LibraryManagementController::closeRequested, this, [this] { service_.clear(); if(centerFace())openCenter(); menuOpen_ = !libraryFromWorlds_; libraryFromWorlds_ = false; emit changed(); });
    connect(&worlds_, &WorldsController::setupRequested, this, [this](const QString& id) {
        libraryFromWorlds_ = true; service_ = "library"; libraryManager_.beginEdit(id); emit changed();
    });
    connect(&libraryManager_, &LibraryManagementController::textRequested, this, [this](const QString& title, const QString& initial, int limit) {
        textTarget_ = TextTarget::Library; keyboard_.begin(title, initial, limit);
    });
    connect(&settings_, &SettingsController::changed, this, &ShellController::changed);
    connect(&settings_, &SettingsController::quickAdjustment, &device_, &DeviceController::adjustQuick);
    connect(&settings_, &SettingsController::messageRequested, this, [this](const QString& message) {
        if (service_ != "settings" || menuOpen_) { notice_ = message; emit changed(); }
    });
    connect(&settings_, &SettingsController::closeRequested, this, [this] { service_.clear(); if(centerFace())openCenter(); menuOpen_ = true; emit changed(); });
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
    connect(&worlds_, &WorldsController::homeRequested, this, [this] { multiverseHome_ = false; goToPage(0); });
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
        if (target == TextTarget::SetupName) trainerSetup_.applyName(text);
        else if (target == TextTarget::TrainerName) trainer_.setDraftName(text);
        else if (target == TextTarget::PokedexSearch) pokedex_.applySearch(text);
        else if (target == TextTarget::WorldsSearch) worlds_.applySearch(text);
        else if (target == TextTarget::MultiverseSearch) multiverse_.applySearch(text);
        else if (target == TextTarget::Library) libraryManager_.applyText(text);
        else if (target == TextTarget::LibraryTools) libraryTools_.applyText(text);
        else if (target == TextTarget::Archive) hall_.editor()->applyText(text);
        else if (target == TextTarget::TrainerFavorite) trainer_.picker()->applySearch(text);
        else if (target == TextTarget::CenterSearch) center_.applySearch(text);
        else if (target == TextTarget::AchievementAccount) hall_.account()->applyText(text);
    });
    refreshContinue();
}
void ShellController::configureServices(FileCatalog* files, PreferencesRepository* preferences) {
    libraryTools_.setCatalog(files);
    libraryManager_.files()->setCatalog(files); settings_.setRepository(preferences);
    const auto records=repository_.registrations();
    settings_.setLegacyTrashAvailable(std::any_of(records.cbegin(),records.cend(),[](const auto& r){return r.removed && !r.trashPath.isEmpty();}));
}
void ShellController::refreshLibrary() {
    worlds_.refresh(); libraryManager_.refresh(); multiverse_.refresh();
    const auto records=repository_.registrations();
    settings_.setLegacyTrashAvailable(std::any_of(records.cbegin(),records.cend(),[](const auto& r){return r.removed && !r.trashPath.isEmpty();}));
    if (page_ == 3) trainer_.refreshOverview();
    const QString selected = drawerFocus_ < points_.size() ? points_[drawerFocus_].id : QString();
    refreshContinue();
    drawerFocus_ = 0;
    for (int i = 0; i < points_.size(); ++i) if (points_[i].id == selected) drawerFocus_ = i;
    if (centerFace() && !serviceOpen()) openCenter(); else refreshParty();
    emit changed();
}
QString ShellController::currentAdventureId() const {
    // An explicit choice remains authoritative even if its installation vanishes.
    // Never silently replace it with a different game's latest launch/save.
    return homeAdventureId_.isEmpty() ? repository_.home().activeAdventureId : homeAdventureId_;
}
bool ShellController::canHoldConfirm() const {
    if(page_!=1 || !repository_.editable() || menuOpen_ || !notice_.isEmpty() || !service_.isEmpty()
        || keyboard_.isOpen() || drawerOpen_ || libraryTools_.isOpen())return false;
    if(multiverseFace_ ? multiverse_.route()!="games" : worlds_.route()!="adventures")return false;
    const auto id=(multiverseFace_?multiverse_.detail():worlds_.detail()).value("id").toString();
    const auto record=repository_.registration(id);return record && !record->removed;
}
bool ShellController::canEditWorld() const {
    return page_==1 && !multiverseFace_ && repository_.editable() && settings_.worldEditing()
        && service_.isEmpty() && !menuOpen_ && notice_.isEmpty() && !keyboard_.isOpen()
        && !drawerOpen_ && !libraryTools_.isOpen() && !worlds_.region().value("id").toString().isEmpty();
}
bool ShellController::localModalOpen() {
    return libraryTools_.isOpen() || trainer_.editing() || (page_ == 2 && (centerFace_ ? center_.confirming() || party_.detailOpen()
        : pokedex_.zone() == "picker" || pokedex_.zone() == "art" || pokedex_.saving()))
        || (page_ == 4 && (hall_.editor()->isOpen() || hall_.account()->isOpen()));
}
bool ShellController::chooseAdventureAvailable() {
    return page_ != 1 && !serviceOpen() && !menuOpen_ && notice_.isEmpty()
        && !keyboard_.isOpen() && !localModalOpen();
}
bool ShellController::pairedNavigationAvailable() {
    if (page_ == 1) return !serviceOpen() && !menuOpen_ && notice_.isEmpty()
        && !keyboard_.isOpen() && !localModalOpen() && !drawerOpen_;
    return (page_ == 4 || page_ == 2) && chooseAdventureAvailable() && !drawerOpen_;
}
void ShellController::openCenter() {
    centerFace_ = true;
    center_.beginSelected(currentAdventureId());
    refreshParty();
}
void ShellController::refreshParty() {
    const auto adventure = homeAdventure();
    {
        const QSignalBlocker batch(&party_);
        party_.setAdventure(currentAdventureId(), adventure ? adventure->title : QString());
        party_.setProgress(progress_ ? progress_->adventureId() : QString(), progress_ ? progress_->snapshot() : GameProgress{});
    }
    emit party_.changed();
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
        if(const auto record=repository_.registration(point.adventureId);record && record->removed)continue;
        if (point.id.isEmpty() || ids.contains(point.id)) continue;
        ids.insert(point.id);
        points_.append({point.id, point.adventureId, point.savedAt, point, {}});
        represented.insert(point.adventureId);
    }
    for (const auto& session : repository_.recentSessions()) {
        const auto registration = repository_.registration(session.adventureId);
        if (registration && (registration->removed || registration->adventure.domain != "pokemon")) continue;
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
    if (libraryTools_.isOpen()) return libraryTools_.focusIndex();
    if (drawerOpen_) return page_ == 0 && multiverseHome_ ? multiverseDrawerFocus_ : drawerFocus_;
    if (service_ == "library") return libraryManager_.files()->isOpen() ? libraryManager_.files()->focusIndex() : libraryManager_.focusIndex();
    if (service_ == "trainer-settings") return hall_.account()->isOpen() ? hall_.account()->focusIndex() : trainerSettingsFocus_;
    if (service_ == "trainer-setup") return trainerSetup_.focusIndex();
    if (service_ == "settings") return hall_.account()->isOpen() ? hall_.account()->focusIndex() : trainer_.editing() ? trainer_.focusIndex() : settings_.focusIndex();
    if (service_ == "device") return device_.focusIndex();
    if (service_ == "diagnostics") return diagnostics_.focusIndex();
    if (service_ == "center") return center_.focusIndex();
    if (trainer_.editing()) return trainer_.focusIndex();
    if (page_ == 1) return multiverseFace_ ? multiverse_.focusIndex() : worlds_.focusIndex();
    if (page_ == 2) return centerFace_ ? (party_.section() == "saves" ? center_.focusIndex() : party_.focusIndex()) : pokedex_.focusIndex();
    if (page_ == 4) return hall_.focusIndex();
    return drawerOpen_ ? drawerFocus_ : 0;
}
QJsonObject ShellController::navigationState() const {
    const QStringList pages{"home", "worlds", "pokedex", "trainer", "hall"};
    return {{"version", 1}, {"page", pages[page_]},
            {"homeAdventure", homeAdventureId_}, {"homeResume", homeResumeId_}, {"pokedexFace", centerFace_ ? "center" : "pokedex"},
            {"homeResumeSource", homeResumeSource_.toJson()},
            {"multiverse",multiverse_.navigationState()},{"homeDomain",multiverseHome_?"multiverse":"pokemon"},
            {"worldsFace",multiverseFace_?"multiverse":"pokemon"},
            {"resume", drawerFocus_ < points_.size() ? points_[drawerFocus_].id : QString()},
            {"worlds", worlds_.navigationState()}, {"pokedex", pokedex_.navigationState()}, {"hall", hall_.navigationState()}};
}
void ShellController::restoreNavigation(const QJsonObject& state) {
    if (state["version"].toInt() != 1) return;
    const QStringList pages{"home", "worlds", "pokedex", "trainer", "hall"};
    centerFace_ = false;
    goToPage(std::max(0, int(pages.indexOf(state["page"].toString()))));
    homeAdventureId_ = state["homeAdventure"].toString(); homeResumeId_ = state["homeResume"].toString();
    homeResumeSource_ = ResumeSource::fromJson(state["homeResumeSource"].toObject());
    // Preserve the Adventure choice, but never restore a retired state target.
    if (repository_.editable()) { homeResumeId_.clear(); homeResumeSource_ = {}; }
    worlds_.restoreNavigation(state["worlds"].toObject());
    multiverse_.restoreNavigation(state["multiverse"].toObject());
    multiverseHome_=state["homeDomain"].toString()=="multiverse";
    multiverseFace_=state["worldsFace"].toString()=="multiverse";
    pokedex_.restoreNavigation(state["pokedex"].toObject());
    hall_.restoreNavigation(state["hall"].toObject());
    drawerFocus_ = 0;
    for (int i = 0; i < points_.size(); ++i) if (points_[i].id == state["resume"].toString()) drawerFocus_ = i;
    centerFace_ = state["pokedexFace"].toString() == "center";
    if (centerFace()) openCenter(); else refreshParty();
    emit changed();
}
std::optional<Adventure> ShellController::homeAdventure() const {
    const auto adventures = repository_.adventures();
    for (const auto& a : adventures) if (a.id == currentAdventureId() && !a.collectionOnly && a.domain == "pokemon") return a;
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
    const auto media = adventure ? repository_.exitMedia(adventure->id) : std::nullopt;
    QString action = "Explore Worlds", actionHint = "Worlds", milestone = snapshot.milestone;
    if (!adventure && !homeAdventureId_.isEmpty()) {
        title = "Selected Adventure is unavailable";
        milestone = "Your choice is kept";
    }
    std::optional<int> badges, caught;
    QVariantList badgeSlots;
    QString progressNote, badgeSet;
    std::optional<qint64> seconds;
    if (adventure) {
        title = adventure->title;
        for (const auto& w : repository_.worlds()) if (w.id == adventure->worldId) world = w.name;
        badges = adventure->badges; caught = adventure->caught;
        if (adventure->id == snapshot.activeAdventureId) {
            if (!badges) badges = snapshot.badges;
            if (!caught) caught = snapshot.caught;
        }
        if (progress_) {
            badges.reset(); caught.reset();
            if (progress_->adventureId() == adventure->id) {
                const auto observed = progress_->snapshot();
                progressNote = observed.message;
                if (observed.availability == ProgressAvailability::Available) {
                    badgeSet = observed.badgeSet;
                    caught = observed.caught;
                    badgeSlots = BadgeAssets::entries(badgeSet, observed.badgeMask);
                    if (observed.badgeMask) {
                        badges = std::popcount(static_cast<unsigned>(*observed.badgeMask) & 255u);

                    }
                }
            }
        }
        seconds = repository_.recordedSeconds(adventure->id);
        const auto caps = adapter_.capabilities(*adventure);
        const auto resumeStatus = homeResumeAvailability(*adventure);
        action = resumeStatus == ResumeAvailability::Exact ? "Resume Adventure" : caps.launch ? "Start Adventure" : "Set up Adventure";
        actionHint = resumeStatus == ResumeAvailability::Exact ? "Resume" : caps.launch ? "Play" : "Set up";
        if (repository_.editable()) {
            milestone = "Your selected Adventure";
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
            {"badgeSlots", badgeSlots}, {"progressNote", progressNote}, {"badgeSet", badgeSet},
            {"exitPreview", media ? "image://exit-media/" + media->sessionId : adventure ? repository_.artwork(adventure->id).value("cover").toString() : QString()},
            {"exitPreviewLabel", media ? "Last exit · " + media->capturedAt.toLocalTime().toString("dd MMM · HH:mm") : QString()},
            {"recordedTime", seconds ? recordedDuration(*seconds) : "—"}, {"milestone", milestone}};
}
QVariantList ShellController::resumePoints() const {
    if (page_ == 0 && multiverseHome_) return multiverse_.choices();
    QVariantList result;
    const auto adventures = repository_.adventures();
    const auto worlds = repository_.worlds();
    for (const auto& point : points_) {
        const auto media = repository_.exitMedia(point.adventureId);
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
            {"location", location}, {"summary", summary}, {"previewLabel", media ? "Last exit · " + media->capturedAt.toLocalTime().toString("dd MMM · HH:mm") : point.resumePoint ? resumeLabel(status) : "Recent Adventure"},
            {"time", point.recordedAt.toLocalTime().toString("dd MMM · HH:mm")},
            {"preview", media ? "image://exit-media/" + media->sessionId : point.resumePoint && !point.resumePoint->previewKey.isEmpty() ? "image://moments/" + point.resumePoint->previewKey : repository_.artwork(point.adventureId).value("cover").toString()}});
    }
    return result;
}
QStringList ShellController::menuItems() const {
    if (powerMenu_) {
        QStringList items{"Power off", "Restart", trainerSetup_.live()?"Switch Player":"Switch Player (coming later)", "Cancel"};
        if (!platform_.dedicatedSession()) items.append(platform_.canSwitchSession() ? "Enter TrainerOS Mode" : "Exit Development App");
        return items;
    }
    // Stable action IDs: slot 1 retired when Controller moved into Settings.
    return {"Settings", "", "Pokémon Center", "",
            "Desktop / Maintenance Mode", "Steam Gaming Mode", "Power", "Volume", "Screen brightness"};
}
void ShellController::openTrainers() {
    trainerChooserFromPower_ = powerMenu_;
    goToPage(page_);trainerSetup_.begin();service_="trainer-setup";emit changed();
}
void ShellController::goToPage(int page) {
    if(libraryTools_.busy())return;
    // Closing transient controllers emits their local notifications. Publish
    // only the completed shell transition, not every intermediate close, so
    // hidden Home/drawer bindings do not rebuild the library repeatedly.
    QSignalBlocker transition(this);
    libraryTools_.close();
    keyboard_.cancel();
    textTarget_ = TextTarget::None;
    pokedex_.cancelTransient();
    hall_.editor()->cancel();
    hall_.account()->close();
    trainer_.cancel();
    trainerSetup_.close();
    center_.close();
    libraryManager_.close(); service_.clear();
    page_ = std::clamp(page, 0, 4); // No wrapping until physical-device testing.
    if (page_ == 1) repository_.refreshContentAvailability();
    if (centerFace()) openCenter(); else refreshParty();
    if (page_ == 3) trainer_.refreshOverview();
    drawerOpen_ = false;
    menuOpen_ = false;
    if (powerMenu_) menuFocus_ = 6;
    powerMenu_ = false;
    notice_.clear();
    mode_.clear();
    transition.unblock();
    emit changed();
}
void ShellController::activate(int index, const QString& area) {
    if(libraryTools_.busy())return;
    if(area=="world-edit" && canEditWorld()){libraryTools_.beginWorld(worlds_.region().value("id").toString(),true);return;}
    if (area == "continue") { dispatch(Action::ToggleContinue); return; }
    if (!notice_.isEmpty()) { confirm(); emit changed(); return; }
    if (menuOpen_) menuFocus_ = std::clamp(index, 0, int(menuItems().size()) - 1);
    else if (keyboard_.isOpen()) { keyboard_.activate(index); return; }
    else if (libraryTools_.isOpen()) {libraryTools_.activate(index);return;}
    else if (drawerOpen_) {
        auto& focus = page_ == 0 && multiverseHome_ ? multiverseDrawerFocus_ : drawerFocus_;
        focus = std::clamp(index, 0, std::max(0, int(resumePoints().size()) - 1));
    }
    else if (service_ == "library") { libraryManager_.activate(index, area); return; }
    else if (service_ == "trainer-settings") { trainerSettingsAction(index); return; }
    else if (service_ == "trainer-setup") { trainerSetup_.activate(index); return; }
    else if (service_ == "settings") { if(hall_.account()->isOpen()) { hall_.account()->activate(index); return; } if(trainer_.editing()) { trainer_.activate(index); return; } if(settings_.controlsFocused()) settings_.activateRow(index); else settings_.selectCategory(index,true); return; }
    else if (service_ == "device") { device_.activate(index); return; }
    else if (service_ == "diagnostics") { diagnostics_.activate(index); return; }
    else if (service_ == "center") { center_.activate(index); return; }
    else if (trainer_.editing()) { trainer_.activate(index); return; }
    else if (page_ == 1) {
        if (multiverseFace_) { multiverse_.activate(index); return; }
        if (area == "worlds-search") worlds_.dispatch(Action::Secondary);
        else if (area == "worlds-filter") worlds_.dispatch(Action::ToggleContinue);
        else worlds_.activate(index);
        return;
    }
    else if (page_ == 2) {
        if (centerFace_) {
            if (area == "party-activities") party_.openActivities();
            else if (party_.section() == "saves") center_.activate(index); else party_.activate(index);
            return;
        }
        if (area.isEmpty()) pokedex_.activate(index);
        else pokedex_.activateControl(area, index);
        return;
    }
    else if (page_ == 4) {
        if (area.isEmpty()) hall_.activate(index);
        else hall_.activateControl(area, index);
        return;
    }
    confirm();
    emit changed();
}
void ShellController::trainerSettingsAction(int index) {
    if (hall_.account()->isOpen()) { hall_.account()->activate(index); return; }
    if (index < 0 || index > 4) return;
    trainerSettingsFocus_ = index;
    if (index == 0) { trainer_.beginEdit(); }
    else if (index == 1) {
        if (hall_.account()->available()) hall_.account()->begin();
        else showNotice("RetroAchievements account management is unavailable right now.");
    } else if (index == 2) {
        if (sampleLibrary()) { trainerChooserFromPower_ = false; trainerSetup_.begin(); service_ = "trainer-setup"; }
        else if (trainerSetup_.live()) emit trainersRequested();
        else showNotice("Trainer selection is unavailable until your data is open.");
    } else if (index==3 && service_=="trainer-settings") service_="settings";
    else if (index==3 || index==4) emit pinRequested(index==4);
    emit changed();
}
void ShellController::confirm() {
    if (!notice_.isEmpty()) {
        const auto requested = mode_; mode_.clear(); notice_.clear();
        if (requested == "development-exit") emit exitRequested();
        else if (!requested.isEmpty()) emit modeRequested(requested);
        return;
    }
    if (menuOpen_) {
        if (powerMenu_) {
            if (menuFocus_ == 3) { powerMenu_ = false; menuFocus_ = 6; return; }
            if (menuFocus_ == 2) { if(trainerSetup_.live())emit trainersRequested();else notice_ = "Switch Player will be available when separate Trainer profiles are ready."; return; }
            if (menuFocus_ == 4) {
                if (platform_.canSwitchSession()) { mode_ = "traineros"; notice_ = "Enter the TrainerOS session?"; }
                else { mode_ = "development-exit"; notice_ = "Close the development app?"; }
                return;
            }
            device_.requestPower(menuFocus_ == 1);
            return;
        }
        if (menuFocus_ >= 7) { device_.adjustQuick(menuFocus_ - 7, Action::Confirm); return; }
        if (menuFocus_ == 6) { powerMenu_ = true; menuFocus_ = 3; return; }
        if (menuFocus_ >= 4 && platform_.canSwitchSession()) {
            mode_ = menuFocus_ == 5 ? "steam" : "desktop";
            notice_ = mode_ == "steam" ? "Open Steam Gaming Mode? Your Trainer data will be saved first."
                : mode_ == "traineros" ? "Enter the dedicated TrainerOS session? Your Trainer data will be saved first."
                                      : "Open Desktop / Maintenance Mode? Your Trainer data will be saved first.";
            return;
        }

        if(menuFocus_==2) {
            centerFace_ = false; goToPage(2); openCenter(); return;
        }
        if (menuFocus_ == 0 || menuFocus_ == 3) {
            hall_.account()->close();
            trainerSetup_.close();
            keyboard_.cancel(); textTarget_ = TextTarget::None; trainer_.cancel();
            libraryManager_.close(); libraryTools_.close(); menuOpen_ = false; drawerOpen_ = false;
            center_.close();
            service_ = menuFocus_ == 0 ? "settings" : "library";
            if (service_ == "settings") settings_.begin();
            else { libraryFromWorlds_ = false; libraryManager_.begin(worlds_.region()["id"].toString()); }
            return;
        }
        notice_ = menuFocus_ >= 4 ? platform_.sessionStatus()
            : "This service is not available in the first prototype yet.";
        return;
    }
    if (drawerOpen_) {
        if (page_ == 0 && multiverseHome_) {
            const auto choices = multiverse_.choices();
            if (multiverseDrawerFocus_ >= 0 && multiverseDrawerFocus_ < choices.size())
                multiverse_.select(choices[multiverseDrawerFocus_].toMap()["id"].toString());
            drawerOpen_ = false; return;
        }
        if (points_.isEmpty()) { drawerOpen_ = false; return; }
        const auto& point = points_.at(drawerFocus_);
        for (const auto& adventure : repository_.adventures()) if (adventure.id == point.adventureId) {
            homeAdventureId_ = adventure.id; homeResumeId_ = point.resumePoint ? point.id : QString();
            homeResumeSource_ = point.resumePoint ? point.resumePoint->source : ResumeSource{};
            drawerOpen_ = false;
            if (centerFace()) openCenter(); else refreshParty();
            return;
        }
        notice_ = "This Adventure is unavailable. Its history has been kept.";
    } else if (page_ == 0) {
        if (multiverseHome_) {
            if (multiverse_.selected().isEmpty()) { multiverseFace_ = true; goToPage(1); }
            else if(multiverse_.sample()) notice_ = "Development preview only. No game was launched.";
            else {
                const auto record=repository_.registration(multiverse_.selected()["id"].toString());
                if(!record || record->adventure.domain!="multiverse")notice_="This Adventure is unavailable. Choose another Adventure.";
                else if(record->contentAvailable && adapter_.capabilities(record->adventure).launch) {
                    emit homeLaunchPressed();const auto result=adapter_.launch(record->adventure);
                    if(!result.inProgress)notice_=result.message;
                } else {
                    libraryFromWorlds_=true;service_="library";libraryManager_.beginEdit(record->adventure.id);
                }
            }
            return;
        }
        const auto adventure = homeAdventure();
        if (!adventure) { multiverseFace_ = false; goToPage(1); return; }
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
    if(libraryTools_.busy())return;
    if(action==Action::ContextMenu && canHoldConfirm()) {
        libraryTools_.beginGame((multiverseFace_?multiverse_.detail():worlds_.detail()).value("id").toString());return;
    }
    if(action==Action::LocalAction && canEditWorld()) {libraryTools_.beginWorld(worlds_.region().value("id").toString(),true);return;}
    if (action == Action::Home) { goToPage(0); return; }
    if (action == Action::PreviousPage || action == Action::NextPage) {
        goToPage(page_ + (action == Action::NextPage ? 1 : -1));
        return;
    }
    // Global section/system actions outrank the active local layer. Start overlays
    // the keyboard without changing its draft or key focus; Back unwinds it first.
    if (action == Action::SystemMenu) {
        if (notice_.isEmpty()) { menuOpen_ = !menuOpen_; if (!menuOpen_ && powerMenu_) { powerMenu_ = false; menuFocus_ = 6; } }
        if(menuOpen_ && !powerMenu_ && menuItems().value(menuFocus_).isEmpty())menuFocus_=0;
        emit changed();
        return;
    }
    if (action == Action::PreviousFace || action == Action::NextFace) {
        if (pairedNavigationAvailable()) {
            if (page_ == 1) { multiverseFace_ = !multiverseFace_; if(multiverseFace_)repository_.refreshContentAvailability(); }
            else if (page_ == 4) hall_.switchFace();
            else if (centerFace_) { center_.close(); centerFace_ = false; }
            else openCenter();
            emit changed();
        }
        return;
    }
    if (action == Action::ToggleContinue && chooseAdventureAvailable()) {
        if(!drawerOpen_ && page_==0 && multiverseHome_)repository_.refreshContentAvailability();
        drawerOpen_ = !drawerOpen_; emit changed(); return;
    }
    if (notice_.isEmpty() && !menuOpen_) {
        if (keyboard_.isOpen()) { keyboard_.dispatch(action); return; }
        if (libraryTools_.isOpen()) {libraryTools_.dispatch(action);return;}
        if (drawerOpen_) {
            if (action == Action::Back) drawerOpen_ = false;
            else if (action == Action::Confirm) confirm();
            else if (action == Action::Left || action == Action::Right) {
                auto& focus = page_ == 0 && multiverseHome_ ? multiverseDrawerFocus_ : drawerFocus_;
                focus = std::clamp(focus + (action == Action::Right ? 1 : -1), 0, std::max(0, int(resumePoints().size()) - 1));
            }
            emit changed(); return;
        }
        if (service_ == "library") { libraryManager_.dispatch(action); return; }
        if (service_ == "trainer-setup") { trainerSetup_.dispatch(action); return; }
        if (service_ == "trainer-settings") {
            if (hall_.account()->isOpen()) hall_.account()->dispatch(action);
            else if (action == Action::Back) service_ = "settings";
            else if (action == Action::Confirm) { trainerSettingsAction(trainerSettingsFocus_); return; }
            else if (action == Action::Up) trainerSettingsFocus_ = std::max(0, trainerSettingsFocus_ - 1);
            else if (action == Action::Down) trainerSettingsFocus_ = std::min(3, trainerSettingsFocus_ + 1);
            emit changed(); return;
        }
        if (service_ == "settings") { if(hall_.account()->isOpen()) hall_.account()->dispatch(action); else if(trainer_.editing()) trainer_.dispatch(action,true); else settings_.dispatch(action); return; }
        if (service_ == "device") { device_.dispatch(action); return; }
        if (service_ == "diagnostics") { diagnostics_.dispatch(action); return; }
        if (service_ == "center") { center_.dispatch(action); return; }
        if (trainer_.editing()) { trainer_.dispatch(action); return; }
        if (page_ == 0 && action == Action::Secondary) { multiverseHome_ = !multiverseHome_; emit changed(); return; }
        if (page_ == 1) {
            if (multiverseFace_) multiverse_.dispatch(action); else worlds_.dispatch(action);
            return;
        }
        if (page_ == 2) {
            if (centerFace_) {
                if (party_.section() != "saves") party_.dispatch(action);
                else if (action == Action::Back && !center_.confirming() && !center_.busy()) party_.returnFromSaves();
                else center_.dispatch(action);
            }
            else pokedex_.dispatch(action);
            return;
        }
        if (page_ == 4) { hall_.dispatch(action == Action::LocalAction && !localModalOpen() ? Action::ToggleContinue : action); return; }
    }
    if (menuOpen_ && !powerMenu_ && notice_.isEmpty() && action == Action::Secondary) {
        if (menuFocus_ >= 7) menuFocus_ = menuServiceFocus_;
        else { menuServiceFocus_ = menuFocus_; menuFocus_ = 7; }
        emit changed(); return;
    }
    if (menuOpen_ && !powerMenu_ && notice_.isEmpty() && menuFocus_ >= 7
            && (action == Action::Left || action == Action::Right)) {
        device_.adjustQuick(menuFocus_ - 7, action); emit changed(); return;
    }
    if (action == Action::Back) {
        if (!notice_.isEmpty()) { notice_.clear(); mode_.clear(); }
        else if (menuOpen_ && powerMenu_) { powerMenu_ = false; menuFocus_ = 6; }
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
        if(menuOpen_ && !powerMenu_) {
            const QList<int> order{7,8,0,2,4,5,6};
            *focus=order[std::clamp(int(order.indexOf(*focus))+delta,0,int(order.size())-1)];
        } else *focus = std::clamp(*focus + delta, 0, std::max(0, count - 1));
    }
    emit changed();
}
}
