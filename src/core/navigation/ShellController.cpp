#include <cmath>
#include "core/model/GameFavorite.h"
#include "core/PerformanceTrace.h"
#include "ShellController.h"
#include "core/model/SeriesCatalog.h"
#include "features/home/PlayHistoryController.h"
#include "ResumePresentation.h"
#include <QSet>
#include <QTimer>
#include <QSignalBlocker>
#include <QCryptographicHash>
#include <algorithm>
#include "core/model/NavigationVersion.h"

namespace trainer {
void ShellController::showAchievements(const QString& title,const QStringList& names) {
    if(names.isEmpty())return;
    const auto id="achievement:"+QString::fromLatin1(QCryptographicHash::hash((title+"\n"+names.join('\n')).toUtf8(),QCryptographicHash::Sha256).toHex());
    bool known=false;for(const auto& row:achievementNotifications_)if(row.toMap()["id"]==id)known=true;
    if(known)return;
    if(!known)achievementNotifications_.prepend(QVariantMap{{"id",id},{"name",title},{"detail",names.join(" · ")},{"provider","achievements"}});
    while(achievementNotifications_.size()>32)achievementNotifications_.removeLast();
    achievementToast_=title+"\n"+(names.size()==1?names.first():QString("%1 achievements unlocked").arg(names.size()));
    const auto generation=++achievementToastGeneration_;emit changed();
    QTimer::singleShot(7000,this,[this,generation]{if(generation==achievementToastGeneration_){achievementToast_.clear();emit changed();}});
}
QVariantList ShellController::notifications() const {auto rows=social_.notifications();rows.append(achievementNotifications_);return rows;}
QVariantMap ShellController::passiveNotice() const {
    if(social_.account()["doNotDisturb"].toBool())return {};
    if(!social_.toastTitle().isEmpty())return {{"title",social_.toastTitle()},{"detail",social_.toastText()},{"kind","social"}};
    if(!achievementToast_.isEmpty())return {{"title",achievementToast_.section('\n',0,0)},{"detail",achievementToast_.section('\n',1)},{"kind","achievement"}};
    return {};
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
        if(const auto* module=resolveExperience(record->adventure))rows.append(module->capabilityNotes());
        if(progress_)progress_->inspectCapabilities(*record,receiver,[done,rows](QStringList semantic){
            done(rows+semantic);
        });
        else done(rows);
    };
    if (progress_) connect(progress_, &GameProgressProvider::changed, this, [this] {
        refreshExperience();
        emit changed();
    });
    if (progress_) {
        refreshExperience();
    }
    emit changed();
}
ShellController::ShellController(LibraryRepository& repo, TrainerRepository& profiles, AdventureAdapter& adapter,
        PlatformService& platform, ExperienceFactory factory,
        HallOfFameRepository& archive, AchievementProvider& achievements, QObject* parent)
    : QObject(parent), repository_(repo), adapter_(adapter), platform_(platform),
      keyboard_(this), trainer_(profiles, this), worlds_(repo, adapter, this), multiverse_(repo, adapter, this),
      hall_(archive, achievements, this),
      libraryManager_(repo, nullptr, this), libraryTools_(repo,this), scraper_(repo,this), settings_(this), device_(this), diagnostics_(this), dashboard_(repo,achievements,social_,this), story_(repo,achievements) {

    connect(&dashboard_,&HomeDashboardController::changed,this,&ShellController::changed);
    modules_=factory({repo,trainer_,hall_,archive,profiles});
    Q_ASSERT(!modules_.empty());
    for(auto& owned:modules_) {
        auto* module=owned.get();
        connect(module,&ExperienceModule::changed,this,&ShellController::changed);
        connect(module,&ExperienceModule::notice,this,&ShellController::showNotice);
        connect(module,&ExperienceModule::screenRequested,this,[this,module](QString face,bool accepted,quint64 generation){
            if(module!=activeModule_ || generation!=experience_.generation())return;
            if(!experience_.screenIds().contains(face))return;
            if(accepted){page_=GameHubPage;hubFace_="adapter";adapterMenuOpen_=false;menuOpen_=drawerOpen_=false;service_.clear();}
            else if(navigationLocked())return;
            showExperienceScreen(face);
        });
        connect(module,&ExperienceModule::hostActionRequested,this,[this,module](const QString& action,quint64 generation){
            if(module!=activeModule_ || generation!=experience_.generation())return;
            if(action=="home")goToPage(0);
            else if(action=="game-properties"){if(currentAdventureId().isEmpty())goToPage(1);else libraryTools_.beginGame(currentAdventureId());}
            else if(action=="clear-resume"){homeResumeId_.clear();homeResumeSource_={};emit changed();}
            else if(action=="close-service"){service_.clear();menuOpen_=true;menuFocus_=2;emit changed();}
        });
        connect(module,&ExperienceModule::textRequested,this,[this,module](QString title,QString initial,int limit,quint64 generation){
            if(module!=activeModule_ || generation!=experience_.generation())return;
            textTarget_=TextTarget::Module;textModule_=module;textGeneration_=experience_.generation();keyboard_.begin(title,initial,limit);
        });
        connect(module,&ExperienceModule::textCancelled,this,[this,module]{if(textModule_==module){textModule_=nullptr;textTarget_=TextTarget::None;keyboard_.cancel();}});
    }
    connect(this,&ShellController::changed,this,&ShellController::syncExperience);
    connect(&album_,&ScreensController::changed,this,&ShellController::changed);
    album_.setRepository(&repository_);
    connect(&hall_,&HallOfFameController::momentRequested,this,[this](const QString& id){
        if(!album_.showCapture(id))return;
        achievementPhotoNavigation_=hall_.navigationState();
        setHubFace("overview");showOverview("screens");achievementPhotoReturn_=true;emit changed();
    });
    connect(&social_,&SocialController::presentationChanged,this,&ShellController::changed,Qt::QueuedConnection);
    settings_.setScraper(&scraper_);
    connect(&libraryTools_,&LibraryToolsController::browserActionRequested,this,&ShellController::libraryAction);
    connect(&settings_,&SettingsController::collectionsRequested,this,[this]{goToPage(1);});
    connect(collectionManager(),&GameCollections::changed,this,&ShellController::changed);
    connect(collectionManager(),&GameCollections::definitionsChanged,this,[this]{
        collectionFocus_=std::clamp(collectionFocus_,0,std::max(0,int(collections().size())-1));
        if(!collectionManager()->known(worldCollection_)){worldCollection_="multiverse";collectionsRoot_=true;}
        emit changed();
    });
    connect(collectionManager(),&GameCollections::textRequested,this,[this](const QString& title,const QString& initial,int limit){textTarget_=TextTarget::Collections;keyboard_.begin(title,initial,limit);});
    connect(&libraryTools_,&LibraryToolsController::collectionsRequested,this,[this](const QString& id){libraryTools_.close();collectionManager()->beginMembership(id);});
    libraryTools_.editGuard=[this]{return !liveGame_.isEmpty()?QString("Exit the running game before changing library files."):scraper_.busy()?QString("Finish or cancel downloads before changing library files or names."):QString();};
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
    connect(&libraryTools_,&LibraryToolsController::togetherRequested,this,&ShellController::togetherRequested);
    connect(&libraryTools_,&LibraryToolsController::lobbyRequested,&social_,&SocialController::openLobbyForGame);
    connect(&social_,&SocialController::reviewsChanged,&libraryTools_,&LibraryToolsController::receiveReviews);
    connect(&network_, &NetworkController::changed,this,&ShellController::changed);
    connect(settings_.clock(), &ClockController::changed, this, &ShellController::changed);
    settings_.communication()->configure(&social_);
    connect(&social_,&SocialController::lobbyRequested,this,[this]{
        closeHomeMenu();openGameHubFace("lobby");
    });
    connect(settings_.communication(),&CommunicationSettings::changed,this,[this]{if(service_=="account-profile" || (service_=="settings" && settings_.category()==13))emit changed();});
    connect(settings_.communication(),&CommunicationSettings::backRequested,this,[this]{if(service_=="account-profile"){service_.clear();emit changed();}else settings_.selectCategory(13,false);});
    connect(this,&ShellController::changed,this,[this]{if(service_!="account-profile" && (service_!="settings" || settings_.category()!=13))settings_.communication()->leave();});
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
    connect(&social_,&SocialController::conversationsRequested,this,[this]{
        if(navigationLocked())return;
        socialFace_="chats";social_.setFace(socialFace_);goToPage(SocialPage);emit changed();
    });
    connect(&social_,&SocialController::communicationSettingsRequested,this,[this]{
        if(navigationLocked())return;
        service_="settings";menuOpen_=drawerOpen_=false;settings_.begin();settings_.selectCategory(13);emit changed();
    });
    connect(&multiverse_, &MultiversePresentation::changed, this, &ShellController::changed);
    connect(&multiverse_, &MultiversePresentation::searchRequested, this, [this](const QString& text) {
        textTarget_ = TextTarget::MultiverseSearch; keyboard_.begin("Multiverse · find a title", text, 48);
    });
    connect(&launchPreparation_, &LaunchPreparation::changed, this, &ShellController::changed);
    connect(&launchPreparation_, &LaunchPreparation::libraryChanged, this, &ShellController::refreshLibrary);
    connect(&launchPreparation_, &LaunchPreparation::messageRequested, this, &ShellController::showNotice);
    const auto openGame=[this](const QString& id){
        if(!liveGame_.isEmpty()) {
            if(liveGame_.value("id")==id)returnToLiveGame();
            else showNotice("A game is still running. Return to it or exit it from Game Options before starting another.");
            return;
        }
        launchPreparation_.launch(id);
    };
    connect(&multiverse_, &MultiversePresentation::setupRequested, this, openGame);
    connect(&worlds_, &WorldsController::setupRequested, this, openGame);
    connect(&multiverse_, &MultiversePresentation::messageRequested, this, &ShellController::showNotice);
    connect(&multiverse_, &MultiversePresentation::homeRequested, this, [this] {
        homeAdventureId_=multiverse_.selected().value("id").toString();homeResumeId_.clear();homeResumeSource_={};refreshExperience();goToPage(0);
    });
    connect(&settings_, &SettingsController::trainerRequested, this, [this](int index) {
        trainerSettingsAction(index);
    });
    connect(&apps_, &AppsController::changed, this, &ShellController::changed);
    connect(&apps_, &AppsController::foregroundRequested, this, [this]{
        if(quickAppsOpen_){quickAppsOpen_=false;apps_.leave();emit changed();}
    });
    connect(&apps_, &AppsController::homeRequested, this, [this]{goToPage(0);});
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
    hall_.editor()->setLibrary(&repo);
    if (!repo.editable()) hall_.enableSampleJourney();
    hall_.showJourney();
    connect(hall_.account(), &AchievementAccountController::textRequested, this,
        [this](const QString& title, const QString& initial, int limit, bool secret) {
            textTarget_ = TextTarget::AchievementAccount; keyboard_.begin(title, initial, limit, secret);
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
    connect(&libraryManager_, &LibraryManagementController::closeRequested, this, [this] { service_.clear(); if(activeModule_ && adapterActive())activeModule_->show(experience_.screen()); menuOpen_ = !libraryFromWorlds_; libraryFromWorlds_ = false; emit changed(); });
    connect(&libraryManager_, &LibraryManagementController::textRequested, this, [this](const QString& title, const QString& initial, int limit) {
        textTarget_ = TextTarget::Library; keyboard_.begin(title, initial, limit);
    });
    connect(&settings_, &SettingsController::changed, this, &ShellController::changed);
    connect(&settings_, &SettingsController::quickAdjustment, &device_, &DeviceController::adjustQuick);
    connect(&settings_, &SettingsController::messageRequested, this, [this](const QString& message) {
        if (service_ != "settings" || menuOpen_) { notice_ = message; emit changed(); }
    });
    connect(&settings_, &SettingsController::closeRequested, this, [this] { service_.clear(); menuOpen_ = false; if(optionsReturn_){optionsReturn_=false;homeMenuOpen_=true;}emit changed(); });
    connect(&hall_, &HallOfFameController::changed, this, &ShellController::changed);
    connect(&hall_, &HallOfFameController::messageRequested, this, [this](const QString& message) {
        notice_ = message; emit changed();
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
        if(socialActive()||homeMenuOpen_)emit changed();
    });
    connect(&social_, &SocialController::textRequested, this, [this](QString title, QString text, int limit) {
        textTarget_ = TextTarget::Social; keyboard_.begin(title,text,limit,false,social_.textSubmitLabel(),social_.textAllowsEmoji());
    });
    connect(&keyboard_, &TextEntryController::accepted, this, [this](const QString& text) {
        const auto target = textTarget_;
        textTarget_ = TextTarget::None;
        if (target == TextTarget::SetupName) trainerSetup_.applyName(text);
        else if (target == TextTarget::TrainerName) trainer_.setDraftName(text);
        else if (target == TextTarget::WorldsSearch) worlds_.applySearch(text);
        else if (target == TextTarget::MultiverseSearch) multiverse_.applySearch(text);
        else if (target == TextTarget::Library) libraryManager_.applyText(text);
        else if (target == TextTarget::LibraryTools) libraryTools_.applyText(text);
        else if (target == TextTarget::Collections) collectionManager()->applyText(text);
        else if (target == TextTarget::Archive) hall_.editor()->applyText(text);
        else if (target == TextTarget::AchievementAccount) hall_.account()->applyText(text);
        else if(target==TextTarget::Module){if(textModule_ && textModule_==activeModule_ && textGeneration_==experience_.generation())textModule_->applyText(text);textModule_=nullptr;}
        else if (target == TextTarget::Network) network_.applyText(text);
        else if (target == TextTarget::Social) social_.applyText(text);
        else if (target == TextTarget::Scraper) scraper_.applyText(text);
        else if (target == TextTarget::Communication) settings_.communication()->applyText(text);
    });
    refreshContinue();
    syncExperience();
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
    if(activeModule_ && adapterActive())activeModule_->show(experience_.screen());
    refreshContinue();
    refreshExperience();
    emit changed();
}
QString ShellController::currentAdventureId() const {
    if(!liveGame_.value("id").toString().isEmpty())return liveGame_.value("id").toString();
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
    return collectionManager()->isOpen() || downloads_.isOpen() || scraper_.isOpen() || (socialActive() && !social_.menu().isEmpty()) || libraryTools_.isOpen() || trainer_.editing()
        || (adapterActive() && activeModule_ && activeModule_->modalOpen())
        || (page_==GameHubPage && hubFace_=="ra" && (hall_.editor()->isOpen() || hall_.account()->isOpen() || hall_.galleryMenuOpen() || hall_.route()=="achievement-detail"));
}
bool ShellController::chooseAdventureAvailable() {
    return !homeMenuOpen_ && (!adapterActive() || !activeModule_ || activeModule_->recentsAllowed())
        && !menuOpen_ && notice_.isEmpty() && !keyboard_.isOpen()
        && !localModalOpen() && !moduleActivityBusy() && !navigationLocked();
}
bool ShellController::navigationLocked(bool primaryRecovery) const {
    return modulesBlocked(primaryRecovery) || launchPreparation_.busy() || settings_.clock()->busy() || settings_.storage()->busy() || libraryTools_.busy();
}
bool ShellController::modulesBlocked(bool recovery) const {for(const auto& module:modules_)if(module->navigationBlocked(recovery))return true;return false;}
bool ShellController::moduleWriting() const {for(const auto& module:modules_)if(module->writing())return true;return false;}
bool ShellController::moduleActivityBusy(bool recovery) const {for(const auto& module:modules_)if(module->activityBusy(recovery))return true;return false;}
ExperienceModule* ShellController::module(const QString& id) const {for(const auto& module:modules_)if(module->descriptor().id==id)return module.get();return nullptr;}
QVariantMap ShellController::moduleInvitation() const {for(const auto& module:modules_){auto request=module->invitation();if(request["open"].toBool())return request;}return {};}

QString ShellController::primaryId() const {
    return QStringList{"home","games","gamehub","social","apps"}.value(page_,"home");
}
QStringList ShellController::primaryNames() const {
    return {"Home","Games","GameHub","Social","Apps"};
}
QStringList ShellController::hubFaceIds() const {
    QStringList result{"overview","ra"};
    if(experience_.hasAdapter())result.append("adapter");
    result.append("lobby");return result;
}
void ShellController::goToSection(const QString& id) {
    const auto index=QStringList{"home","games","gamehub","social","apps"}.indexOf(id);
    if(index>=0)goToPage(index);
}
void ShellController::setHubFace(const QString& requested) {
    const auto face=hubFaceIds().contains(requested)?requested:QString("overview");
    const bool switching=hubFace_!=face;
    if(switching && adapterActive() && activeModule_)
        experience_.rememberNavigation({{"module",activeModule_->navigation()},{"hall",hall_.navigationState()}});
    if(hubFace_!=face && activeModule_)activeModule_->leave(false);
    hubFace_=face;quickAppsOpen_=false;
    if(face=="adapter") {
        showExperienceScreen(experience_.screen());
        if(switching && activeModule_)activeModule_->restoreNavigation(experience_.navigation()["module"].toObject());
    }
    else if(face=="ra")hall_.showCurrentAchievements();
    else if(face=="lobby")social_.setFace("lobby");
    emit changed();
}
void ShellController::openGameHubFace(const QString& face) {
    if(navigationLocked() || keyboard_.isOpen() || localModalOpen())return;
    goToPage(GameHubPage);setHubFace(face);
}
void ShellController::selectFace(int index) {
    if(index<0 || index>=faceNames().size() || index==faceIndex()
       || !pairedNavigationAvailable() || navigationLocked())return;
    quickAppsOpen_=false;
    if(page_==GameHubPage)setHubFace(hubFaceIds().value(index));
    else if(page_==SocialPage) {
        socialFace_=QStringList{"chats","communities","friends"}.value(index,"chats");
        social_.setFace(socialFace_);
    } else if(page_==AppsPage)appsFace_=QStringList{"apps","crates","browser","files"}.value(index,"apps");
    else if(page_==GamesPage && !collectionsRoot_) {
        const auto rows=gamesRootZone_=="platforms"?gamesPlatforms():gamesCollections();
        openGamesRootItem(gamesRootZone_,rows.value(index).toMap()["id"].toString());
    }
    emit changed();
}
bool ShellController::pairedNavigationAvailable() {
    return !homeMenuOpen_ && !serviceOpen() && !menuOpen_ && notice_.isEmpty()
        && !keyboard_.isOpen() && !localModalOpen() && !drawerOpen_ && !quickAppsOpen_;
}
QVariantList ShellController::adapterMenu() const {
    QVariantList rows;
    for(const auto& screen:experience_.descriptor().adapter.screens)
        rows.append(QVariantMap{{"id",screen.id},{"label",screen.label},{"icon",screen.icon}});
    return rows;
}
void ShellController::selectAdapterScreen(const QString& screen) {
    if(!adapterActive() || navigationLocked() || !pairedNavigationAvailable()
       || !experience_.screenIds().contains(screen))return;
    adapterMenuOpen_=false;adapterMenuFocus_=experience_.screenIds().indexOf(screen);
    showExperienceScreen(screen);
}
void ShellController::showAdapterMenu() {
    if(!adapterActive() || navigationLocked() || localModalOpen() || keyboard_.isOpen())return;
    adapterMenuOpen_=true;adapterMenuFocus_=qMax(0,experience_.index());emit changed();
}
void ShellController::showOverview(const QString& face) {
    if(page_!=GameHubPage || hubFace_!="overview" || !pairedNavigationAvailable())return;
    if(!QStringList{"summary","timeline","screens"}.contains(face))return;
    if(overviewFace_=="timeline")overviewSessionId_=storyHistory().value(overviewFocus_).toMap()["id"].toString();
    const auto previous=overviewFace_;
    overviewFace_=face;overviewFocus_=face=="summary" && previous=="screens"?1:0;
    if(face=="timeline" && !overviewSessionId_.isEmpty()) {
        const auto rows=storyHistory();
        for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==overviewSessionId_){overviewFocus_=i;break;}
    }
    emit changed();
}
void ShellController::closeDrawers() {
    if(keyboard_.isOpen() || localModalOpen() || homeMenuOpen_ || menuOpen_
       || !notice_.isEmpty() || !apps_.menu().isEmpty() || navigationLocked())return;
    if(quickAppsOpen_)apps_.leave();
    quickAppsOpen_=drawerOpen_=false;emit changed();
}
void ShellController::toggleQuickApps() {
    if(keyboard_.isOpen() || localModalOpen() || homeMenuOpen_ || menuOpen_
       || !notice_.isEmpty() || navigationLocked())return;
    quickAppsOpen_=!quickAppsOpen_;drawerOpen_=false;
    if(quickAppsOpen_){apps_.refresh();apps_.focusRail();}
    else apps_.leave();
    emit changed();
}
void ShellController::activateDashboard(const QString& key) {
    if(page_!=HomePage || !pairedNavigationAvailable() || navigationLocked())return;
    const auto target=dashboard_.target(key);if(target.isEmpty())return;
    dashboard_.select(key);
    if(target["type"]=="play"){confirm();emit changed();return;}
    if(target["type"]=="screens") {
        openGameHubFace("overview");showOverview("screens");
        album_.forGame(currentAdventureId());
        dashboardReturn_=true;
    } else if(target["type"]=="person") {
        socialFace_="chats";goToPage(SocialPage);
        if(!social_.openQuickConversation(target["conversation"].toString()))social_.profile(target["id"].toString());
        dashboardReturn_=true;
    } else if(target["type"]=="event") {
        homeMenuOpen_=true;openNotificationById(target["id"].toString());
        if(page_!=HomePage)dashboardReturn_=true;
    } else if(target["type"]=="achievement") {
        openGameHubFace("ra");auto state=hall_.navigationState();
        state["route"]="achievement-detail";state["set"]=target["set"].toString();state["zone"]="actions";
        auto ids=state["achievements"].toObject();ids[target["set"].toString()]=target["id"].toString();state["achievements"]=ids;
        hall_.restoreNavigation(state);dashboardReturn_=true;
    }
    emit changed();
}
void ShellController::setLiveGame(const QVariantMap& game) {
    if(liveGame_==game)return;
    const auto previous=currentAdventureId();
    liveGame_=game;
    if(!game["id"].toString().isEmpty())homeAdventureId_=game["id"].toString();
    if(previous!=currentAdventureId())refreshExperience();
    emit changed();
}
bool ShellController::dispatchGameHub(Action action) {
    if(page_!=GameHubPage)return false;
    if(hubFace_=="lobby"){social_.dispatch(action);return true;}
    if(hubFace_=="ra"){hall_.dispatch(action==Action::LocalAction?Action::ToggleContinue:action);return true;}
    if(adapterActive() && activeModule_) {
        if(adapterMenuOpen_) {
            const auto ids=experience_.screenIds();
            if(action==Action::Up || action==Action::Down)
                adapterMenuFocus_=std::clamp(adapterMenuFocus_+(action==Action::Down?1:-1),0,qMax(0,int(ids.size())-1));
            else if(action==Action::Confirm || action==Action::Right)selectAdapterScreen(ids.value(adapterMenuFocus_));
            else if(action==Action::Back)setHubFace("overview");
            emit changed();return true;
        }
        const auto before=activeModule_->navigation();
        const bool modal=activeModule_->modalOpen();
        const bool handled=activeModule_->dispatch(action);
        if(action==Action::Back && !modal && !navigationLocked() && before==activeModule_->navigation()){showAdapterMenu();return true;}
        return handled;
    }
    if(overviewFace_=="screens" && (action!=Action::Back || album_.filtering())) {album_.dispatch(action);return true;}
    if(action==Action::Back) {
        if(overviewFace_!="summary")showOverview("summary");else goToPage(HomePage);
    } else if(action==Action::Confirm) {
        if(overviewFace_=="summary")showOverview(overviewFocus_==0?"timeline":"screens");
    } else if(action==Action::Up || action==Action::Down || action==Action::Left || action==Action::Right) {
        const int count=overviewFace_=="summary"?2:overviewFace_=="timeline"?storyHistory().size():storyMedia().size();
        const int direction=action==Action::Down || action==Action::Right?1:-1;
        // History is newest-first; the visual timeline runs oldest to newest.
        overviewFocus_=std::clamp(overviewFocus_+(overviewFace_=="timeline"?-direction:direction),0,qMax(0,count-1));
    }
    emit changed();return true;
}
ExperienceModule* ShellController::resolveExperience(const std::optional<Adventure>& game) const {
    ExperienceModule *fallback=nullptr,*selected=nullptr;int best=0;bool ambiguous=false;
    const auto evidence=game?repository_.experienceIdentity(game->id):QVariantMap{};
    for(const auto& module:modules_) {
        if(!module->enabled() || !ExperienceNavigation::valid(module->descriptor()) || module->presenter().scheme()!="qrc")continue;
        if(module->descriptor().id=="generic"){fallback=module.get();continue;}
        const int score=module->match(game,evidence);
        if(score>best){selected=module.get();best=score;ambiguous=false;}
        else if(score && score==best)ambiguous=true;
    }
    return ambiguous || !selected?fallback:selected;
}
void ShellController::syncExperience() {
    if(syncingExperience_)return;
    syncingExperience_=true;
    const auto game=homeAdventure();
    const auto record=game?repository_.registration(game->id):std::nullopt;
    const auto owner=trainer_.profile()["id"].toString();
    if(experience_.owner()!=owner){overviewFocus_=0;overviewSessionId_.clear();overviewFace_="summary";}
    if(!experience_.owner().isEmpty() && experience_.owner()!=owner){dashboard_.reset();achievementNotifications_.clear();achievementToast_.clear();++achievementToastGeneration_;}
    album_.setOwner(owner);
    album_.setMedia(storyMedia());
    ExperienceModule* next=resolveExperience(game);
    Q_ASSERT(next);
    const auto descriptor=next->descriptor();
    dashboard_.setContext(currentAdventureId(),descriptor.id=="generic");
    const bool changing=activeModule_!=next || experience_.owner()!=owner || experience_.adventure()!=currentAdventureId() || experience_.descriptor()!=descriptor;
    if(changing && activeModule_)experience_.rememberNavigation({{"module",activeModule_->navigation()},{"hall",hall_.navigationState()}});
    const bool changed=experience_.select(owner,currentAdventureId(),descriptor,record?record->revision:0);
    if(changed || changing) {
        if(activeModule_)activeModule_->leave(false);
        if(textModule_){textModule_->cancelText();textModule_=nullptr;keyboard_.cancel();textTarget_=TextTarget::None;}
        hall_.editor()->cancel();activeModule_=next;
        if(hubFace_=="adapter" && !experience_.hasAdapter())hubFace_="overview";
        adapterMenuFocus_=qMax(0,experience_.index());
        ExperienceContext context{owner,currentAdventureId(),game,trainer_.profile(),repository_.home(),progress_,experience_.generation()};
        for(auto& module:modules_)module->refresh(context);
        if(changing) {
            const auto restored=experience_.navigation();
            activeModule_->restoreNavigation(restored.contains("module")?restored["module"].toObject():restored.contains(descriptor.id)?restored.value(descriptor.id).toObject():restored);
            hall_.setCurrentAdventure(currentAdventureId());hall_.restoreNavigation(restored["hall"].toObject());
        }
        if(adapterActive())showExperienceScreen(experience_.screen());
        else if(hubFace_=="ra")hall_.showCurrentAchievements();
    }
    syncingExperience_=false;
}
void ShellController::showExperienceScreen(const QString& screen) {
    if(!experience_.show(screen) || !activeModule_)return;
    if(adapterActive())activeModule_->show(screen);
    emit changed();
}

QVariantList ShellController::gameHistory() const {
    QVariantList rows;
    for(const auto& session:repository_.gameSessions(currentAdventureId())) {
        QString status=session.outcome==PlaySessionOutcome::Running?"Playing":session.outcome==PlaySessionOutcome::Failed?"Ended with an error":session.outcome==PlaySessionOutcome::Interrupted?"Interrupted":"Finished";
        rows.append(QVariantMap{{"id",session.id},{"adventure",session.adventureId},
            {"time",session.startedAt.toLocalTime().toString("dd MMM yyyy · HH:mm")},
            {"date",session.startedAt.toLocalTime().toString("dd MMM yyyy")},
            {"clock",session.startedAt.toLocalTime().toString("HH:mm")},
            {"ended",session.endedAt.isValid()?session.endedAt.toLocalTime().toString("dd MMM yyyy · HH:mm"):QString()},
            {"outcome",session.outcome==PlaySessionOutcome::Returned?"returned":session.outcome==PlaySessionOutcome::Running?"running":session.outcome==PlaySessionOutcome::Failed?"failed":"interrupted"},
            {"duration",session.elapsedSeconds?recordedDuration(*session.elapsedSeconds):session.outcome==PlaySessionOutcome::Running?QString("Session in progress"):QString("Duration unavailable")},
            {"status",status}});
    }
    return rows;
}
QStringList ShellController::faceNames() const {
    if(page_==GamesPage && !collectionsRoot_) {QStringList names;for(const auto& row:gamesRootZone_=="platforms"?gamesPlatforms():gamesCollections())names.append(row.toMap()["name"].toString());return names;}
    if(page_==GameHubPage) {
        QStringList result{"My Story","RetroAchievements"};
        if(experience_.hasAdapter())result.append(experience_.descriptor().adapter.label);
        result.append("Lobby");return result;
    }
    if(page_==SocialPage)return {"Messages","Communities","Discover"};
    if(page_==AppsPage)return {"Apps","Crates","Browser","Files"};
    return {};
}
int ShellController::faceIndex() const {
    if(page_==GamesPage) {
        return gamesRootZone_=="platforms"?gamesPlatformIndex():gamesCollectionIndex();
    }
    if(page_==GameHubPage)return hubFaceIds().indexOf(hubFace_);
    if(page_==SocialPage)return socialFace_=="communities"?1:socialFace_=="friends" || socialFace_=="recent"?2:0;
    if(page_==AppsPage)return QStringList{"apps","crates","browser","files"}.indexOf(appsFace_);
    return 0;
}
QString ShellController::trainerFace() const {return hubFace_=="ra"?QString("ra"):experience_.screen();}
void ShellController::showTrainerFace(const QString& face) {
    if(face=="ra")setHubFace("ra");else {setHubFace("adapter");selectAdapterScreen(face);}
}
void ShellController::goToTrainerFace(const QString& face) {
    syncExperience();
    if(navigationLocked() || (face!="ra" && !experience_.screenIds().contains(face)))return;
    goToPage(GameHubPage);showTrainerFace(face);
}
void ShellController::refreshExperience() {
    syncExperience();
    hall_.setCurrentAdventure(currentAdventureId());
    hall_.setProgress(progress_?progress_->adventureId():QString(),progress_?progress_->snapshot():GameProgress{});
    ExperienceContext context{trainer_.profile()["id"].toString(),currentAdventureId(),homeAdventure(),trainer_.profile(),repository_.home(),progress_,experience_.generation()};
    for(auto& module:modules_)module->refresh(context);
}

void ShellController::refreshContinue() {
    const auto recentId=points_.value(drawerFocus_).adventureId;
    const auto favoriteId=favorites_.value(favoriteFocus_).adventureId;
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
    drawerFocus_=0;
    for(int i=0;i<points_.size();++i)if(points_[i].adventureId==recentId)drawerFocus_=i;
    favorites_.clear();
    auto games=repository_.adventures();
    std::sort(games.begin(),games.end(),[](const auto& a,const auto& b){
        const auto order=a.title.compare(b.title,Qt::CaseInsensitive);return order?order<0:a.id<b.id;
    });
    QSet<QString> seen;
    for(const auto& game:games) {
        const auto registration=repository_.registration(game.id);
        if(game.collectionOnly || !registration || registration->removed || seen.contains(game.id)
            || !gameFavorite(repository_.artwork(game.id)))continue;
        seen.insert(game.id);favorites_.append({"favorite:"+game.id,game.id,{},{},{}});
    }
    favoriteFocus_=favoriteSelectionLost_?-1:0;
    if(!favoriteId.isEmpty()) {
        favoriteFocus_=-1;
        for(int i=0;i<favorites_.size();++i)if(favorites_[i].adventureId==favoriteId)favoriteFocus_=i;
        favoriteSelectionLost_=favoriteFocus_<0;
    }
}
void ShellController::setDrawerSource(const QString& source) {
    if(!drawerOpen_ || navigationLocked() || homeMenuOpen_ || keyboard_.isOpen() || menuOpen_ || !notice_.isEmpty() || localModalOpen())return;
    if(source!="recent" && source!="favorites")return;
    if(drawerFavorites_==(source=="favorites"))return;
    drawerFavorites_=source=="favorites";
    emit drawerSourceChanged();emit changed();
}
void ShellController::setDrawerScroll(qreal position) {
    if(drawerOpen_ && std::isfinite(position))drawerScroll_[drawerFavorites_?1:0]=std::max(qreal(0),position);
}
void ShellController::selectDrawerGame(const QString& id) {
    if(!drawerOpen_ || navigationLocked() || homeMenuOpen_ || menuOpen_ || keyboard_.isOpen() || !notice_.isEmpty() || localModalOpen())return;
    const auto& entries=drawerEntries();
    for(int i=0;i<entries.size();++i)if(entries[i].id==id){drawerSelection()=i;if(drawerFavorites_)favoriteSelectionLost_=false;confirm();emit changed();return;}
}


int ShellController::focusIndex() const {
    if (!notice_.isEmpty()) return 0;
    if (menuOpen_) return menuFocus_;
    if (keyboard_.isOpen()) return keyboard_.focusIndex();
    if (downloads_.isOpen()) return downloads_.focusIndex();
    if (scraper_.isOpen()) return scraper_.focusIndex();
    if (libraryTools_.isOpen()) return libraryTools_.focusIndex();
    if (drawerOpen_) return drawerSelection();
    if (service_ == "library") return libraryManager_.files()->isOpen() ? libraryManager_.files()->focusIndex() : libraryManager_.focusIndex();
    if (service_ == "trainer-settings") return hall_.account()->isOpen() ? hall_.account()->focusIndex() : trainerSettingsFocus_;
    if (service_ == "trainer-setup") return trainerSetup_.focusIndex();
    if (service_ == "settings") return hall_.account()->isOpen() ? hall_.account()->focusIndex() : trainer_.editing() ? trainer_.focusIndex() : settings_.focusIndex();
    if (service_ == "device") return device_.focusIndex();
    if (service_ == "diagnostics") return diagnostics_.focusIndex();
    if (trainer_.editing()) return trainer_.focusIndex();
    if (page_ == 1 && collectionsRoot_) return collectionFocus_;
    if (page_ == 1) return multiverseFace_ ? multiverse_.focusIndex() : worlds_.focusIndex();
    if(adapterActive() && activeModule_)return adapterMenuOpen_?adapterMenuFocus_:activeModule_->focusIndex();
    if(page_==GameHubPage)return hubFace_=="ra"?hall_.focusIndex():hubFace_=="overview" && overviewFace_=="screens"?album_.index():overviewFocus_;
    return drawerOpen_ ? drawerSelection() : 0;
}
QJsonObject ShellController::navigationState() const {
    const QStringList pages{"home","games","gamehub","social","apps"};
    auto experience=experience_;
    if(activeModule_)experience.rememberNavigation({{"module",activeModule_->navigation()},{"hall",hall_.navigationState()}});
    QJsonObject state{{"version", ShellNavigationVersion}, {"experienceVersion",2}, {"experienceNavigation",experience.state()}, {"page", pages[page_]},
            {"hubFace",hubFace_},{"adapterMenuOpen",adapterMenuOpen_},
            {"overviewFace",overviewFace_},{"appsFace",appsFace_},{"socialFace",socialFace_},
            {"homeAdventure", homeAdventureId_}, {"homeResume", homeResumeId_},
            {"homeResumeSource", homeResumeSource_.toJson()},
            {"multiverse",multiverse_.navigationState()},{"homeDomain",multiverseHome_?"multiverse":"pokemon"},
            {"worldsFace",multiverseFace_?"multiverse":"pokemon"},
            {"globalHome",true},{"homeCollection",homeCollection_},{"worldCollection",worldCollection_},
            {"collectionsRoot",collectionsRoot_},{"collectionFocus",collectionFocus_},
            {"gamesRoot",QJsonObject{{"zone",gamesRootZone_},{"collection",gamesCollectionId_},{"platform",gamesPlatformId_},
                {"collectionScroll",collectionScroll_},{"platformScroll",platformScroll_}}},
            {"resume", points_.value(drawerFocus_).id},
            {"drawer",QJsonObject{{"source",drawerSource()},{"favorite",favorites_.value(favoriteFocus_).adventureId},
                {"favoriteLost",favoriteSelectionLost_},{"recentScroll",drawerScroll_[0]},{"favoriteScroll",drawerScroll_[1]}}},
            {"worlds", worlds_.navigationState()}, {"hall", hall_.navigationState()}};
    if(activeModule_){const auto legacy=activeModule_->legacyState();for(auto it=legacy.begin();it!=legacy.end();++it)state[it.key()]=it.value();}
    return state;
}
void ShellController::restoreNavigation(const QJsonObject& state) {
    if(navigationLocked())return;
    homeMenuOpen_=quickAppsOpen_=drawerOpen_=false;
    const int version=state["version"].toInt();
    if(version<1 || version>ShellNavigationVersion) {
        drawerFavorites_=favoriteSelectionLost_=false;drawerFocus_=favoriteFocus_=0;
        drawerScroll_[0]=drawerScroll_[1]=-1;emit drawerSourceChanged();
        experience_.clear();socialFace_="chats";hubFace_="overview";appsFace_="apps";
        social_.setFace(socialFace_);refreshExperience();goToPage(HomePage);return;
    }
    if(!state.contains("experienceNavigation"))experience_.clear();
    const QStringList roots{"home","games","gamehub","social","apps"};
    const QStringList legacyV1{"home","worlds","pokedex","trainer","hall"};
    const QStringList legacyV2{"home","worlds","companions","trainer","social","apps"};
    QString target=state["page"].toString();
    if(state["page"].isDouble())
        target=(version==1?legacyV1:version==2?legacyV2:roots).value(state["page"].toInt(),"home");
    const QString oldTarget=target;
    const bool oldFirst=target=="pokedex" || target=="companions" || target=="first";
    const bool oldSecond=target=="trainer" || target=="hall" || target=="history" || target=="second";
    hubFace_=version>=3?state["hubFace"].toString("overview"):"overview";
    overviewFace_=state["overviewFace"].toString("summary");
    if(!QStringList{"summary","timeline","screens"}.contains(overviewFace_))overviewFace_="summary";
    appsFace_=state["appsFace"].toString("apps");
    if(!QStringList{"apps","crates","browser","files"}.contains(appsFace_))appsFace_="apps";
    if(target=="worlds" || target=="collections" || target=="multiverse")target="games";
    if(oldFirst || oldSecond){target="gamehub";hubFace_="adapter";}
    socialFace_=state["socialFace"].toString("chats");
    if(!QStringList{"chats","messages","groups","communities","friends","discover","lobby","recent"}.contains(socialFace_))socialFace_="chats";
    if(socialFace_=="messages")socialFace_="chats";
    if(socialFace_=="discover")socialFace_="friends";
    if(target=="social" && socialFace_=="lobby"){target="gamehub";hubFace_="lobby";socialFace_="chats";}
    const bool legacyGroups=socialFace_=="groups";
    // Provider still owns conversation IDs/drafts. Group presentation migration
    // does not create a second message store or discard its selected channel.
    if(legacyGroups)socialFace_="communities";
    goToPage(qMax(0,int(roots.indexOf(target))));
    homeAdventureId_=state["homeAdventure"].toString();homeResumeId_=state["homeResume"].toString();
    homeResumeSource_=ResumeSource::fromJson(state["homeResumeSource"].toObject());
    if(repository_.editable()){homeResumeId_.clear();homeResumeSource_={};}
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
    const auto rootState=state["gamesRoot"].toObject();
    gamesRootZone_=rootState["zone"].toString("collections");
    if(!QStringList{"collections","platforms"}.contains(gamesRootZone_))gamesRootZone_="collections";
    gamesCollectionId_=rootState["collection"].toString(collections().value(collectionFocus_).toMap()["id"].toString());
    gamesPlatformId_=rootState["platform"].toString(state["multiverse"].toObject()["system"].toString());
    collectionScroll_=std::max(0.0,rootState["collectionScroll"].toDouble());
    platformScroll_=std::max(0.0,rootState["platformScroll"].toDouble());
    if(rootState.isEmpty() && worldCollection_=="multiverse") {
        gamesRootZone_="platforms";
        if(state["multiverse"].toObject()["route"]!="games")collectionsRoot_=true;
    }
    if(!state["globalHome"].toBool() && multiverseHome_) {
        const auto legacy=state["multiverse"].toObject();
        const auto scoped=legacy["collections"].toObject()[homeCollection_].toObject();
        homeAdventureId_=scoped.value("selected").toString(legacy["selected"].toString());
        homeResumeId_.clear();homeResumeSource_={};
    }
    if(!liveGame_.value("id").toString().isEmpty())homeAdventureId_=liveGame_.value("id").toString();
    multiverseFace_=true;
    if(page_==GamesPage)multiverse_.setCollection(worldCollection_);
    refreshExperience();
    hall_.restoreNavigation(state["hall"].toObject());
    // Screen identity belongs to the owner/game/module-keyed navigation, never
    // an unscoped route field copied when another game becomes selected.
    QString requested;
    if(state.contains("experienceNavigation")) {
        experience_.restore(state["experienceNavigation"].toObject());
        const auto navigation=experience_.navigation();
        activeModule_->restoreNavigation(navigation.contains("module")?navigation["module"].toObject()
            :navigation.contains(experience_.descriptor().id)?navigation.value(experience_.descriptor().id).toObject():navigation);
        if(navigation.contains("hall"))hall_.restoreNavigation(navigation["hall"].toObject());
        if(version<3 && (oldFirst || oldSecond))requested=experience_.legacyScreen(oldSecond?1:0);
    } else if(activeModule_) {
        const auto legacy=activeModule_->restoreLegacy(state);
        if(oldFirst || oldSecond)requested=legacy.value(oldSecond?1:0);
    }
    if(version<3) {
        if(oldSecond) {
            if(requested.isEmpty())requested=state["trainerFace"].toString("profile");
            if(oldTarget=="hall")requested=QStringList{"journey","hall","ra"}.value(hall_.faceIndex(),"journey");
            if(requested=="ra")hubFace_="ra";
            else if(!experience_.hasAdapter()){hubFace_="overview";overviewFace_="timeline";}
        } else if(oldFirst) {
            if(requested.isEmpty())requested=state["pokemonFace"].toString("dex");
            if(!experience_.hasAdapter())hubFace_="overview";
        }
    }
    if(!requested.isEmpty())experience_.show(requested);
    if(!hubFaceIds().contains(hubFace_))hubFace_="overview";
    adapterMenuOpen_=version>=3?state["adapterMenuOpen"].toBool(true):false;
    adapterMenuFocus_=qMax(0,experience_.index());
    drawerFocus_=0;
    for(int i=0;i<points_.size();++i)if(points_[i].id==state["resume"].toString())drawerFocus_=i;
    const auto drawer=state["drawer"].toObject();drawerFavorites_=drawer["source"]=="favorites";
    favoriteSelectionLost_=drawer["favoriteLost"].toBool();favoriteFocus_=favoriteSelectionLost_?-1:0;
    const auto favoriteId=drawer["favorite"].toString();
    if(!favoriteId.isEmpty()) {
        favoriteFocus_=-1;for(int i=0;i<favorites_.size();++i)if(favorites_[i].adventureId==favoriteId)favoriteFocus_=i;
        favoriteSelectionLost_=favoriteFocus_<0;
    }
    drawerScroll_[0]=std::max(-1.,drawer["recentScroll"].toDouble(-1));
    drawerScroll_[1]=std::max(-1.,drawer["favoriteScroll"].toDouble(-1));
    emit drawerSourceChanged();
    refreshExperience(); // restore advances the generation; all callbacks bind it again.
    if(page_==GameHubPage)setHubFace(hubFace_);
    else if(page_==SocialPage)social_.setFace(socialFace_);
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
    const auto progress=activeModule_?activeModule_->homeProgress():QVariantMap();
    std::optional<qint64> seconds;
    if (adventure) {
        title = adventure->title;
        for (const auto& w : repository_.worlds()) if (w.id == adventure->worldId) world = w.name;
        seconds = repository_.recordedSeconds(adventure->id);
        const auto resumeStatus = homeResumeAvailability(*adventure);
        action = resumeStatus == ResumeAvailability::Exact ? "Resume Adventure" : "Start Adventure";
        actionHint = resumeStatus == ResumeAvailability::Exact ? "Resume" : "Play";
        if (liveGame_.value("id") == adventure->id) { action = "Return to game"; actionHint = "Return"; }
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
    QVariantMap result{{"trainer", trainer_.exists() ? trainer_.profile()["name"] : "TRAINER"},
            {"hasTrainer", trainer_.exists()}, {"adventure", title}, {"world", world},
            {"adventureId", adventure ? adventure->id : QString()}, {"action", action}, {"actionHint", actionHint},
            {"exitPreview", media ? "image://exit-media/" + media->sessionId : adventure ? repository_.artwork(adventure->id).value("cover").toString() : QString()},
            {"exitPreviewLabel", media ? "Last exit · " + media->capturedAt.toLocalTime().toString("dd MMM · HH:mm") : QString()},
            {"recordedTime", seconds ? recordedDuration(*seconds) : "—"},
            {"sessionCount",adventure?repository_.gameSessionCount(adventure->id):qint64(0)}, {"milestone", milestone}};
    for(auto it=progress.begin();it!=progress.end();++it)if(!result.contains(it.key()))result.insert(it.key(),it.value());
    return result;
}
QVariantList ShellController::resumePoints() const { return continueRows(points_,false); }
QVariantList ShellController::drawerGames() const { return continueRows(drawerEntries(),drawerFavorites_); }
QVariantList ShellController::continueRows(const QList<ContinueEntry>& entries, bool favorites) const {
    PerformanceTrace::Scope perf("ShellController.continueRows");

    QVariantList result;
    const auto adventures = repository_.adventures();
    const auto worlds = repository_.worlds();
    for (const auto& point : entries) {
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
        if(favorites)if(const auto registration=repository_.registration(point.adventureId);registration && !registration->contentAvailable)
            summary="Game file unavailable";
        result.append(QVariantMap{{"id", point.id}, {"title", title}, {"world", world},
            {"location", location}, {"summary", summary}, {"previewLabel", media ? "Last exit · " + media->capturedAt.toLocalTime().toString("dd MMM · HH:mm") : point.resumePoint ? resumeLabel(status) : favorites ? "Favorite game" : "Recent Adventure"},
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
    homeMenuOpen_ = false;dashboardReturn_=false;
    if(adapterActive() && activeModule_)
        experience_.rememberNavigation({{"module",activeModule_->navigation()},{"hall",hall_.navigationState()}});
    for(auto& module:modules_)module->leave(page!=page_);
    collectionManager()->close();
    { PerformanceTrace::Scope phase("navigation.tools"); libraryTools_.close(); }
    { PerformanceTrace::Scope phase("navigation.storage"); settings_.storage()->close(); }
    { PerformanceTrace::Scope phase("navigation.keyboard"); if(textTarget_==TextTarget::Social)social_.preserveText(keyboard_.text()); keyboard_.cancel(); }
    if(textModule_){textModule_->cancelText();textModule_=nullptr;}
    textTarget_ = TextTarget::None;
    apps_.leave(true);
    social_.closeMenu();
    { PerformanceTrace::Scope phase("navigation.archive"); hall_.editor()->cancel(); }
    { PerformanceTrace::Scope phase("navigation.account"); hall_.account()->close(); }
    { PerformanceTrace::Scope phase("navigation.trainer"); trainer_.cancel(); }
    { PerformanceTrace::Scope phase("navigation.setup"); trainerSetup_.close(); }
    { PerformanceTrace::Scope phase("navigation.manager"); libraryManager_.close(); service_.clear(); }
    page_ = std::clamp(page, 0, int(primaryNames().size())-1); // No wrapping until physical-device testing.
    quickAppsOpen_=false;
    if(page_==AppsPage)apps_.refresh();
    if(page_==SocialPage)social_.setFace(socialFace_);
    if(page_==1 && multiverseFace_)multiverse_.setCollection(worldCollection_);
    // Primary tabs resume their existing route and focus. Only explicit Back
    // or a destination action goes to a root; switching tabs is not navigation up.
    if (page_ == 1) repository_.refreshContentAvailability();
    // Party observes the current Adventure, library and save provider directly.
    // Visiting an unrelated page must not republish the same actors/practice UI.
    syncExperience();
    if(page_==GameHubPage) {
        setHubFace(hubFace_);
        if(adapterActive() && activeModule_)activeModule_->restoreNavigation(experience_.navigation()["module"].toObject());
    }
    drawerOpen_ = false;
    menuOpen_ = false;
    if (powerMenu_) menuFocus_ = 6;
    powerMenu_ = false;
    notice_.clear();
    mode_.clear();
    transition.unblock();
    { PerformanceTrace::Scope phase("navigation.publish"); emit changed(); }
}
QVariantList ShellController::gamesCollections() const {
    QVariantList rows;
    for(const auto& row:collections()) {
        const auto id=row.toMap()["id"].toString();
        if(id!="multiverse" && id!="auto:favorites")rows.append(row);
    }
    return rows;
}
int ShellController::gamesCollectionIndex() const {
    const auto rows=gamesCollections();
    if(gamesCollectionId_=="_create")return rows.size();
    for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==gamesCollectionId_)return i;
    return 0;
}
int ShellController::gamesPlatformIndex() const {
    const auto rows=gamesPlatforms();
    for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==gamesPlatformId_)return i;
    return 0;
}
void ShellController::focusGamesRootItem(const QString& zone,const QString& id) {
    if(page_!=GamesPage || !collectionsRoot_ || navigationLocked() || !pairedNavigationAvailable())return;
    const auto rows=zone=="collections"?gamesCollections():gamesPlatforms();
    bool found=zone=="collections" && id=="_create";
    for(const auto& row:rows)if(row.toMap()["id"]==id)found=true;
    if(!found || (zone!="collections" && zone!="platforms"))return;
    gamesRootZone_=zone;
    if(zone=="collections")gamesCollectionId_=id;else gamesPlatformId_=id;
    emit changed();
}
void ShellController::openGamesRootItem(const QString& zone,const QString& id) {
    if(page_!=GamesPage || navigationLocked() || !pairedNavigationAvailable())return;
    if(zone=="collections") {
        if(id=="_create") {gamesRootZone_=zone;gamesCollectionId_=id;manageCollection(true);emit changed();return;}
        for(int i=0;i<collections().size();++i)if(collections()[i].toMap()["id"]==id && id!="multiverse" && id!="auto:favorites") {
            gamesRootZone_=zone;gamesCollectionId_=id;openCollection(i);return;
        }
    } else if(zone=="platforms") {
        for(const auto& row:gamesPlatforms())if(row.toMap()["id"]==id) {
            gamesRootZone_=zone;gamesPlatformId_=id;worldCollection_="multiverse";multiverseFace_=true;collectionsRoot_=false;
            multiverse_.openSystem(id);emit changed();return;
        }
    }
}
void ShellController::openCollection(int index) {
    const auto list=collections();if(index<0 || index>=list.size())return;
    const bool fromRoot=collectionsRoot_;
    collectionFocus_=index;worldCollection_=list[index].toMap()["id"].toString();
    gamesRootZone_="collections";gamesCollectionId_=worldCollection_;
    multiverseFace_=true;collectionsRoot_=false;
    if(multiverseFace_){multiverse_.setCollection(worldCollection_);if(fromRoot)multiverse_.showSystems();}
    else if(fromRoot)worlds_.showRegions();
    emit changed();
}
void ShellController::editCollection(int index) {
    if(index<0 || index>=collections().size() || !collectionsRoot_ || !pairedNavigationAvailable())return;
    collectionFocus_=index;openContext("collection",collections().value(index).toMap()["id"].toString());
}
void ShellController::openContext(const QString& type,const QString& target) {
    if(navigationLocked() || homeMenuOpen_ || menuOpen_ || serviceOpen() || keyboard_.isOpen() || localModalOpen() || !notice_.isEmpty())return;
    if(type.isEmpty() && page_==HomePage && dashboard_.focus()!="play")return;
    if(type.isEmpty() && page_==GamesPage && collectionsRoot_) {
        if(gamesRootZone_=="collections") {
            if(gamesCollectionId_=="_create")manageCollection(true);
            else collectionManager()->begin(gamesCollections().value(gamesCollectionIndex()).toMap()["id"].toString());
        }
        return;
    }
    if(type=="collection" || (type.isEmpty() && page_==1 && (collectionsRoot_ || multiverse_.route()!="games"))) {
        const auto id=target.isEmpty()?(collectionsRoot_?collections().value(collectionFocus_).toMap()["id"].toString():worldCollection_):target;
        if(page_==1 && collectionManager()->known(id))collectionManager()->begin(id);
        return;
    }
    if(type=="game" || page_==0 || page_==1) {
        const auto id=target.isEmpty()?(page_==1?multiverse_.detail()["id"].toString():currentAdventureId()):target;
        if(page_==GamesPage)libraryTools_.beginBrowsing(id,multiverseFace_?multiverse_.viewStyle():QString());else libraryTools_.beginGame(id);return;
    }
    // Feature controllers own their captured person/message/entity targets.
    // This is the same dispatcher used by Select, with their existing guards.
    if(page_==AppsPage && appsFace_=="apps")apps_.dispatch(Action::LocalAction);
    else if(page_==SocialPage)social_.dispatch(Action::LocalAction);
    else if(page_==GameHubPage)dispatchGameHub(Action::LocalAction);
}
void ShellController::focusLibraryGame(int index) {
    if(page_!=GamesPage || collectionsRoot_ || !multiverseFace_ || !pairedNavigationAvailable()
       || navigationLocked() || localModalOpen() || keyboard_.isOpen() || !notice_.isEmpty()
       || social_.online()["open"].toBool() || moduleInvitation()["open"].toBool())return;
    multiverse_.focusGame(index);
}
void ShellController::libraryAction(const QString& action) {
    if(page_!=GamesPage || collectionsRoot_ || navigationLocked() || localModalOpen()
       || keyboard_.isOpen() || drawerOpen_ || quickAppsOpen_ || homeMenuOpen_ || menuOpen_ || serviceOpen() || !notice_.isEmpty()
       || social_.online()["open"].toBool() || moduleInvitation()["open"].toBool())return;
    if(multiverseFace_ && action.startsWith("view:")){multiverse_.setViewStyle(action.mid(5));return;}
    if(action!="search" && action!="filter")return;
    const auto input=action=="search"?Action::Secondary:Action::ToggleContinue;
    if(multiverseFace_)multiverse_.dispatch(input);else worlds_.dispatch(input);
}
void ShellController::toggleLibraryFavorite(const QString& id) {
    if(page_!=GamesPage || collectionsRoot_ || !multiverseFace_ || multiverse_.route()!="games"
       || favoriteWriting_ || navigationLocked() || localModalOpen() || keyboard_.isOpen()
       || drawerOpen_ || quickAppsOpen_ || homeMenuOpen_ || menuOpen_ || serviceOpen() || !notice_.isEmpty()
       || social_.online()["open"].toBool() || moduleInvitation()["open"].toBool())return;
    bool visible=false;
    for(const auto& row:multiverse_.games())if(row.toMap()["id"].toString()==id){visible=true;break;}
    const auto record=repository_.registration(id);
    if(!visible || !record || record->removed || record->adventure.collectionOnly)return;
    const bool next=!gameFavorite(repository_.artwork(id));
    favoriteWriting_=true;
    repository_.setFavoriteAsync(id,next,this,[this](const QString& error){
        favoriteWriting_=false;
        if(!error.isEmpty())showNotice(error);
        else refreshLibrary();
    });
}
void ShellController::pressButton(const QString& button) {
    static const QMap<QString,Action> actions{{"A",Action::Confirm},{"B",Action::Back},
        {"X",Action::Secondary},{"Y",Action::ToggleContinue},{"Select",Action::LocalAction},
        {"Home",Action::Home},{"Start",Action::SystemMenu},{"R3",Action::ChangeView},{"L3",Action::ToggleFavorite},
        {"L2",Action::PreviousFace},{"R2",Action::NextFace}};
    const auto found=actions.constFind(button);if(found!=actions.cend())dispatch(*found);
}
void ShellController::manageCollection(bool create) {
    if(page_!=1 || navigationLocked() || menuOpen_ || serviceOpen() || keyboard_.isOpen() || localModalOpen() || !notice_.isEmpty())return;
    const auto id=collectionsRoot_?gamesCollections().value(gamesCollectionIndex()).toMap()["id"].toString():worldCollection_;
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
    if(launchPreparation_.busy() || libraryTools_.busy() || moduleWriting())return;
    if(area=="world-edit" && canEditWorld()){libraryTools_.beginWorld(worlds_.region().value("id").toString(),true);return;}
    if (area == "continue") { dispatch(Action::ToggleContinue); return; }
    if (!notice_.isEmpty()) { confirm(); emit changed(); return; }
    if (menuOpen_) menuFocus_ = std::clamp(index, 0, int(menuItems().size()) - 1);
    else if (keyboard_.isOpen()) { keyboard_.activate(index); return; }
    else if (libraryTools_.isOpen()) {libraryTools_.activate(index);return;}
    else if (drawerOpen_) {
        auto& focus = drawerSelection();
        focus = std::clamp(index, 0, std::max(0, int(drawerEntries().size()) - 1));
        if(drawerFavorites_)favoriteSelectionLost_=false;
    }
    else if (service_ == "library") { libraryManager_.activate(index, area); return; }
    else if (service_ == "trainer-settings") { trainerSettingsAction(index); return; }
    else if (service_ == "trainer-setup") { trainerSetup_.activate(index); return; }
    else if(service_=="profile") {if(trainer_.editing())trainer_.activate(index);else trainer_.beginEdit();return;}
    else if (service_ == "settings") { if(hall_.account()->isOpen()) { hall_.account()->activate(index); return; } if(trainer_.editing()) { trainer_.activate(index); return; } if(settings_.controlsFocused()) {if(settings_.category()==10)network_.activate(index);else settings_.activateRow(index);} else settings_.selectCategory(index,true); return; }
    else if (service_ == "device") { device_.activate(index); return; }
    else if (service_ == "diagnostics") { diagnostics_.activate(index); return; }
    else if (trainer_.editing()) { trainer_.activate(index); return; }
    else if(page_==0 && area.startsWith("experience-home:") && activeModule_) {
        activeModule_->activateHome(index,area.mid(16));return;
    }
    else if (page_ == 1) {
        if(collectionsRoot_){openCollection(index);return;}
        if (multiverseFace_) { multiverse_.activate(index); return; }
        if (area == "worlds-search") worlds_.dispatch(Action::Secondary);
        else if (area == "worlds-filter") worlds_.dispatch(Action::ToggleContinue);
        else worlds_.activate(index);
        return;
    }
    else if(page_==GameHubPage) {
        if(hubFace_=="ra"){if(area.isEmpty())hall_.activate(index);else hall_.activateControl(area,index);return;}
        if(hubFace_=="lobby"){social_.activate(index);return;}
        if(adapterActive() && activeModule_){activeModule_->activate(index,area);return;}
        if(overviewFace_=="screens" && area=="overview-media"){album_.select(index);return;}
        overviewFocus_=std::clamp(index,0,qMax(0,int(overviewFace_=="timeline"?storyHistory().size():overviewFace_=="screens"?storyMedia().size():2)-1));
        if(area=="overview-timeline")showOverview("timeline");
        else if(area=="overview-screens")showOverview("screens");
        emit changed();return;
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
            if(menuFocus_ != 0)for(auto& module:modules_)module->leave(false);
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
        const auto& entries=drawerEntries();
        if(entries.isEmpty())return;
        if(drawerSelection()<0 || drawerSelection()>=entries.size())return;
        const auto point=entries.at(drawerSelection());
        if(drawerFavorites_) {
            const auto registration=repository_.registration(point.adventureId);
            if(!registration || registration->removed || !gameFavorite(repository_.artwork(point.adventureId))) {
                refreshContinue();favoriteFocus_=-1;favoriteSelectionLost_=true;return;
            }
        }
        if(!liveGame_.value("id").toString().isEmpty() && liveGame_.value("id")!=point.adventureId) {
            notice_="Your running game stays selected. Return to it or exit from Game Options first.";return;
        }
        for (const auto& adventure : repository_.adventures()) if (adventure.id == point.adventureId) {
            homeAdventureId_ = adventure.id; homeResumeId_ = point.resumePoint ? point.resumePoint->id : QString();
            homeResumeSource_ = point.resumePoint ? point.resumePoint->source : ResumeSource{};
            drawerOpen_ = false;
            refreshExperience();if(adapterActive() && activeModule_)showExperienceScreen(experience_.screen());
            return;
        }
        notice_ = "This Adventure is unavailable. Its history has been kept.";
    } else if (page_ == 0) {
        if(dashboard_.focus()!="play"){activateDashboard(dashboard_.focus());return;}
        if(!liveGame_.isEmpty()) {
            if(liveGame_.value("id")==currentAdventureId())returnToLiveGame();
            else showNotice("A game is still running. Return to it or exit it from Game Options before starting another.");
            return;
        }
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
    else if (page_==GameHubPage) {
        dispatchGameHub(Action::Confirm);
    } else if (page_ == SocialPage) {
        social_.dispatch(Action::Confirm); return;
    } else {
        notice_ = "This section is not available in the prototype yet.";
    }
}
void ShellController::closeHomeMenu() {
    if (!homeMenuOpen_ || navigationLocked()) return;
    homeMenuOpen_ = false; emit changed();
}
void ShellController::openAccountProfile() {
    if(!canReceiveNearby() || quickAppsOpen_ || moduleWriting() || !apps_.menu().isEmpty())return;
    service_="account-profile";settings_.communication()->beginProfile();emit changed();
}
void ShellController::openProfile() {
    if(navigationLocked() || moduleWriting() || keyboard_.isOpen() || trainer_.saving())return;
    homeMenuOpen_=false;service_="profile";trainer_.reload();emit changed();
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
        QVariantMap{{"id","chats"},{"label","Chats"}},QVariantMap{{"id","notifications"},{"label","Notifications · "+QString::number(notifications().size())}}};
    rows.append(QVariantMap{{"id","profile"},{"label","Your profile"},{"detail",trainer_.profile()["name"]}});
    rows.append(QVariantMap{{"id","together"},{"label","Play together"}});
    rows.append(QVariantMap{{"id","lobby"},{"label",social_.lobbyState().isEmpty()?"Lobbies":"My lobby"}});
    if(!liveGame_.isEmpty()) {
        rows.prepend(QVariantMap{{"id","live-options"},{"label","Game Options"},{"detail",liveGame_.value("title")}});
        rows.prepend(QVariantMap{{"id","live-return"},{"label","Return to game"},{"detail","Still running · "+liveGame_.value("title").toString()}});
    }
    if(!voice["channel"].toString().isEmpty())rows.append(QVariantMap{{"id","call"},{"label","Voice call"},{"detail",voice["name"]}});
    if(!social_.gameParty()["party"].toString().isEmpty()||social_.gameParty()["joining"].toBool())rows.append(QVariantMap{{"id","game-party"},{"label","Game party"},{"detail",social_.gameParty()["game"].toMap()["label"]}});
    const bool answering=homeMenuSelection_.startsWith("answer-call:");
    const bool declining=homeMenuSelection_.startsWith("decline-call:");
    rows.append(social_.incomingCallActions(answering?homeMenuSelection_.section(':',1):QString()));
    if(homeMenuOpen_&&!homeMenuSelection_.isEmpty()) {
        const bool present=std::any_of(rows.cbegin(),rows.cend(),[this](const QVariant& row){return row.toMap()["id"]==homeMenuSelection_;});
        // Never redirect a pending A press onto Home when an asynchronous
        // update removes the selected invitation. Navigation clears this row.
        if(!present)rows.append(QVariantMap{{"id",homeMenuSelection_},{"label",answering||declining?"Call no longer ringing":"Action no longer available"},{"enabled",false}});
    }
    return rows;
}
QString ShellController::homeMenuCaption() const {
    if(!homeCallOpen_)return trainer_.profile()["name"].toString();
    const auto voice=social_.account()["voice"].toMap();
    return voice["summary"].toString().isEmpty()?voice["status"].toString():voice["summary"].toString();
}
void ShellController::activateHomeMenu(int index) {
    if (!homeMenuOpen_ || navigationLocked() || index < 0 || index >= homeMenuActions().size()) return;
    const auto selected=homeMenuActions()[index].toMap();
    if(!selected.value("enabled",true).toBool())return;
    const auto id=selected["id"].toString();
    if(id=="together"){emit togetherRequested(liveGame_.value("id").toString());return;}
    if(id=="lobby"){if(liveGame_.isEmpty())social_.openLobby();else social_.openLobbyForGame(liveGame_.value("id").toString());return;}
    if(id=="live-return"||id=="live-options"){returnToLiveGame(id=="live-options");return;}
    if(id=="profile"){openProfile();return;}
    if(id=="game-party"){closeHomeMenu();goToPage(SocialPage);emit social_.runtimeAction("multiplayer-party");return;}
    if(id.startsWith("answer-call:")||id.startsWith("decline-call:")) {
        const bool accept=id.startsWith("answer-call:");
        if(social_.answerCall(id.section(':',1),accept,true)&&accept)closeHomeMenu();
        emit changed();return;
    }
    if(homeCallOpen_){if(id=="back"){homeCallOpen_=false;homeMenuSelection_="home";}else social_.controlCall(id);emit changed();return;}
    if(id=="call"){homeCallOpen_=true;homeMenuSelection_="voice-mute";emit changed();return;}
    if(id=="notifications"){notificationsOpen_=true;notificationFocus_=0;notificationSelection_=notifications().value(0).toMap()["id"].toString();emit changed();return;}
    if(id=="chats"&&!social_.notificationFace().isEmpty()){openSocialNotification();return;}
    if (id!="home") socialFace_ = "chats";
    social_.setFace(socialFace_);
    goToPage(id=="home" ? HomePage : SocialPage);
    if(id=="friends")social_.showContacts();
}
void ShellController::activateHomeAction(const QString& id) {
    const auto actions=homeMenuActions();
    for(int i=0;i<actions.size();++i)if(actions[i].toMap()["id"].toString()==id){activateHomeMenu(i);return;}
}
int ShellController::notificationFocus() const {
    const auto rows=notifications();
    for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"].toString()==notificationSelection_)return i;
    return -1;
}
void ShellController::activateNotification(int index) {
    if(!notificationsOpen()||navigationLocked())return;
    if(index<0 || index>=notifications().size())return;
    const auto row=notifications().value(index).toMap();
    if(row["provider"]=="achievements"){showNotice(row["name"].toString()+"\n"+row["detail"].toString());closeHomeMenu();return;}
    if(row["answerable"].toBool()) {
        notificationsOpen_=false;homeCallOpen_=false;homeMenuSelection_="answer-call:"+row["id"].toString();emit changed();
        return;
    }
    const auto face=social_.notificationFaceAt(index);if(face.isEmpty())return;
    if(face=="lobby")openGameHubFace("lobby");else {socialFace_=face;goToPage(SocialPage);}
    social_.openNotificationAt(index);emit changed();
}
void ShellController::openNotificationById(const QString& id) {
    if(!homeMenuOpen_ || navigationLocked() || id.isEmpty())return;
    const auto rows=notifications();
    for(int index=0;index<rows.size();++index)if(rows[index].toMap()["id"].toString()==id) {
        notificationsOpen_=true;notificationFocus_=index;notificationSelection_=id;activateNotification(index);emit changed();return;
    }
}
void ShellController::dismissNotifications() {
    achievementNotifications_.clear();achievementToast_.clear();++achievementToastGeneration_;
    social_.dismissNotifications();emit changed();
}
void ShellController::openSocialNotification() {
    if(navigationLocked()||(!homeMenuOpen_&&!canReceiveNearby()))return;
    const auto face=social_.notificationFace();if(face.isEmpty())return;
    if(face=="lobby")openGameHubFace("lobby");else {socialFace_=face;goToPage(SocialPage);}
    social_.openNotification();emit changed();
}
void ShellController::dispatch(Action action) {
    if(action==Action::ToggleFavorite) {
        if(page_==GameHubPage && hubFace_=="overview" && overviewFace_=="screens") {
            if(pairedNavigationAvailable() && !navigationLocked() && !album_.filtering())album_.toggleFavorite();
        } else toggleLibraryFavorite(multiverse_.detail().value("id").toString());
        return;
    }
    if(action==Action::ChangeView) {
        if(page_==GameHubPage && hubFace_=="overview" && overviewFace_=="screens") {
            if(pairedNavigationAvailable() && !navigationLocked())album_.switchLibrary();
            return;
        }
        libraryAction(multiverse_.viewStyle()=="wheel"?"view:list":multiverse_.viewStyle()=="list"?"view:shelves":multiverse_.viewStyle()=="shelves"?"view:grid":"view:wheel");return;
    }
    if(action==Action::ScrollUp || action==Action::ScrollDown) {
        if(page_==HomePage && pairedNavigationAvailable() && !navigationLocked()
           && !social_.online()["open"].toBool() && !moduleInvitation()["open"].toBool())
            emit dashboard_.scrollRequested(action==Action::ScrollDown?1:-1);
        else if(page_==GamesPage && !collectionsRoot_ && multiverseFace_ && multiverse_.route()=="games"
                && pairedNavigationAvailable() && !navigationLocked()
                && !social_.online()["open"].toBool() && !moduleInvitation()["open"].toBool())
            emit multiverse_.scrollRequested(action==Action::ScrollDown?1:-1);
        return;
    }
    if(keyboard_.isOpen()&&!menuOpen_&&!homeMenuOpen_&&notice_.isEmpty()&&!downloads_.isOpen()&&!social_.online()["open"].toBool()
       &&(action==Action::PreviousFace||action==Action::NextFace)){keyboard_.dispatch(action);return;}
    if(downloads_.isOpen()){downloads_.dispatch(action);return;}
    if(scraper_.isOpen()&&action==Action::SystemMenu&&!keyboard_.isOpen()){menuFocus_=scraper_.selectingSystems()?13:12;scraper_.hide();menuOpen_=true;emit changed();return;}
    if(social_.online()["open"].toBool()) {
        if(action==Action::Confirm)social_.answerOnline(true);
        else if(action==Action::Back)social_.answerOnline(false);
        return;
    }
    for(auto& module:modules_)if(module->intercept(action))return;
    if(scraper_.isOpen()){if(keyboard_.isOpen())keyboard_.dispatch(action);else scraper_.dispatch(action);return;}
    if(launchPreparation_.busy() || libraryTools_.busy())return;
    if(navigationLocked(action==Action::PreviousPage || action==Action::NextPage) && (action==Action::Home || action==Action::PreviousPage || action==Action::NextPage || action==Action::SystemMenu || action==Action::PreviousFace || action==Action::NextFace))return;
    if (homeMenuOpen_) {
        if(notificationsOpen_) {
            if(action==Action::Home)closeHomeMenu();
            else if(action==Action::Back){notificationsOpen_=false;emit changed();}
            else if(action==Action::Confirm)activateNotification(notificationFocus());
            else if(action==Action::Secondary) {
                const auto row=notifications().value(notificationFocus()).toMap();
                if(row["provider"]=="achievements")achievementNotifications_.removeIf([&](const QVariant& item){return item.toMap()["id"]==row["id"];});
                else social_.dismissNotificationAt(notificationFocus());
                emit changed();
            }
            else if(action==Action::SystemMenu){homeMenuOpen_=false;optionsReturn_=true;dispatch(action);}
            else if(action==Action::Left){notificationsOpen_=false;emit changed();}
            else if(action==Action::Up||action==Action::Down){notificationFocus_=std::clamp(notificationFocus()+(action==Action::Up?-1:1),0,std::max(0,int(notifications().size())-1));notificationSelection_=notifications().value(notificationFocus_).toMap()["id"].toString();emit changed();}
            return;
        }
        if(action==Action::Back&&homeCallOpen_){homeCallOpen_=false;homeMenuSelection_="home";emit changed();}
        else if (action == Action::Home || action == Action::Back) closeHomeMenu();
        else if (action == Action::Confirm) activateHomeMenu(homeMenuFocus());
        else if (action == Action::Up || action == Action::Down) {
            const auto actions=homeMenuActions();
            const int index=std::clamp(homeMenuFocus() + (action == Action::Up ? -1 : 1), 0, qMax(0,int(actions.size())-1));
            homeMenuSelection_=actions.value(index).toMap()["id"].toString();emit changed();
        } else if(action==Action::Right && !homeCallOpen_) {
            const auto rows=homeMenuActions();for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]=="notifications"){activateHomeMenu(i);break;}
        } else if (action == Action::SystemMenu) {
            homeMenuOpen_=false;optionsReturn_=true;dispatch(action);
        }
        return;
    }
    if(service_=="account-profile" && !menuOpen_ && notice_.isEmpty() && action!=Action::SystemMenu && action!=Action::Home) {
        if(keyboard_.isOpen())keyboard_.dispatch(action);
        else settings_.communication()->dispatch(action);
        return;
    }
    if(!keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && !apps_.menu().isEmpty()) {
        if(action==Action::Secondary || action==Action::ToggleContinue)apps_.menuAction(3);
        else if(action!=Action::PreviousPage && action!=Action::NextPage && action!=Action::SystemMenu && action!=Action::Home) {
            apps_.dispatch(action);return;
        }
    }
    if(!keyboard_.isOpen() && !localModalOpen() && !menuOpen_ && notice_.isEmpty() && action==Action::Secondary) {
        toggleQuickApps();return;
    }
    if(quickAppsOpen_) {
        if(action==Action::Back){quickAppsOpen_=false;apps_.leave();emit changed();return;}
        if(action!=Action::PreviousPage && action!=Action::NextPage && action!=Action::Home
           && action!=Action::SystemMenu && action!=Action::ToggleContinue
           && action!=Action::PreviousFace && action!=Action::NextFace){apps_.dispatch(action,true);return;}
        quickAppsOpen_=false;apps_.leave();
    }
    if(!menuOpen_ && !keyboard_.isOpen() && !localModalOpen() && !serviceOpen() && notice_.isEmpty() && action==Action::LocalAction) {openContext();return;}
    if(achievementPhotoReturn_ && action==Action::Back && page_==GameHubPage && hubFace_=="overview" && overviewFace_=="screens"
       && !album_.filtering() && !homeMenuOpen_ && !menuOpen_ && !serviceOpen() && notice_.isEmpty()) {
        achievementPhotoReturn_=false;setHubFace("ra");hall_.restoreNavigation(achievementPhotoNavigation_);emit changed();return;
    }
    if(dashboardReturn_ && page_==GameHubPage && hubFace_=="ra" && hall_.route()=="achievement-detail"
       && (action==Action::Back || (action==Action::Confirm && hall_.detail()["capture"].toString().isEmpty())) && !homeMenuOpen_ && !menuOpen_
       && !keyboard_.isOpen() && !serviceOpen() && notice_.isEmpty()) {
        hall_.dispatch(Action::Back); goToPage(HomePage); return;
    }
    if(action==Action::Back && dashboardReturn_ && !(page_==GameHubPage && overviewFace_=="screens" && album_.filtering()) && !keyboard_.isOpen() && !localModalOpen()
       && !menuOpen_ && !serviceOpen() && !drawerOpen_ && notice_.isEmpty()) {goToPage(HomePage);return;}
    if(action==Action::ContextMenu && canHoldConfirm()) {
        openContext("game",(multiverseFace_?multiverse_.detail():worlds_.detail()).value("id").toString());return;
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
        if(drawerOpen_) {setDrawerSource(drawerFavorites_?"recent":"favorites");return;}
        if(pairedNavigationAvailable()) {
            drawerOpen_=false;
            if(page_==HomePage){dashboard_.move(action);return;}
            const auto count=faceNames().size();
            if(count>1)selectFace((faceIndex()+(action==Action::NextFace?1:count-1))%count);
        }
        return;
    }
    if (action == Action::ToggleContinue && chooseAdventureAvailable()) {
        if(!drawerOpen_) {
            repository_.refreshContentAvailability();refreshContinue();
        }
        drawerOpen_ = !drawerOpen_; emit changed(); return;
    }
    if (notice_.isEmpty() && !menuOpen_) {
        if (keyboard_.isOpen()) {
            const bool moduleText=textTarget_==TextTarget::Module;
            const bool connecting=textTarget_==TextTarget::Network;
            if(textTarget_==TextTarget::Social && action==Action::Back)social_.preserveText(keyboard_.text());
            keyboard_.dispatch(action);
            if(moduleText && action==Action::Back) {textTarget_=TextTarget::None;if(textModule_)textModule_->cancelText();textModule_=nullptr;}
            if(connecting && action==Action::Back) {textTarget_=TextTarget::None;network_.cancelText();}
            return;
        }
        if(collectionManager()->isOpen()){collectionManager()->dispatch(action);return;}
        if (libraryTools_.isOpen()) {libraryTools_.dispatch(action);return;}
        if (drawerOpen_) {
            if (action == Action::Back) drawerOpen_ = false;
            else if (action == Action::Confirm) confirm();
            else if (action == Action::Left || action == Action::Right) {
                auto& focus = drawerSelection();
                focus = std::clamp(focus + (action == Action::Right ? 1 : -1), 0, std::max(0, int(drawerEntries().size()) - 1));
                if(drawerFavorites_)favoriteSelectionLost_=false;
            }
            emit changed(); return;
        }
        if (service_ == "library") { libraryManager_.dispatch(action); return; }
        if (service_ == "trainer-setup") { trainerSetup_.dispatch(action); return; }
        if(service_=="profile") {
            if(trainer_.editing())trainer_.dispatch(action,true);
            else if(action==Action::Back){service_.clear();homeMenuOpen_=true;homeMenuSelection_="profile";}
            else if(action==Action::Confirm)trainer_.beginEdit();
            emit changed();return;
        }
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
        if (trainer_.editing()) { trainer_.dispatch(action); return; }

        if(page_==HomePage && (action==Action::Up || action==Action::Down || action==Action::Left || action==Action::Right || action==Action::Back)) {
            dashboard_.move(action);return;
        }
        if (page_ == 1) {
            if(collectionsRoot_) {
                if(action==Action::Back){goToPage(0);return;}
                if(action==Action::Confirm) {
                    if(gamesRootZone_=="platforms")openGamesRootItem("platforms",gamesPlatforms().value(gamesPlatformIndex()).toMap()["id"].toString());
                    else openGamesRootItem("collections",gamesCollectionIndex()==gamesCollections().size()?QString("_create"):gamesCollections()[gamesCollectionIndex()].toMap()["id"].toString());
                } else if(gamesRootZone_=="platforms") {
                    const auto rows=gamesPlatforms();int i=gamesPlatformIndex();
                    if(action==Action::Up)gamesRootZone_="collections";
                    else {
                        const int delta=action==Action::Left?-1:action==Action::Right?1:0;
                        if(!rows.isEmpty())gamesPlatformId_=rows[std::clamp(i+delta,0,int(rows.size())-1)].toMap()["id"].toString();
                    }
                } else {
                    const auto rows=gamesCollections();
                    if(action==Action::Down && !gamesPlatforms().isEmpty())gamesRootZone_="platforms";
                    else {
                        const int i=std::clamp(gamesCollectionIndex()+(action==Action::Left?-1:action==Action::Right?1:0),0,int(rows.size()));
                        gamesCollectionId_=i==rows.size()?QString("_create"):rows[i].toMap()["id"].toString();
                    }
                }
                emit changed();
                return;
            }
            if(action==Action::Back) {
                collectionsRoot_=true;emit changed();return;
            }
            if (multiverseFace_) multiverse_.dispatch(action); else worlds_.dispatch(action);
            return;
        }
        if(dispatchGameHub(action))return;
        if (page_ == AppsPage) { if(appsFace_=="apps")apps_.dispatch(action);else if(action==Action::Back)selectFace(0); return; }
        if (page_ == SocialPage) { social_.dispatch(action); return; }
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
        else if (menuOpen_) {menuOpen_=false;if(optionsReturn_){optionsReturn_=false;homeMenuOpen_=true;}}
        else if (drawerOpen_) drawerOpen_ = false;
    } else if (action == Action::ToggleContinue) {
        if (page_ == 0 && !menuOpen_ && notice_.isEmpty()) drawerOpen_ = !drawerOpen_;
    } else if (action == Action::Confirm) confirm();
    else if (notice_.isEmpty() && (menuOpen_ || drawerOpen_)) {
        // Page actions use fixed physical buttons. Only an open list moves focus.
        int* focus = menuOpen_ ? &menuFocus_ : &drawerSelection();
        const int count = menuOpen_ ? menuItems().size() : std::max(1, int(drawerEntries().size()));
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
