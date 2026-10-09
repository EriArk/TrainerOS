#include "core/PerformanceTrace.h"
#include "ShellController.h"
#include "core/model/SeriesCatalog.h"
#include "features/home/PlayHistoryController.h"
#include "ResumePresentation.h"
#include "features/home/BadgeAssets.h"
#include <QSet>
#include <QTimer>
#include <QSignalBlocker>
#include <algorithm>
#include <bit>
#include "core/model/Experience.h"
#include "core/model/NavigationVersion.h"

namespace trainer {
void ShellController::showAchievements(const QString& title,const QStringList& names) {
    if(names.isEmpty())return;
    achievementToast_=title+"\n"+(names.size()==1?names.first():QString("%1 achievements unlocked").arg(names.size()));
    const auto generation=++achievementToastGeneration_;emit changed();
    QTimer::singleShot(7000,this,[this,generation]{if(generation==achievementToastGeneration_){achievementToast_.clear();emit changed();}});
}
void ShellController::configureProgress(GameProgressProvider* provider) {
    if (progress_) disconnect(progress_, nullptr, this, nullptr);
    progress_ = provider;
    libraryTools_.completionQuery=[this](const AdventureRegistration& requested,QObject* receiver,std::function<void(AdventureCompletion)> done){
        const auto record=repository_.registration(requested.adventure.id);
        if(!progress_||!record||record->revision!=requested.revision){done({});return;}
        progress_->inspectCompletion(*record,receiver,std::move(done));
    };
    libraryTools_.capabilityQuery=[this](const AdventureRegistration& requested,QObject* receiver,std::function<void(QStringList)> done){
        const auto record=repository_.registration(requested.adventure.id);
        if(!record || record->revision!=requested.revision){done({"Adventure changed · Reopen Properties"});return;}
        const auto caps=adapter_.capabilities(record->adventure);
        QStringList rows{"Launch · "+QString(record->contentAvailable?(caps.launch?"Ready":"Needs setup"):"File unavailable")};
        if(progress_)progress_->inspectCapabilities(*record,receiver,[this,done,rows](QStringList semantic){
            if(center_.readOnly()) semantic.prepend("Save changes: Read-only");
            done(rows+semantic);
        });
        else done(rows);
    };
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
      libraryManager_(repo, nullptr, this), libraryTools_(repo,this), scraper_(repo,this), settings_(this), device_(this), diagnostics_(this), center_(repo,this), party_(!repo.editable(),this) {
    settings_.setScraper(&scraper_);
    connect(&settings_,&SettingsController::collectionsRequested,this,[this]{goToPage(1);});
    connect(collectionManager(),&GameCollections::changed,this,&ShellController::changed);
    connect(collectionManager(),&GameCollections::definitionsChanged,this,[this]{
        collectionFocus_=std::clamp(collectionFocus_,0,std::max(0,int(collections().size())-1));
        if(!collectionManager()->known(worldCollection_)){worldCollection_="multiverse";collectionsRoot_=true;}
        emit changed();
    });
    connect(collectionManager(),&GameCollections::textRequested,this,[this](const QString& title,const QString& initial,int limit){textTarget_=TextTarget::Collections;keyboard_.begin(title,initial,limit);});
    connect(&libraryTools_,&LibraryToolsController::collectionsRequested,this,[this](const QString& id){libraryTools_.close();collectionManager()->beginMembership(id);});
    libraryTools_.editGuard=[this]{return scraper_.busy()?QString("Finish or cancel downloads before changing library files or names."):QString();};
    connect(&downloads_,&DownloadsController::changed,this,&ShellController::changed);
    connect(&scraper_,&ScrapeController::changed,this,[this]{downloads_.publish("screenscraper",scraper_.downloadTasks());});
    connect(&scraper_,&ScrapeController::jobStarted,this,[this]{scraper_.hide();downloads_.begin();});
    connect(&scraper_,&ScrapeController::systemSelectionClosed,this,[this]{menuOpen_=true;powerMenu_=false;menuFocus_=13;emit changed();});
    connect(&downloads_,&DownloadsController::commandRequested,this,[this](const QString& provider,const QString& task,const QString& command){
        if(provider=="screenscraper"){if(command=="open")downloads_.close();scraper_.downloadCommand(task,command);}
    });
    connect(&scraper_,&ScrapeController::changed,this,&ShellController::changed);
    connect(&scraper_,&ScrapeController::changed,&settings_,&SettingsController::changed);
    connect(&scraper_,&ScrapeController::settingsBackRequested,this,[this]{settings_.selectCategory(14,false);});
    connect(&scraper_,&ScrapeController::textRequested,this,[this](QString title,QString initial,bool secret){textTarget_=TextTarget::Scraper;keyboard_.begin(title,initial,256,secret,"Save");});
    connect(&libraryTools_,&LibraryToolsController::scrapeRequested,this,[this](const QString& game,const QString& world){
        if(scraper_.busy())downloads_.begin();else scraper_.begin(game,world);
    });
    connect(&libraryTools_,&LibraryToolsController::reviewRequested,&social_,&SocialController::reviewCommand);
    connect(&social_,&SocialController::reviewsChanged,&libraryTools_,&LibraryToolsController::receiveReviews);
    connect(&network_, &NetworkController::changed,this,&ShellController::changed);
    connect(settings_.clock(), &ClockController::changed, this, &ShellController::changed);
    settings_.communication()->configure(&social_);
    connect(settings_.communication(),&CommunicationSettings::changed,this,[this]{if(service_=="settings" && settings_.category()==13)emit changed();});
    connect(settings_.communication(),&CommunicationSettings::backRequested,this,[this]{settings_.selectCategory(13,false);});
    connect(this,&ShellController::changed,this,[this]{if(service_!="settings" || settings_.category()!=13)settings_.communication()->leave();});
    connect(settings_.communication(),&CommunicationSettings::textRequested,this,[this](QString title,QString text,int limit){textTarget_=TextTarget::Communication;keyboard_.begin(title,text,limit,false,"Save",true);});
    connect(settings_.clock(), &ClockController::backRequested, this, [this]{ if(service_=="settings")settings_.selectCategory(11,false); });
    connect(this, &ShellController::changed, this, [this]{ if(service_!="settings" || settings_.category()!=11)settings_.clock()->leave(); });
    connect(settings_.storage(), &LibraryStorageController::changed, &settings_, &SettingsController::changed);
    connect(settings_.storage(), &LibraryStorageController::changed, this, &ShellController::changed);
    connect(this,&ShellController::changed,this,[this]{network_.setActive(onboardingConnections_ || (service_=="settings" && settings_.category()==10));});
    connect(&settings_,&SettingsController::changed,this,[this]{network_.setActive(onboardingConnections_ || (service_=="settings" && settings_.category()==10));});
    connect(&network_,&NetworkController::backRequested,this,[this]{settings_.selectCategory(10,false);});
    connect(&network_,&NetworkController::radioRequested,this,[this](int index){device_.adjustQuick(index,Action::Confirm);});
    connect(&network_,&NetworkController::textRequested,this,[this](const QString& title,int limit,bool secret){
        textTarget_=TextTarget::Network;keyboard_.begin(title,{},limit,secret);
    });
    connect(&network_,&NetworkController::closeKeyboardRequested,this,[this]{
        if(textTarget_==TextTarget::Network){textTarget_=TextTarget::None;keyboard_.cancel();}
    });
    connect(&libraryTools_, &LibraryToolsController::changed,this,&ShellController::changed);
    connect(&libraryTools_, &LibraryToolsController::saved,this,&ShellController::refreshLibrary);
    connect(&libraryTools_, &LibraryToolsController::textRequested,this,[this](const QString& title,const QString& initial,int limit){
        textTarget_=TextTarget::LibraryTools;keyboard_.begin(title,initial,limit);
    });
    connect(&settings_, &SettingsController::trashRequested,this,[this]{libraryTools_.beginTrash();});
    connect(&party_, &PartyPresentation::changed, this, [this] {
        if(textTarget_==TextTarget::BoxName && party_.moveStage()!=QStringLiteral("name-edit")) {
            textTarget_=TextTarget::None; keyboard_.cancel();
        }
        emit changed();
    });
    connect(&party_, &PartyPresentation::boxNameRequested,this,[this](const QString& name,int limit){
        textTarget_=TextTarget::BoxName;keyboard_.begin("Box name",name,limit);
    });
    connect(&party_, &PartyPresentation::healingRequested, this, [this]{centerRoute_="clinic";showPokemonFace("center");});
    connect(&party_, &PartyPresentation::backupsRequested, this, [this]{centerRoute_="backups";showPokemonFace("center");});
    connect(party_.activities(), &CenterActivities::shopsRequested, this, [this]{showPokemonFace("shops");});
    connect(party_.activities()->link(),&LinkController::closeRequested,this,[this]{centerRoute_="clinic";showPokemonFace("center");});
    connect(party_.activities()->link(),&LinkController::workspaceRequested,this,[this]{
        // Both Trainers explicitly accepted this activity. Enter its one shared
        // workspace even though the activity now owns the navigation gate.
        page_=2;menuOpen_=drawerOpen_=false;service_.clear();
        centerRoute_="link";showPokemonFace("center");emit changed();
    });
    settings_.configureNearby(party_.activities()->link());
    social_.setLink(party_.activities()->link());
    connect(&social_,&SocialController::conversationsRequested,this,[this]{
        if(navigationLocked())return;
        socialFace_="chats";social_.setFace(socialFace_);goToPage(4);emit changed();
    });
    connect(&social_,&SocialController::communicationSettingsRequested,this,[this]{
        if(navigationLocked())return;
        service_="settings";menuOpen_=drawerOpen_=false;settings_.begin();settings_.selectCategory(13);emit changed();
    });
    connect(&center_, &SaveCenterController::changed, this, [this] {
        if (center_.confirming()) party_.openSaves();
    });
    connect(&multiverse_, &MultiversePresentation::changed, this, &ShellController::changed);
    connect(&multiverse_, &MultiversePresentation::searchRequested, this, [this](const QString& text) {
        textTarget_ = TextTarget::MultiverseSearch; keyboard_.begin("Multiverse · find a title", text, 48);
    });
    connect(&launchPreparation_, &LaunchPreparation::changed, this, &ShellController::changed);
    connect(&launchPreparation_, &LaunchPreparation::libraryChanged, this, &ShellController::refreshLibrary);
    connect(&launchPreparation_, &LaunchPreparation::messageRequested, this, &ShellController::showNotice);
    const auto openGame=[this](const QString& id){launchPreparation_.launch(id);};
    connect(&multiverse_, &MultiversePresentation::setupRequested, this, openGame);
    connect(&worlds_, &WorldsController::setupRequested, this, openGame);
    connect(&multiverse_, &MultiversePresentation::messageRequested, this, &ShellController::showNotice);
    connect(&multiverse_, &MultiversePresentation::homeRequested, this, [this] {
        homeAdventureId_=multiverse_.selected().value("id").toString();homeResumeId_.clear();homeResumeSource_={};refreshParty();goToPage(0);
    });
    connect(&settings_, &SettingsController::trainerRequested, this, [this](int index) {
        trainerSettingsAction(index);
    });
    connect(&trainerSetup_, &TrainerSetupPresentation::changed, this, &ShellController::changed);
    connect(&trainerSetup_, &TrainerSetupPresentation::closeRequested, this, [this] {
        if (trainerChooserFromPower_) {
            service_.clear(); menuOpen_ = true; powerMenu_ = false; menuFocus_ = 2;
        } else service_ = "settings";
        emit changed();
    });
    connect(&trainerSetup_, &TrainerSetupPresentation::nameRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::SetupName; keyboard_.begin(trainerSetup_.live()?"Trainer name":"Sample Trainer name", initial, 24);
    });
    connect(&settings_, &SettingsController::deviceRequested, this, [this](int index) { device_.activate(index + 2); emit changed(); });
    connect(&settings_, &SettingsController::controllerRequested, this, [this] { service_ = "diagnostics"; diagnostics_.begin(); emit changed(); });
    connect(&device_, &DeviceController::changed, this, &ShellController::changed);
    connect(this, &ShellController::changed, this, [this] { device_.setMonitoring(onboardingConnections_ || menuOpen_ || service_ == "device" || service_ == "settings"); });
    connect(&device_, &DeviceController::closeRequested, this, [this] { service_ = "settings"; emit changed(); });
    connect(&device_, &DeviceController::messageRequested, this, &ShellController::showNotice);
    connect(&device_, &DeviceController::powerRequested, this, [this](const QString& mode) {
        if(scraper_.busy()){showNotice("Finish or cancel downloads before leaving this system session.");return;}
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
    connect(&libraryManager_, &LibraryManagementController::closeRequested, this, [this] { service_.clear(); if(centerFace())showPokemonFace(pokemonFace_); menuOpen_ = !libraryFromWorlds_; libraryFromWorlds_ = false; emit changed(); });
    connect(&libraryManager_, &LibraryManagementController::textRequested, this, [this](const QString& title, const QString& initial, int limit) {
        textTarget_ = TextTarget::Library; keyboard_.begin(title, initial, limit);
    });
    connect(&settings_, &SettingsController::changed, this, &ShellController::changed);
    connect(&settings_, &SettingsController::quickAdjustment, &device_, &DeviceController::adjustQuick);
    connect(&settings_, &SettingsController::messageRequested, this, [this](const QString& message) {
        if (service_ != "settings" || menuOpen_) { notice_ = message; emit changed(); }
    });
    connect(&settings_, &SettingsController::closeRequested, this, [this] { service_.clear(); menuOpen_ = false; emit changed(); });
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
        keyboard_.begin("Field Guide · name or number", initial, 32);
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
    connect(&worlds_, &WorldsController::homeRequested, this, [this] { homeCollection_="pokemon"; multiverseHome_ = false; goToPage(0); });
    connect(&keyboard_, &TextEntryController::changed, this, &ShellController::changed);
    connect(&trainer_, &TrainerController::changed, this, &ShellController::changed);
    connect(&trainer_, &TrainerController::messageRequested, this, [this](const QString& message) {
        notice_ = message; emit changed();
    });
    connect(&trainer_, &TrainerController::nameRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::TrainerName;
        keyboard_.begin("Trainer name", initial, TrainerController::NameLimit);
    });
    connect(&social_, &SocialController::changed, this, [this]{
        if(homeMenuOpen_)homeMenuSelection_=homeMenuActions().value(homeMenuFocus()).toMap()["id"].toString();
        if(page_==4||homeMenuOpen_)emit changed();
    });
    connect(&social_, &SocialController::textRequested, this, [this](QString title, QString text, int limit) {
        textTarget_ = TextTarget::Social; keyboard_.begin(title,text,limit,false,social_.textSubmitLabel(),social_.textAllowsEmoji());
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
        else if (target == TextTarget::Collections) collectionManager()->applyText(text);
        else if (target == TextTarget::Archive) hall_.editor()->applyText(text);
        else if (target == TextTarget::TrainerFavorite) trainer_.picker()->applySearch(text);
        else if (target == TextTarget::CenterSearch) center_.applySearch(text);
        else if(target==TextTarget::ShopSearch)center_.applyShopSearch(text);
        else if(target==TextTarget::BoxName)party_.applyBoxName(text);
        else if (target == TextTarget::AchievementAccount) hall_.account()->applyText(text);
        else if (target == TextTarget::Network) network_.applyText(text);
        else if (target == TextTarget::Social) social_.applyText(text);
        else if (target == TextTarget::Scraper) scraper_.applyText(text);
        else if (target == TextTarget::Communication) settings_.communication()->applyText(text);
    });
    connect(&center_, &SaveCenterController::shopSearchRequested,this,[this](const QString& text){textTarget_=TextTarget::ShopSearch;keyboard_.begin("Find goods or shops",text,64);});
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
    downloads_.publish("screenscraper",scraper_.downloadTasks());
    const auto records=repository_.registrations();
    settings_.setLegacyTrashAvailable(std::any_of(records.cbegin(),records.cend(),[](const auto& r){return r.removed && !r.trashPath.isEmpty();}));
    if (page_ == 3 && trainerProfile_) trainer_.refreshOverview();
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
    if(!homeAdventureId_.isEmpty())return homeAdventureId_;
    for(const auto& session:repository_.recentSessions()) {
        const auto record=repository_.registration(session.adventureId);
        if(record && !record->removed)return session.adventureId;
    }
    return repository_.home().activeAdventureId;
}
bool ShellController::canHoldConfirm() const {
    if(page_!=1 || !repository_.editable() || homeMenuOpen_ || menuOpen_ || !notice_.isEmpty() || !service_.isEmpty()
        || keyboard_.isOpen() || drawerOpen_ || libraryTools_.isOpen() || multiverse_.collectionManager()->isOpen())return false;
    if(collectionsRoot_)return false;
    if(multiverseFace_ ? multiverse_.route()!="games" : worlds_.route()!="adventures")return false;
    const auto id=(multiverseFace_?multiverse_.detail():worlds_.detail()).value("id").toString();
    const auto record=repository_.registration(id);return record && !record->removed;
}
bool ShellController::canEditWorld() const {
    return page_==1 && !collectionsRoot_ && !multiverseFace_ && repository_.editable() && settings_.worldEditing()
        && service_.isEmpty() && !menuOpen_ && notice_.isEmpty() && !keyboard_.isOpen()
        && !drawerOpen_ && !libraryTools_.isOpen() && !worlds_.region().value("id").toString().isEmpty();
}
bool ShellController::localModalOpen() {
    return collectionManager()->isOpen() || downloads_.isOpen() || scraper_.isOpen() || (page_==4 && !social_.menu().isEmpty()) || libraryTools_.isOpen() || trainer_.editing() || (page_ == 2 && (centerFace() ? center_.confirming() || center_.writing() || (center_.shopsOpen() && center_.shopModal()) || party_.detailOpen() || party_.moveOpen()
        : pokedex_.zone() == "picker" || pokedex_.zone() == "art" || pokedex_.saving()))
        || (trainerHistoryFace() && (hall_.editor()->isOpen() || hall_.account()->isOpen()));
}
bool ShellController::chooseAdventureAvailable() {
    return !homeMenuOpen_ && page_ != 1 && page_ != 4 && !(page_==2 && pokemonFace_=="shops") && !serviceOpen() && !menuOpen_ && notice_.isEmpty()
        && !keyboard_.isOpen() && !localModalOpen() && !party_.activities()->practice()->running() && !party_.activities()->link()->active();
}
bool ShellController::navigationLocked(bool primaryRecovery) const {
    const auto* link=party_.activities()->link();
    // A disconnected durable exchange must not strand both peers away from
    // Social. Only primary browsing relaxes this gate; runtime/save operations,
    // owner changes and Adventure selection keep their existing protection.
    return (link->navigationBlocked() && !(primaryRecovery && link->canBrowseForRecovery())) || launchPreparation_.busy() || settings_.clock()->busy() || settings_.storage()->busy() || party_.moveOpen() || libraryTools_.busy() || center_.writing() || center_.confirming()
        || (center_.shopsOpen() && center_.shopModal());
}
bool ShellController::pairedNavigationAvailable() {
    return !homeMenuOpen_ && !serviceOpen() && !menuOpen_ && notice_.isEmpty()
        && !keyboard_.isOpen() && !localModalOpen() && !drawerOpen_;
}
QStringList ShellController::faceNames() const {
    if(page_==1) { QStringList names;for(const auto& row:collections())names.append(row.toMap()["name"].toString());return names; }
    if(page_==2)return {"Guide","Party","Boxes","Center","Playroom","Shops"};
    if(page_==3)return {"Profile","Journey","Hall","RA"};
    if(page_==4)return {"Messages","Communities","Discover"};
    return {};
}
int ShellController::faceIndex() const {
    if(page_==1 && collectionsRoot_)return collectionFocus_;
    if(page_==0 || page_==1) {
        const auto id=page_==0?homeCollection_:worldCollection_;const auto list=collections();
        for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==id)return i;
        return 0;
    }
    if(page_==2)return QStringList{"dex","party","boxes","center","playroom","shops"}.indexOf(pokemonFace_);
    if(page_==3)return trainerProfile_ ? 0 : hall_.faceIndex()+1;
    if(page_==4)return QStringList{"chats","communities","friends"}.indexOf(socialFace_);
    return 0;
}
QString ShellController::trainerFace() const {
    return trainerProfile_ ? QStringLiteral("profile") : QStringList{"journey","hall","ra"}[hall_.faceIndex()];
}
void ShellController::showTrainerFace(const QString& face) {
    const QStringList faces{"profile","journey","hall","ra"};
    const int index=faces.indexOf(face);
    if(index<0)return;
    trainerProfile_=index==0;
    if(index>0)hall_.showFace(index-1);
    if(trainerProfile_)trainer_.refreshOverview();
    emit changed();
}
void ShellController::goToTrainerFace(const QString& face) {
    if(navigationLocked() || !QStringList{"profile","journey","hall","ra"}.contains(face))return;
    goToPage(3);
    showTrainerFace(face);
}
void ShellController::showPokemonFace(const QString& face) {
    if(pokemonFace_=="playroom")playroomRoute_=party_.activities()->route()=="practice"?"practice":"playroom";
    if(face!=pokemonFace_){party_.activities()->practice()->leave();party_.activities()->link()->leave();}
    pokemonFace_=face;
    center_.leaveClinic();center_.leaveShops();
    if(face=="dex"){emit changed();return;}
    openCenter();
    if(face=="party" || face=="boxes")party_.showSection(face=="boxes"?"storage":"party");
    else if(face=="center"){
        if(centerRoute_=="backups"){party_.openSaves();center_.refresh();}
        else if(centerRoute_=="link"){party_.showSection("activities");party_.activities()->showPlace("link");}
        else {party_.showSection("party");center_.visitClinic();}
    }
    else if(face=="shops")center_.visitShops();
    else if(face=="playroom"){party_.showSection("activities");party_.activities()->showPlace(playroomRoute_);}
    emit changed();
}
void ShellController::openCenter() {
    center_.beginSelected(currentAdventureId());
    refreshParty();
}
void ShellController::refreshParty() {
    PerformanceTrace::Scope perf("ShellController.refreshParty");
    hall_.setCurrentAdventure(currentAdventureId());
    hall_.setProgress(progress_?progress_->adventureId():QString(),progress_?progress_->snapshot():GameProgress{});
    const auto adventure = homeAdventure();
    bool partyChanged = false;
    {
        const QSignalBlocker batch(&party_);
        partyChanged = party_.setAdventure(currentAdventureId(), adventure ? adventure->title : QString());
        partyChanged |= party_.setProgress(progress_ ? progress_->adventureId() : QString(), progress_ ? progress_->snapshot() : GameProgress{});
        const auto progress=progress_ && progress_->adventureId()==currentAdventureId()?progress_->snapshot():GameProgress{};
        party_.activities()->practice()->setObservation({trainer_.profile()["id"].toString(),currentAdventureId(),
            progress.contextRevision,progress.contentRevision,progress.saveRevision},progress,party_.activities()->actors());
        party_.activities()->link()->setTrainerName(trainer_.profile()["name"].toString());
        party_.activities()->link()->setObservation({trainer_.profile()["id"].toString(),currentAdventureId(),
            progress.contextRevision,progress.contentRevision,progress.saveRevision},progress,party_.activities()->actors());
    }
    if (partyChanged) emit party_.changed();
}
void ShellController::refreshContinue() {
    points_.clear();
    auto sessions=repository_.recentSessions();
    auto states=repository_.resumePoints();
    std::stable_sort(states.begin(),states.end(),[](const auto& a,const auto& b){return a.observedAt>b.observedAt;});
    // Repository order is launch order, even after the device clock changes.
    QSet<QString> represented;
    for(const auto& session:sessions) {
        const auto r=repository_.registration(session.adventureId);
        if((r && r->removed) || represented.contains(session.adventureId))continue;
        represented.insert(session.adventureId);
        std::optional<ResumePoint> resume;
        for(const auto& point:states)if(point.adventureId==session.adventureId && !point.id.isEmpty()){resume=point;break;}
        points_.append({"recent:"+session.adventureId,session.adventureId,session.startedAt,resume,session});
    }
    // Legacy read-only sample moments remain available in the development fixture.
    if(!repository_.editable()) {
        std::stable_sort(states.begin(),states.end(),[](const auto& a,const auto& b){return a.savedAt>b.savedAt;});
        for(const auto& point:states)if(!represented.contains(point.adventureId)) {
            represented.insert(point.adventureId);points_.append({point.id,point.adventureId,point.savedAt,point,{}});
        }
        std::stable_sort(points_.begin(),points_.end(),[](const auto& a,const auto& b){return a.recordedAt>b.recordedAt;});
    }
}

int ShellController::focusIndex() const {
    if (!notice_.isEmpty()) return 0;
    if (menuOpen_) return menuFocus_;
    if (keyboard_.isOpen()) return keyboard_.focusIndex();
    if (downloads_.isOpen()) return downloads_.focusIndex();
    if (scraper_.isOpen()) return scraper_.focusIndex();
    if (libraryTools_.isOpen()) return libraryTools_.focusIndex();
    if (drawerOpen_) return drawerFocus_;
    if (service_ == "library") return libraryManager_.files()->isOpen() ? libraryManager_.files()->focusIndex() : libraryManager_.focusIndex();
    if (service_ == "trainer-settings") return hall_.account()->isOpen() ? hall_.account()->focusIndex() : trainerSettingsFocus_;
    if (service_ == "trainer-setup") return trainerSetup_.focusIndex();
    if (service_ == "settings") return hall_.account()->isOpen() ? hall_.account()->focusIndex() : trainer_.editing() ? trainer_.focusIndex() : settings_.focusIndex();
    if (service_ == "device") return device_.focusIndex();
    if (service_ == "diagnostics") return diagnostics_.focusIndex();
    if (service_ == "center") return center_.focusIndex();
    if (trainer_.editing()) return trainer_.focusIndex();
    if (page_ == 1 && collectionsRoot_) return collectionFocus_;
    if (page_ == 1) return multiverseFace_ ? multiverse_.focusIndex() : worlds_.focusIndex();
    if (page_ == 2) return centerFace() ? (party_.section() == "saves" || center_.shopsOpen() || center_.clinicOpen() ? center_.focusIndex() : party_.focusIndex()) : pokedex_.focusIndex();
    if (trainerHistoryFace()) return hall_.focusIndex();
    return drawerOpen_ ? drawerFocus_ : 0;
}
QJsonObject ShellController::navigationState() const {
    const QStringList pages{"home", "worlds", "companions", "trainer", "social"};
    return {{"version", ShellNavigationVersion}, {"experienceVersion",pokemonExperience().version}, {"page", pages[page_]},
            {"trainerFace",trainerFace()},{"socialFace",socialFace_},
            {"homeAdventure", homeAdventureId_}, {"homeResume", homeResumeId_}, {"pokedexFace", pokemonFace_=="dex" ? "pokedex" : "center"}, {"pokemonFace",pokemonFace_}, {"centerRoute",centerRoute_}, {"party",party_.navigationState()},
            {"homeResumeSource", homeResumeSource_.toJson()},
            {"multiverse",multiverse_.navigationState()},{"homeDomain",multiverseHome_?"multiverse":"pokemon"},
            {"worldsFace",multiverseFace_?"multiverse":"pokemon"},
            {"globalHome",true},{"homeCollection",homeCollection_},{"worldCollection",worldCollection_},
            {"collectionsRoot",collectionsRoot_},{"collectionFocus",collectionFocus_},
            {"resume", drawerFocus_ < points_.size() ? points_[drawerFocus_].id : QString()},
            {"worlds", worlds_.navigationState()}, {"pokedex", pokedex_.navigationState()}, {"hall", hall_.navigationState()}};
}
void ShellController::restoreNavigation(const QJsonObject& state) {
    if(navigationLocked())return;
    homeMenuOpen_ = false;
    const int version=state["version"].toInt();
    if(version!=1 && version!=ShellNavigationVersion) {
        // A future layout is not permission to reinterpret its numeric slots.
        trainerProfile_=true;socialFace_="chats";social_.setFace(socialFace_);goToPage(0);return;
    }
    const QStringList legacyPages{"home","worlds","pokedex","trainer","hall"};
    const QStringList pages{"home", "worlds", "companions", "trainer", "social"};
    QString target=state["page"].toString();
    if(version==1 && state["page"].isDouble())target=legacyPages.value(state["page"].toInt(),"home");
    if(version==1 && target=="pokedex")target="companions";
    const bool legacyHall=version==1 && target=="hall";
    if(legacyHall)target="trainer";
    pokemonFace_ = "dex";
    trainerProfile_=true;
    socialFace_=version==2 && QStringList{"chats","groups","communities","friends"}.contains(state["socialFace"].toString()) ? state["socialFace"].toString() : "chats";
    social_.setFace(socialFace_);
    if(socialFace_=="groups")socialFace_="chats";
    goToPage(std::max(0, int(pages.indexOf(target))));
    homeAdventureId_ = state["homeAdventure"].toString(); homeResumeId_ = state["homeResume"].toString();
    homeResumeSource_ = ResumeSource::fromJson(state["homeResumeSource"].toObject());
    // Preserve the Adventure choice, but never restore a retired state target.
    if (repository_.editable()) { homeResumeId_.clear(); homeResumeSource_ = {}; }
    worlds_.restoreNavigation(state["worlds"].toObject());
    multiverse_.restoreNavigation(state["multiverse"].toObject());
    multiverseHome_=state["homeDomain"].toString()=="multiverse";
    multiverseFace_=state["worldsFace"].toString()=="multiverse";
    homeCollection_=state["homeCollection"].toString(multiverseHome_?"multiverse":"pokemon");
    worldCollection_=state["worldCollection"].toString(multiverseFace_?"multiverse":"pokemon");
    if(multiverseHome_ && !state.contains("homeCollection")) {
        const auto legacy=repository_.registration(state["multiverse"].toObject()["selected"].toString());
        if(legacy)homeCollection_=seriesForTitle(legacy->adventure.title);
    }
    collectionsRoot_=state["collectionsRoot"].toBool(true);
    collectionFocus_=std::clamp(state["collectionFocus"].toInt(),0,std::max(0,int(collections().size())-1));
    // Migrate the old independent non-Pokemon Home choice once; never change game IDs.
    if(!state["globalHome"].toBool() && multiverseHome_) {
        const auto legacy=state["multiverse"].toObject();
        const auto scoped=legacy["collections"].toObject()[homeCollection_].toObject();
        homeAdventureId_=scoped.value("selected").toString(legacy["selected"].toString());
        homeResumeId_.clear();homeResumeSource_={};
    }
    multiverseFace_=true;
    if(page_==1)multiverse_.setCollection(worldCollection_);
    pokedex_.restoreNavigation(state["pokedex"].toObject());
    // Establish the selected game's provider context before replaying its view.
    // Otherwise the initial current-game reconciliation replaces the restored
    // inactive RA detail with its default set list.
    refreshParty();
    hall_.restoreNavigation(state["hall"].toObject());
    if(legacyHall)trainerProfile_=false; // Its nested route chooses Journey, Hall or RA.
    else if(version==2)showTrainerFace(state["trainerFace"].toString("profile"));
    drawerFocus_ = 0;
    for (int i = 0; i < points_.size(); ++i) if (points_[i].id == state["resume"].toString()) drawerFocus_ = i;
    pokemonFace_=state["pokemonFace"].toString(state["pokedexFace"].toString()=="center"?"party":"dex");
    if(!pokemonExperience().pokemonFaces.contains(pokemonFace_))pokemonFace_="dex";
    centerRoute_=state["centerRoute"].toString()=="backups"?"backups":"clinic";
    party_.restoreNavigation(state["party"].toObject());
    if(page_==2)showPokemonFace(pokemonFace_);
    emit changed();
}
std::optional<Adventure> ShellController::homeAdventure() const {
    const auto adventures = repository_.adventures();
    for (const auto& a : adventures) if (a.id == currentAdventureId() && !a.collectionOnly) return a;
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
    PerformanceTrace::Scope perf("ShellController.home");
    const auto snapshot = repository_.home();
    QString title = "Choose a game in Collections", world = "Your journey";
    const auto adventure = homeAdventure();
    const auto media = adventure ? repository_.exitMedia(adventure->id) : std::nullopt;
    QString action = "Explore Collections", actionHint = "Collections", milestone = snapshot.milestone;
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
        const auto resumeStatus = homeResumeAvailability(*adventure);
        action = resumeStatus == ResumeAvailability::Exact ? "Resume Adventure" : "Start Adventure";
        actionHint = resumeStatus == ResumeAvailability::Exact ? "Resume" : "Play";
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
    PerformanceTrace::Scope perf("ShellController.resumePoints");

    QVariantList result;
    const auto adventures = repository_.adventures();
    const auto worlds = repository_.worlds();
    for (const auto& point : points_) {
        const auto media = repository_.exitMedia(point.adventureId);
        QString title = "Unavailable Adventure";
        QString world;
        auto status = ResumeAvailability::Incompatible;
        for (const auto& a : adventures) if (a.id == point.adventureId) {
            title = a.title;
            world = a.platformId.toUpper();
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
        QStringList items{"Power off", "Restart", "Cancel"};
        if (!platform_.dedicatedSession()) items.append(platform_.canSwitchSession() ? "Enter TrainerOS Mode" : "Exit Development App");
        return items;
    }
    // Stable action IDs: slot 1 retired when Controller moved into Settings.
    return {"Settings", "", "Switch Trainer", "",
            "Desktop / Maintenance Mode", "Steam Gaming Mode", "Power", "Volume", "Screen brightness", "Wi-Fi", "Bluetooth", "Airplane", "Downloads", "Scraping"};
}
void ShellController::openTrainers() {
    trainerChooserFromPower_ = menuOpen_;
    goToPage(page_);trainerSetup_.begin();service_="trainer-setup";emit changed();
}
void ShellController::goToPage(int page) {
    PerformanceTrace::Scope perf("ShellController.goToPage");
    if(navigationLocked(true))return;
    // Closing transient controllers emits their local notifications. Publish
    // only the completed shell transition, not every intermediate close, so
    // hidden Home/drawer bindings do not rebuild the library repeatedly.
    QSignalBlocker transition(this);
    homeMenuOpen_ = false;
    const bool enteringWorlds = page_ != 1 && std::clamp(page, 0, 4) == 1;
    if(page!=page_){party_.activities()->practice()->leave();party_.activities()->link()->leave();}
    collectionManager()->close();
    { PerformanceTrace::Scope phase("navigation.tools"); libraryTools_.close(); }
    { PerformanceTrace::Scope phase("navigation.storage"); settings_.storage()->close(); }
    { PerformanceTrace::Scope phase("navigation.keyboard"); if(textTarget_==TextTarget::Social)social_.preserveText(keyboard_.text()); keyboard_.cancel(); }
    textTarget_ = TextTarget::None;
    social_.closeMenu();
    pokedex_.cancelTransient();
    { PerformanceTrace::Scope phase("navigation.archive"); hall_.editor()->cancel(); }
    { PerformanceTrace::Scope phase("navigation.account"); hall_.account()->close(); }
    { PerformanceTrace::Scope phase("navigation.trainer"); trainer_.cancel(); }
    { PerformanceTrace::Scope phase("navigation.setup"); trainerSetup_.close(); }
    { PerformanceTrace::Scope phase("navigation.center"); center_.leaveClinic();center_.leaveShops(); }
    { PerformanceTrace::Scope phase("navigation.manager"); libraryManager_.close(); service_.clear(); }
    page_ = std::clamp(page, 0, 4); // No wrapping until physical-device testing.
    if(page_==1 && multiverseFace_)multiverse_.setCollection(worldCollection_);
    if (enteringWorlds) {
        collectionsRoot_=true;
        { PerformanceTrace::Scope phase("navigation.worlds"); worlds_.showRegions(); }
        { PerformanceTrace::Scope phase("navigation.multiverse"); multiverse_.showSystems(); }
    }
    if (page_ == 1) repository_.refreshContentAvailability();
    // Party observes the current Adventure, library and save provider directly.
    // Visiting an unrelated page must not republish the same actors/practice UI.
    if (page_==2) showPokemonFace(pokemonFace_);
    if (page_ == 3 && trainerProfile_) trainer_.refreshOverview();
    drawerOpen_ = false;
    menuOpen_ = false;
    if (powerMenu_) menuFocus_ = 6;
    powerMenu_ = false;
    notice_.clear();
    mode_.clear();
    transition.unblock();
    { PerformanceTrace::Scope phase("navigation.publish"); emit changed(); }
}
void ShellController::openCollection(int index) {
    const auto list=collections();if(index<0 || index>=list.size())return;
    const bool fromRoot=collectionsRoot_;
    collectionFocus_=index;worldCollection_=list[index].toMap()["id"].toString();
    multiverseFace_=true;collectionsRoot_=false;
    if(multiverseFace_){multiverse_.setCollection(worldCollection_);if(fromRoot)multiverse_.showSystems();}
    else if(fromRoot)worlds_.showRegions();
    emit changed();
}
void ShellController::editCollection(int index) {
    if(index<0 || index>=collections().size() || !collectionsRoot_ || !pairedNavigationAvailable())return;
    collectionFocus_=index;manageCollection(false);
}
void ShellController::manageCollection(bool create) {
    if(page_!=1 || navigationLocked() || menuOpen_ || serviceOpen() || keyboard_.isOpen() || localModalOpen() || !notice_.isEmpty())return;
    const auto id=collectionsRoot_?collections().value(collectionFocus_).toMap()["id"].toString():worldCollection_;
    collectionManager()->begin(create?QString():id);
}
void ShellController::cycleCollection(int delta) {
    const auto list=collections();if(list.isEmpty())return;
    const int index=(faceIndex()+delta+list.size())%list.size();
    if(page_!=1)return;
    if(collectionsRoot_)collectionFocus_=index;
    else openCollection(index);
    emit changed();
}

void ShellController::activate(int index, const QString& area) {
    if(downloads_.isOpen())return;
    if(scraper_.isOpen()) {if(keyboard_.isOpen())keyboard_.activate(index);else scraper_.activate(index);return;}
    if(homeMenuOpen_)return;
    if(collectionManager()->isOpen() && !menuOpen_ && notice_.isEmpty()) {if(keyboard_.isOpen())keyboard_.activate(index);else collectionManager()->activate(index);return;}
    if(launchPreparation_.busy() || libraryTools_.busy() || center_.writing())return;
    if(area=="world-edit" && canEditWorld()){libraryTools_.beginWorld(worlds_.region().value("id").toString(),true);return;}
    if (area == "continue") { dispatch(Action::ToggleContinue); return; }
    if (!notice_.isEmpty()) { confirm(); emit changed(); return; }
    if (menuOpen_) menuFocus_ = std::clamp(index, 0, int(menuItems().size()) - 1);
    else if (keyboard_.isOpen()) { keyboard_.activate(index); return; }
    else if (libraryTools_.isOpen()) {libraryTools_.activate(index);return;}
    else if (drawerOpen_) {
        auto& focus = drawerFocus_;
        focus = std::clamp(index, 0, std::max(0, int(resumePoints().size()) - 1));
    }
    else if (service_ == "library") { libraryManager_.activate(index, area); return; }
    else if (service_ == "trainer-settings") { trainerSettingsAction(index); return; }
    else if (service_ == "trainer-setup") { trainerSetup_.activate(index); return; }
    else if (service_ == "settings") { if(hall_.account()->isOpen()) { hall_.account()->activate(index); return; } if(trainer_.editing()) { trainer_.activate(index); return; } if(settings_.controlsFocused()) {if(settings_.category()==10)network_.activate(index);else settings_.activateRow(index);} else settings_.selectCategory(index,true); return; }
    else if (service_ == "device") { device_.activate(index); return; }
    else if (service_ == "diagnostics") { diagnostics_.activate(index); return; }
    else if (service_ == "center") { center_.activate(index); return; }
    else if (trainer_.editing()) { trainer_.activate(index); return; }
    else if (page_ == 1) {
        if(collectionsRoot_){openCollection(index);return;}
        if (multiverseFace_) { multiverse_.activate(index); return; }
        if (area == "worlds-search") worlds_.dispatch(Action::Secondary);
        else if (area == "worlds-filter") worlds_.dispatch(Action::ToggleContinue);
        else worlds_.activate(index);
        return;
    }
    else if (page_ == 2) {
        if (centerFace()) {
            if (area == "party-activities") showPokemonFace("playroom");
            else if (center_.shopsOpen()) center_.shopActivate(index);
            else if (center_.clinicOpen()) center_.dispatch(Action::Confirm);
            else if (party_.section() == "saves") center_.activate(index); else party_.activate(index);
            return;
        }
        if (area.isEmpty()) pokedex_.activate(index);
        else pokedex_.activateControl(area, index);
        return;
    }
    else if (trainerHistoryFace()) {
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
        if(!powerMenu_&&menuFocus_==12){menuOpen_=false;downloads_.begin();return;}
        if(!powerMenu_&&menuFocus_==13){
            menuOpen_=false;
            if(scraper_.busy())downloads_.begin();else scraper_.beginSystems();
            return;
        }
        if(scraper_.busy()&&((powerMenu_&&menuFocus_!=2)||(!powerMenu_&&(menuFocus_==2||menuFocus_==4||menuFocus_==5)))){showNotice("Finish or cancel downloads before switching Trainer or leaving this system session.");return;}
        if (powerMenu_) {
            if (menuFocus_ == 2) { powerMenu_ = false; menuFocus_ = 6; return; }
            if (menuFocus_ == 3) {
                if (platform_.canSwitchSession()) { mode_ = "traineros"; notice_ = "Enter the TrainerOS session?"; }
                else { mode_ = "development-exit"; notice_ = "Close the development app?"; }
                return;
            }
            device_.requestPower(menuFocus_ == 1);
            return;
        }
        if (menuFocus_ >= 7) { if(menuFocus_<9 || !network_.busy())device_.adjustQuick(menuFocus_ - 7, Action::Confirm); return; }
        if (menuFocus_ == 6) { powerMenu_ = true; menuFocus_ = 2; return; }
        if (menuFocus_ >= 4 && platform_.canSwitchSession()) {
            mode_ = menuFocus_ == 5 ? "steam" : "desktop";
            notice_ = mode_ == "steam" ? "Open Steam Gaming Mode? Your Trainer data will be saved first."
                : mode_ == "traineros" ? "Enter the dedicated TrainerOS session? Your Trainer data will be saved first."
                                      : "Open Desktop / Maintenance Mode? Your Trainer data will be saved first.";
            return;
        }

        if(menuFocus_==2) {
            if(trainerSetup_.live())emit trainersRequested();else openTrainers(); return;
        }
        if (menuFocus_ == 0 || menuFocus_ == 3) {
            hall_.account()->close();
            trainerSetup_.close();
            keyboard_.cancel(); textTarget_ = TextTarget::None; trainer_.cancel();
            collectionManager()->close(); libraryManager_.close(); libraryTools_.close(); menuOpen_ = false; drawerOpen_ = false;
            if(menuFocus_ != 0){center_.leaveClinic();center_.leaveShops();}
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
        if (points_.isEmpty()) { drawerOpen_ = false; return; }
        const auto& point = points_.at(drawerFocus_);
        for (const auto& adventure : repository_.adventures()) if (adventure.id == point.adventureId) {
            homeAdventureId_ = adventure.id; homeResumeId_ = point.resumePoint ? point.resumePoint->id : QString();
            homeResumeSource_ = point.resumePoint ? point.resumePoint->source : ResumeSource{};
            drawerOpen_ = false;
            if (centerFace()) showPokemonFace(pokemonFace_); else refreshParty();
            return;
        }
        notice_ = "This Adventure is unavailable. Its history has been kept.";
    } else if (page_ == 0) {
        const auto adventure = homeAdventure();
        if (!adventure) { multiverseFace_ = true; goToPage(1); return; }
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
            launchPreparation_.launch(adventure->id);
        } else notice_ = "This Adventure needs play setup.";
    }
    else if (page_ == 3 && trainerProfile_) {
        trainer_.beginEdit();
    } else if (page_ == 4) {
        social_.dispatch(Action::Confirm); return;
    } else {
        notice_ = "This section is not available in the prototype yet.";
    }
}
void ShellController::closeHomeMenu() {
    if (!homeMenuOpen_ || navigationLocked()) return;
    homeMenuOpen_ = false; emit changed();
}
int ShellController::homeMenuFocus() const {
    const auto actions=homeMenuActions();
    for(int i=0;i<actions.size();++i)if(actions[i].toMap()["id"]==homeMenuSelection_)return i;
    return 0;
}
QVariantList ShellController::homeMenuActions() const {
    const auto voice=social_.account()["voice"].toMap();
    if(homeCallOpen_) {
        QVariantList rows;
        if(!voice["channel"].toString().isEmpty())rows={
            QVariantMap{{"id","voice-mute"},{"label",voice["muted"].toBool()?"Turn microphone on":"Mute microphone"}},
            QVariantMap{{"id","voice-output"},{"label",voice["deaf"].toBool()?"Enable call sound":"Silence call sound"}},
            QVariantMap{{"id","voice-leave"},{"label","Leave call"}}};
        rows.append(QVariantMap{{"id","back"},{"label","Back"}});return rows;
    }
    QVariantList rows{QVariantMap{{"id","home"},{"label","Home"}},QVariantMap{{"id","friends"},{"label","Friends"}},
        QVariantMap{{"id","chats"},{"label","Chats"}},QVariantMap{{"id","notifications"},{"label","Notifications · "+QString::number(social_.notifications().size())}}};
    if(!voice["channel"].toString().isEmpty())rows.append(QVariantMap{{"id","call"},{"label","Voice call"},{"detail",voice["name"]}});
    if(!social_.gameParty()["party"].toString().isEmpty()||social_.gameParty()["joining"].toBool())rows.append(QVariantMap{{"id","game-party"},{"label","Game party"},{"detail",social_.gameParty()["game"].toMap()["label"]}});
    const bool answering=homeMenuSelection_.startsWith("answer-call:");
    const bool declining=homeMenuSelection_.startsWith("decline-call:");
    rows.append(social_.incomingCallActions(answering?homeMenuSelection_.section(':',1):QString()));
    if(homeMenuOpen_&&(answering||declining)) {
        const bool present=std::any_of(rows.cbegin(),rows.cend(),[this](const QVariant& row){return row.toMap()["id"]==homeMenuSelection_;});
        // Never redirect a pending A press onto Home when an asynchronous
        // update removes the selected invitation. Navigation clears this row.
        if(!present)rows.append(QVariantMap{{"id",homeMenuSelection_},{"label","Call no longer ringing"},{"enabled",false}});
    }
    return rows;
}
QString ShellController::homeMenuCaption() const {
    if(!homeCallOpen_)return "Your next stop";
    const auto voice=social_.account()["voice"].toMap();
    return voice["summary"].toString().isEmpty()?voice["status"].toString():voice["summary"].toString();
}
void ShellController::activateHomeMenu(int index) {
    if (!homeMenuOpen_ || navigationLocked() || index < 0 || index >= homeMenuActions().size()) return;
    const auto selected=homeMenuActions()[index].toMap();
    if(!selected.value("enabled",true).toBool())return;
    const auto id=selected["id"].toString();
    if(id=="game-party"){closeHomeMenu();goToPage(4);emit social_.runtimeAction("multiplayer-party");return;}
    if(id.startsWith("answer-call:")||id.startsWith("decline-call:")) {
        const bool accept=id.startsWith("answer-call:");
        if(social_.answerCall(id.section(':',1),accept,true)&&accept)closeHomeMenu();
        emit changed();return;
    }
    if(homeCallOpen_){if(id=="back"){homeCallOpen_=false;homeMenuSelection_="home";}else social_.controlCall(id);emit changed();return;}
    if(id=="call"){homeCallOpen_=true;homeMenuSelection_="voice-mute";emit changed();return;}
    if(index==3){notificationsOpen_=true;notificationFocus_=0;emit changed();return;}
    if(index==2&&!social_.notificationFace().isEmpty()){openSocialNotification();return;}
    if (index) socialFace_ = "chats";
    social_.setFace(socialFace_);
    goToPage(index ? 4 : 0);
    if(index==1)social_.showContacts();
}
void ShellController::activateNotification(int index) {
    if(!notificationsOpen()||navigationLocked())return;
    const auto row=social_.notifications().value(index).toMap();
    if(row["answerable"].toBool()) {
        if(social_.answerCall(row["id"].toString(),true))closeHomeMenu();
        return;
    }
    const auto face=social_.notificationFaceAt(index);if(face.isEmpty())return;
    socialFace_=face;goToPage(4);social_.openNotificationAt(index);emit changed();
}
void ShellController::openSocialNotification() {
    if(navigationLocked()||(!homeMenuOpen_&&!canReceiveNearby()))return;
    const auto face=social_.notificationFace();if(face.isEmpty())return;
    socialFace_=face;goToPage(4);social_.openNotification();emit changed();
}
void ShellController::dispatch(Action action) {
    if(downloads_.isOpen()){downloads_.dispatch(action);return;}
    if(scraper_.isOpen()&&action==Action::SystemMenu&&!keyboard_.isOpen()){menuFocus_=scraper_.selectingSystems()?13:12;scraper_.hide();menuOpen_=true;emit changed();return;}
    if(social_.online()["open"].toBool()) {
        if(action==Action::Confirm)social_.answerOnline(true);
        else if(action==Action::Back)social_.answerOnline(false);
        return;
    }
    if(party_.activities()->link()->invitationOpen()) {
        party_.activities()->link()->dispatch(action);return;
    }
    if(scraper_.isOpen()){if(keyboard_.isOpen())keyboard_.dispatch(action);else scraper_.dispatch(action);return;}
    if(launchPreparation_.busy() || libraryTools_.busy())return;
    if(navigationLocked(action==Action::PreviousPage || action==Action::NextPage) && (action==Action::Home || action==Action::PreviousPage || action==Action::NextPage || action==Action::SystemMenu || action==Action::PreviousFace || action==Action::NextFace))return;
    if (homeMenuOpen_) {
        if(notificationsOpen_) {
            if(action==Action::Home)closeHomeMenu();
            else if(action==Action::Back){notificationsOpen_=false;emit changed();}
            else if(action==Action::Confirm)activateNotification(notificationFocus());
            else if(action==Action::Secondary)social_.dismissNotificationAt(notificationFocus());
            else if(action==Action::SystemMenu){closeHomeMenu();dispatch(action);}
            else if(action==Action::Up||action==Action::Down){notificationFocus_=std::clamp(notificationFocus_+(action==Action::Up?-1:1),0,std::max(0,int(social_.notifications().size())-1));emit changed();}
            return;
        }
        if(action==Action::Back&&homeCallOpen_){homeCallOpen_=false;homeMenuSelection_="home";emit changed();}
        else if (action == Action::Home || action == Action::Back) closeHomeMenu();
        else if (action == Action::Confirm) activateHomeMenu(homeMenuFocus());
        else if (action == Action::Up || action == Action::Down) {
            const auto actions=homeMenuActions();
            const int index=std::clamp(homeMenuFocus() + (action == Action::Up ? -1 : 1), 0, qMax(0,int(actions.size())-1));
            homeMenuSelection_=actions.value(index).toMap()["id"].toString();emit changed();
        } else if (action == Action::SystemMenu) {
            closeHomeMenu(); dispatch(action);
        }
        return;
    }
    if(page_==1 && !menuOpen_ && !keyboard_.isOpen() && !localModalOpen() && !serviceOpen() && notice_.isEmpty() && action==Action::LocalAction) {manageCollection(false);return;}
    if(action==Action::ContextMenu && canHoldConfirm()) {
        libraryTools_.beginGame((multiverseFace_?multiverse_.detail():worlds_.detail()).value("id").toString());return;
    }
    if(action==Action::LocalAction && canEditWorld()) {libraryTools_.beginWorld(worlds_.region().value("id").toString(),true);return;}
    if (action == Action::Home) {
        homeMenuOpen_ = true; notificationsOpen_=homeCallOpen_=false; homeMenuSelection_ = "home";
        const auto actions=homeMenuActions();
        for(const auto& action:actions)if(action.toMap()["id"].toString().startsWith("answer-call:")){homeMenuSelection_=action.toMap()["id"].toString();break;}
        emit changed();return;
    }
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
            if (page_ == 1) cycleCollection(action==Action::NextFace?1:-1);
            else if (page_ == 3) {
                const QStringList faces{"profile","journey","hall","ra"};
                showTrainerFace(faces[(faceIndex()+(action==Action::NextFace?1:3))%4]);
            }
            else if (page_ == 4) { const QStringList faces{"chats","communities","friends"}; socialFace_=faces[(faceIndex()+(action==Action::NextFace?1:2))%3]; social_.setFace(socialFace_); }
            else if(page_==2) {
                const auto& faces=pokemonExperience().pokemonFaces;
                showPokemonFace(faces[(faceIndex()+(action==Action::NextFace?1:5))%6]);
            }
            emit changed();
        }
        return;
    }
    if (action == Action::ToggleContinue && chooseAdventureAvailable()) {
        if(!drawerOpen_) {
            const auto focused=points_.value(drawerFocus_).adventureId;
            repository_.refreshContentAvailability();refreshContinue();drawerFocus_=0;
            for(int i=0;i<points_.size();++i)if(points_[i].adventureId==focused){drawerFocus_=i;break;}
        }
        drawerOpen_ = !drawerOpen_; emit changed(); return;
    }
    if (notice_.isEmpty() && !menuOpen_) {
        if (keyboard_.isOpen()) {
            const bool naming=textTarget_==TextTarget::BoxName;
            const bool connecting=textTarget_==TextTarget::Network;
            if(textTarget_==TextTarget::Social && action==Action::Back)social_.preserveText(keyboard_.text());
            keyboard_.dispatch(action);
            if(naming && action==Action::Back) {textTarget_=TextTarget::None;party_.cancelBoxName();}
            if(connecting && action==Action::Back) {textTarget_=TextTarget::None;network_.cancelText();}
            return;
        }
        if(collectionManager()->isOpen()){collectionManager()->dispatch(action);return;}
        if (libraryTools_.isOpen()) {libraryTools_.dispatch(action);return;}
        if (drawerOpen_) {
            if (action == Action::Back) drawerOpen_ = false;
            else if (action == Action::Confirm) confirm();
            else if (action == Action::Left || action == Action::Right) {
                auto& focus = drawerFocus_;
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
        if (service_ == "settings") { if(hall_.account()->isOpen()) hall_.account()->dispatch(action); else if(trainer_.editing()) trainer_.dispatch(action,true); else if(settings_.category()==10 && settings_.controlsFocused()) network_.dispatch(action); else settings_.dispatch(action); return; }
        if (service_ == "device") { device_.dispatch(action); return; }
        if (service_ == "diagnostics") { diagnostics_.dispatch(action); return; }
        if (service_ == "center") { center_.dispatch(action); return; }
        if (trainer_.editing()) { trainer_.dispatch(action); return; }

        if (page_ == 1) {
            if(collectionsRoot_) {
                if(action==Action::Back){goToPage(0);return;}
                if(action==Action::Confirm)openCollection(collectionFocus_);
                else {
                    const int delta=action==Action::Left?-1:action==Action::Right?1:action==Action::Up?-3:action==Action::Down?3:0;
                    collectionFocus_=std::clamp(collectionFocus_+delta,0,std::max(0,int(collections().size())-1));emit changed();
                }
                return;
            }
            if(action==Action::Back && (multiverseFace_?(multiverse_.collection()!="multiverse" || multiverse_.route()=="systems"):worlds_.route()=="regions")) {
                collectionsRoot_=true;emit changed();return;
            }
            if (multiverseFace_) multiverse_.dispatch(action); else worlds_.dispatch(action);
            return;
        }
        if (page_ == 2) {
            if (centerFace()) {
                if (pokemonFace_=="center" && center_.clinicOpen() && action==Action::LocalAction) {centerRoute_="backups";center_.leaveClinic();party_.openSaves();}
                else if (pokemonFace_=="center" && center_.clinicOpen() && action==Action::Secondary) {centerRoute_="link";center_.leaveClinic();party_.showSection("activities");party_.activities()->showPlace("link");}
                else if (center_.clinicOpen()) {if(action!=Action::Back)center_.dispatch(action);if(!center_.clinicOpen())center_.visitClinic();}
                else if (center_.shopsOpen()) {center_.dispatch(action);if(!center_.shopsOpen())center_.visitShops();}
                else if(pokemonFace_=="center" && centerRoute_=="link")party_.dispatch(action);
                else if (pokemonFace_=="center" && action==Action::Back && !center_.confirming()){centerRoute_="clinic";showPokemonFace("center");}
                else if (party_.section() != "saves") party_.dispatch(action);
                else if (action == Action::Back && !center_.confirming() && !center_.busy()){centerRoute_="clinic";showPokemonFace("center");}
                else center_.dispatch(action);
            }
            else pokedex_.dispatch(action);
            return;
        }
        if (trainerHistoryFace()) { hall_.dispatch(action == Action::LocalAction && !localModalOpen() ? Action::ToggleContinue : action); return; }
        if (page_ == 4) { social_.dispatch(action); return; }
    }
    if (menuOpen_ && !powerMenu_ && notice_.isEmpty() && action == Action::Secondary) {
        if (menuFocus_ >= 7) menuFocus_ = menuServiceFocus_;
        else { menuServiceFocus_ = menuFocus_; menuFocus_ = 7; }
        emit changed(); return;
    }
    if (menuOpen_ && !powerMenu_ && notice_.isEmpty() && menuFocus_ >= 7
            && (action == Action::Left || action == Action::Right)) {
        if(menuFocus_>=12){menuFocus_=action==Action::Right?13:12;emit changed();return;}
        if (menuFocus_ >= 9) { menuFocus_ = std::clamp(menuFocus_ + (action == Action::Right ? 1 : -1), 9, 11); emit changed(); return; }
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
            if (menuFocus_ >= 9 && menuFocus_<=11 && delta > 0) { menuFocus_ = 7; emit changed(); return; }
            if (menuFocus_ >= 9 && menuFocus_<=11) { emit changed(); return; }
            if(menuFocus_==13 && delta)menuFocus_=12;
            const QList<int> order{9,7,8,12,0,2,4,5,6};
            *focus=order[std::clamp(int(order.indexOf(*focus))+delta,0,int(order.size())-1)];
        } else *focus = std::clamp(*focus + delta, 0, std::max(0, count - 1));
    }
    emit changed();
}
}
