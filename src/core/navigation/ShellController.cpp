#include <QDesktopServices>
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
#include <QScopedValueRollback>
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
QVariantList ShellController::notifications() const {auto rows=social_.notifications();rows.append(achievementNotifications_);const auto rewards=backendAccount_.prizes();if(rewards["pending"].toInt()>0)rows.append(QVariantMap{{"id","traineros-prizes"},{"provider","traineros-prizes"},{"name","Rewards earned"},{"detail",QString::number(rewards["pending"].toInt())+" prizes waiting in your profile"},{"unread",true}});const auto update=settings_.updates()->notification();if(!update.isEmpty())rows.append(update);return rows;}
QVariantMap ShellController::passiveNotice() const {
    if(social_.account()["doNotDisturb"].toBool())return {};
    if(!social_.toastTitle().isEmpty())return {{"title",social_.toastTitle()},{"detail",social_.toastText()},{"kind","social"}};
    if(!achievementToast_.isEmpty())return {{"title",achievementToast_.section('\n',0,0)},{"detail",achievementToast_.section('\n',1)},{"kind","achievement"}};
    return {};
}
void ShellController::dismissPassiveNotice() {
    if(!social_.toastTitle().isEmpty()){dismissNotificationById(social_.toastChannel());social_.hideToast();}
    else {if(!achievementNotifications_.isEmpty())dismissNotificationById(achievementNotifications_.first().toMap()["id"].toString());achievementToast_.clear();++achievementToastGeneration_;}
    emit changed();
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
      libraryManager_(repo, nullptr, this), libraryTools_(repo,this), scraper_(repo,this), pcGames_(repo,*files_.service(),this), gamePerformance_(repo,this), settings_(this), device_(this), diagnostics_(this), dashboard_(repo,achievements,social_,this), story_(repo,achievements) {

    connect(&pcGames_,&PcGamesController::changed,this,&ShellController::changed);
    connect(&pcGames_,&PcGamesController::installerStarted,&apps_,&AppsController::watchInstaller);
    connect(&pcGames_,&PcGamesController::installerFinished,&apps_,&AppsController::finishInstaller);
    connect(&pcGames_,&PcGamesController::resumeInstaller,&apps_,&AppsController::resumeInstaller);
    connect(&pcGames_,&PcGamesController::changed,this,[this]{downloads_.publish("pc",pcGames_.tasks());downloads_.publish("pc-runtime",pcGames_.runtimeTasks());});
    connect(&downloads_,&DownloadsController::commandRequested,this,[this](QString provider,QString,QString command){
        if(provider=="pc"){if(command=="open")downloads_.close();pcGames_.taskCommand(command);}
        if(provider=="pc-runtime"){if(command=="open")downloads_.close();pcGames_.runtimeTaskCommand(command);}
    });
    settings_.gameInput()->configure(&repo,{},{});
    connect(settings_.gameInput(),&GameInputProfiles::changed,&settings_,&SettingsController::changed);
    connect(settings_.gameInput(),&GameInputProfiles::backRequested,this,[this]{settings_.selectPage("game-input",false);});
    connect(&pcGames_,&PcGamesController::saved,this,[this](QString){refreshLibrary();});
    connect(&libraryTools_,&LibraryToolsController::gameSettingsRequested,&gamePerformance_,&GamePerformance::begin);
    connect(&gamePerformance_,&GamePerformance::changed,this,&ShellController::changed);
    connect(&gamePerformance_,&GamePerformance::compatibilityRequested,&pcGames_,&PcGamesController::edit);
    connect(&pcGames_,&PcGamesController::performanceRequested,&gamePerformance_,&GamePerformance::begin);
    connect(&pcGames_,&PcGamesController::controlsRequested,this,[this](const QString& id){service_="settings";settings_.begin();settings_.selectPage("game-input",true);settings_.gameInput()->beginGame(id);emit changed();});
    connect(settings_.gameInput(),&GameInputProfiles::textRequested,this,[this](QString title,QString text,int limit){textTarget_=TextTarget::GameInput;keyboard_.begin(title,text,limit);});
    connect(&pcGames_,&PcGamesController::textRequested,this,[this](QString title,QString text,int limit){textTarget_=TextTarget::PcGame;keyboard_.begin(title,text,limit);});

    connect(&dashboard_,&HomeDashboardController::changed,this,&ShellController::changed);
    social_.lobbyCollections=[this]{return gamesCollections();};
    social_.lobbyCollectionContains=[this](QString collection,QString game){return multiverse_.belongsToCollection(game,collection);};
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
        if(service_=="profile" && profileRoute_=="ra"){profileRoute_="story";overviewFace_="screens";}
        else {openProfile();profileRoute_="story";overviewFace_="screens";}
        achievementPhotoReturn_=true;emit changed();
    });
    connect(&social_,&SocialController::presentationChanged,this,&ShellController::changed,Qt::QueuedConnection);
    settings_.setScraper(&scraper_);
    settings_.setCrates(&crates_);
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
    connect(settings_.updates(),&SoftwareUpdateController::changed,this,[this]{downloads_.publish("traineros-software",settings_.updates()->tasks());});
    connect(settings_.updates(),&SoftwareUpdateController::downloadsRequested,&downloads_,&DownloadsController::begin);
    connect(settings_.updates(),&SoftwareUpdateController::cratesRequested,this,[this]{service_.clear();openOnlineFace("crates");});
    connect(&downloads_,&DownloadsController::commandRequested,this,[this](const QString& provider,const QString&,const QString& command){
        if(provider!="traineros-software")return;
        if(command=="open"){downloads_.close();service_="settings";settings_.openUpdates();emit changed();}
        else settings_.updates()->command(command);
    });
    connect(&social_,&SocialController::filesDownloadRequested,&downloads_,&DownloadsController::begin);
    connect(social_.files(),&ChatFiles::changed,this,[this]{downloads_.publish("chat-files",social_.files()->downloads());});
    connect(social_.files(),&ChatFiles::folderRequested,this,[](const QString& folder){QDesktopServices::openUrl(QUrl::fromLocalFile(folder));});
    connect(&scraper_,&ScrapeController::jobStarted,this,[this]{scraper_.showOverview();});
    connect(&scraper_,&ScrapeController::systemSelectionClosed,this,[this]{menuOpen_=true;powerMenu_=false;menuFocus_=13;emit changed();});
    connect(&downloads_,&DownloadsController::commandRequested,this,[this](const QString& provider,const QString& task,const QString& command){
        if(provider=="screenscraper"){if(command=="open")downloads_.close();scraper_.downloadCommand(task,command);}
        if(provider=="chat-files")social_.files()->control(task,command);
    });
    connect(&scraper_,&ScrapeController::changed,this,&ShellController::changed);
    connect(&scraper_,&ScrapeController::changed,&settings_,&SettingsController::changed);
    connect(&scraper_,&ScrapeController::settingsBackRequested,this,[this]{settings_.selectCategory(14,false);});
    connect(&scraper_,&ScrapeController::textRequested,this,[this](QString title,QString initial,bool secret){textTarget_=TextTarget::Scraper;keyboard_.begin(title,initial,256,secret,"Save");});
    connect(&libraryTools_,&LibraryToolsController::scrapeRequested,this,[this](const QString& game,const QString& world){
        if(scraper_.busy())scraper_.showOverview();else scraper_.begin(game,world);
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
    connect(this,&ShellController::changed,this,[this]{network_.setActive(controlsNetworkOpen_ || onboardingConnections_ || (service_=="settings" && settings_.category()==10));});
    connect(&settings_,&SettingsController::changed,this,[this]{network_.setActive(controlsNetworkOpen_ || onboardingConnections_ || (service_=="settings" && settings_.category()==10));});
    connect(&network_,&NetworkController::backRequested,this,[this]{if(controlsNetworkOpen_)closeControlsNetwork();else settings_.selectCategory(10,false);});
    connect(&network_,&NetworkController::radioRequested,this,[this](int index){device_.adjustQuick(index,Action::Confirm);});
    connect(&network_,&NetworkController::textRequested,this,[this](const QString& title,int limit,bool secret){
        textTarget_=TextTarget::Network;keyboard_.begin(title,{},limit,secret,secret?"Connect":"Confirm");
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
        socialFace_="chats";openOnlineFace("chats");emit changed();
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
    connect(&backendAccount_,&BackendAccount::changed,this,[this]{
        settings_.setBackendAccount(backendAccount_.actionLabel(),backendAccount_.detail());emit changed();
    });
    backendAccount_.rewardSource=[this,&achievements,offset=0]() mutable {
        auto source=achievements.rewardCredentials();
        if(source.isEmpty())return source;
        QList<int> ids;
        for(const auto& set:achievements.sets()) {
            bool ok=false;const int id=set.gameId.toInt(&ok);
            if(ok && id>0 && !ids.contains(id))ids.append(id);
        }
        for(const auto& item:achievements.accountHistory()) {
            bool ok=false;const int id=item.toMap()["id"].toString().toInt(&ok);
            if(ok && id>0 && !ids.contains(id))ids.append(id);
        }
        QJsonArray batch;
        // Bounded rotating batches also catch offline awards in older linked games.
        for(int i=0;i<qMin(5,int(ids.size()));++i)batch.append(ids[(offset+i)%ids.size()]);
        if(!ids.isEmpty())offset=(offset+batch.size())%ids.size();
        source["games"]=batch;return source;
    };
    connect(&social_,&SocialController::changed,this,[this]{backendAccount_.setSocialSubject(social_.account()["userId"].toString());});
    connect(&files_, &FilesController::changed, this, &ShellController::changed);
    connect(&browser_, &BrowserController::pageInputRequested,this,[this](int hints){textTarget_=TextTarget::Browser;keyboard_.beginLive(hints);});
    connect(&browser_, &BrowserController::pageInputEnded,this,[this]{if(keyboard_.live())keyboard_.cancel();});
    connect(&keyboard_, &TextEntryController::textCommitted,&browser_,&BrowserController::commitPageText);
    connect(&keyboard_, &TextEntryController::keyRequested,&browser_,&BrowserController::pageKey);
    connect(&keyboard_, &TextEntryController::keyStroke,&browser_,&BrowserController::pageKeyStroke);
    connect(&keyboard_, &TextEntryController::changed,this,[this]{if(!keyboard_.isOpen())browser_.stopPageInput();});
    connect(&browser_, &BrowserController::textRequested, this, [this](QString title,QString text,int limit,bool secret){
        textTarget_=TextTarget::Browser;keyboard_.begin(title,text,limit,secret,"Go");
        keyboard_.select(0,text.size());
    });
    connect(&browser_, &BrowserController::downloadsChanged, this, [this]{downloads_.publish("browser",browser_.downloads());});
    browser_.setFilesService(files_.service());
    social_.files()->setFilesService(files_.service());
    connect(&downloads_,&DownloadsController::destinationRequested,this,[this](QString provider,QString id,QString folder){
        if(provider=="browser")browser_.setDownloadDestination(id,folder);
        else if(provider=="chat-files")social_.files()->setDownloadDestination(id,folder);
    });
    connect(&downloads_,&DownloadsController::reorderRequested,this,[this](QString provider,QString id,QString before){
        if(provider=="browser")browser_.moveDownloadBefore(id,before);
        else if(provider=="crates")crates_.installer()->moveBefore(id,before);
        else if(provider=="screenscraper")scraper_.moveDownloadBefore(id,before);
    });
    connect(&downloads_, &DownloadsController::commandRequested, this, [this](QString provider,QString task,QString command){if(provider=="browser")browser_.controlDownload(task,command);});
    connect(&files_, &FilesController::textRequested, this, [this](QString title,QString text,int limit){textTarget_=TextTarget::Files;keyboard_.begin(title,text,limit);});
    connect(&files_, &FilesController::openRequested, &apps_, &AppsController::openFile);
    connect(files_.service(), &FilesService::changed, this, [this]{downloads_.publish("files",files_.service()->tasks());});
    connect(&downloads_, &DownloadsController::commandRequested, this, [this](QString provider,QString task,QString command){
        if(provider!="files")return;
        if(command=="cancel"||command=="cancel-all")files_.service()->cancel(task);
        if(command=="resolve"){downloads_.close();goToPage(AppsPage);selectFace(2);}
    });
    connect(&apps_, &AppsController::changed, this, &ShellController::changed);
    connect(crates_.installer(),&CratesInstaller::changed,this,[this]{downloads_.publish("crates",crates_.downloadTasks());});
    connect(&crates_,&CratesController::catalogueChanged,this,[this]{downloads_.publish("crates",crates_.downloadTasks());});
    connect(&crates_,&CratesController::downloadsRequested,&downloads_,&DownloadsController::begin);
    connect(&downloads_,&DownloadsController::commandRequested,this,[this](QString provider,QString id,QString command){if(provider=="crates")crates_.installer()->command(id,command);});
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
    connect(&settings_, &SettingsController::softwareRequested, this, [this](const QString& destination) {
        service_.clear(); if(destination=="crates")openOnlineFace("crates");else {goToPage(AppsPage);selectFace(0);} emit changed();
    });
    connect(&settings_, &SettingsController::controllerRequested, this, [this] { service_ = "diagnostics"; diagnostics_.begin(); emit changed(); });
    diagnostics_.configureLibrary(&repo,&adapter);
    diagnostics_.canCheck=[this]{return liveGame_.isEmpty()&&!scraper_.busy()&&!navigationLocked();};
    connect(&settings_,&SettingsController::healthRequested,this,[this]{service_="diagnostics";diagnostics_.beginHealth();emit changed();});
    connect(&diagnostics_,&DiagnosticsController::reportsRequested,this,[this](const QString& path){service_.clear();goToPage(AppsPage);selectFace(2);files_.navigate(QUrl::fromLocalFile(path).toString());emit changed();});
    connect(&diagnostics_,&DiagnosticsController::setupRequested,this,[this](const QString& id){service_.clear();libraryTools_.beginGame(id);libraryTools_.activate(3);emit changed();});
    connect(&diagnostics_,&DiagnosticsController::recheckRequested,&settings_,&SettingsController::libraryRefreshRequested);
    connect(&device_, &DeviceController::changed, this, &ShellController::changed);
    connect(this, &ShellController::changed, this, [this] { device_.setMonitoring(onboardingConnections_ || onboardingSettings_ || menuOpen_ || gameControlsOpen_ || service_ == "device" || service_ == "settings"); });
    connect(&device_, &DeviceController::closeRequested, this, [this] { service_ = "settings"; emit changed(); });
    connect(&device_, &DeviceController::messageRequested, this, &ShellController::showNotice);
    connect(&powerControl_, &PowerControl::changed, this, &ShellController::changed);
    connect(&device_, &DeviceController::powerRequested, this, [this](const QString& mode) {
        if(!liveGame_.isEmpty()){showNotice("Save and exit the running game before leaving this system session.");return;}
        if(scraper_.busy()){showNotice("Finish or cancel downloads before leaving this system session.");return;}
        mode_ = mode;
        notice_ = mode == "reboot" ? "Restart your handheld? Your Trainer data will be saved first."
                                   : "Turn off your handheld? Your Trainer data will be saved first.";
        emit changed();
    });
    hall_.editor()->setLibrary(&repo);
    hall_.setGameLibrary(&repo);
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
        if ((!onboardingSettings_ && service_ != "settings") || menuOpen_) { notice_ = message; emit changed(); }
    });
    connect(settings_.communication(),&CommunicationSettings::trainerSwitchRequested,this,[this]{
        if(!liveGame_.isEmpty() || scraper_.busy()){showNotice("Finish the current game and downloads before switching Trainer.");return;}
        settings_.communication()->leave();if(trainerSetup_.live())emit trainersRequested();else openTrainers();
    });
    connect(settings_.communication(),&CommunicationSettings::appearanceRequested,this,[this]{
        openProfile();
        if(service_=="profile") { settings_.communication()->leave(); trainer_.beginAvatarEdit(); }
    });
    connect(&controls_,&ControlsDashboard::actionRequested,this,&ShellController::systemAction);
    connect(&controls_,&ControlsDashboard::adjustRequested,this,[this](const QString& id,int delta){
        const int index=id=="volume"?0:1;
        const int value=device_.rows().value(index).toMap().value("level",-1).toInt();
        if(value>=0)device_.setQuickLevel(index,value+delta);
    });
    connect(&controls_,&ControlsDashboard::closeRequested,this,&ShellController::closeControls);
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
    connect(&achievements,&AchievementProvider::snapshotChanged,this,[this]{
        if(service_=="profile"){profileSnapshot_=story_.profile();emit changed();}
    });
    connect(&trainer_, &TrainerController::messageRequested, this, [this](const QString& message) {
        notice_ = message; emit changed();
    });
    connect(&trainer_, &TrainerController::nameRequested, this, [this](const QString& initial) {
        textTarget_ = TextTarget::TrainerName;
        keyboard_.begin("Trainer name", initial, TrainerController::NameLimit);
    });
    connect(&outfits_,&OutfitController::changed,this,&ShellController::changed);
    connect(&outfits_,&OutfitController::backRequested,this,[this]{profileRoute_="summary";profileFocus_=1;emit changed();});
    connect(&outfits_,&OutfitController::editRequested,this,[this]{trainer_.beginAvatarEdit();});
    connect(&outfits_,&OutfitController::nameRequested,this,[this](const QString& initial){textTarget_=TextTarget::OutfitName;keyboard_.begin("Name this look",initial,32,false,"Save");});
    connect(this,&ShellController::changed,this,[this]{if(service_!="profile")outfits_.close();});
    connect(&social_, &SocialController::changed, this, [this]{
        if(homeMenuOpen_)homeMenuSelection_=homeMenuActions().value(homeMenuFocus()).toMap()["id"].toString();
        if(socialActive()||homeMenuOpen_)emit changed();
    });
    connect(&social_, &SocialController::textRequested, this, [this](QString title, QString text, int limit) {
        textTarget_ = TextTarget::Social; keyboard_.begin(title,text,limit,social_.textSecret(),social_.textSubmitLabel(),social_.textAllowsEmoji());
    });
    connect(&keyboard_, &TextEntryController::accepted, this, [this](const QString& text) {
        const auto target = textTarget_;
        textTarget_ = TextTarget::None;
        if(target==TextTarget::GameInput)settings_.gameInput()->applyText(text);
        else if (target == TextTarget::SetupName) trainerSetup_.applyName(text);
        else if (target == TextTarget::TrainerName) trainer_.setDraftName(text);
        else if (target == TextTarget::OutfitName) outfits_.acceptName(text);
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
        else if (target == TextTarget::Files) files_.applyText(text);
        else if (target == TextTarget::Browser) browser_.applyText(text);
        else if (target == TextTarget::PcGame) pcGames_.applyText(text);
        else if (target == TextTarget::Scraper) scraper_.applyText(text);
        else if (target == TextTarget::Communication) settings_.communication()->applyText(text);
    });
    refreshContinue();
    syncExperience();
}
void ShellController::configureServices(FileCatalog* files, PreferencesRepository* preferences) {
    downloads_.picker()->setCatalog(files);
    pcGames_.setCatalog(files);
    libraryTools_.setCatalog(files);
    libraryManager_.files()->setCatalog(files); settings_.setRepository(preferences);
    const auto records=repository_.registrations();
    settings_.setLegacyTrashAvailable(std::any_of(records.cbegin(),records.cend(),[](const auto& r){return r.removed && !r.trashPath.isEmpty();}));
}
void ShellController::refreshLibrary() {
    worlds_.refresh(); libraryManager_.refresh(); multiverse_.refresh();
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
    if(page_==AppsPage && appsFace_=="browser" && service_.isEmpty() && !keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && !quickAppsOpen_ && !drawerOpen_ && notice_.isEmpty())return true;
    if(page_==AppsPage && appsFace_=="files" && files_.view()["area"]=="files" && service_.isEmpty() && !files_.modal() && !files_.menuOpen() && !keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && !quickAppsOpen_ && !drawerOpen_ && notice_.isEmpty())return true;
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
    if(pcGames_.isOpen()||gamePerformance_.isOpen())return true;
    return files_.modal() || social_.files()->picker()->isOpen() || (social_.myLobbiesOpen() || social_.lobbyCreateOpen() || social_.lobbyPickerOpen() || social_.lobbyVoteOpen()) || collectionManager()->isOpen() || downloads_.isOpen() || scraper_.isOpen() || (socialActive() && !social_.menu().isEmpty()) || libraryTools_.isOpen() || trainer_.editing()
        || (adapterActive() && activeModule_ && activeModule_->modalOpen())
        || (((page_==GameHubPage && hubFace_=="ra") || (service_=="profile" && profileRoute_=="ra")) && (hall_.editor()->isOpen() || hall_.account()->isOpen() || hall_.galleryMenuOpen() || hall_.historyOpen() || hall_.route()=="achievement-detail"));
}
bool ShellController::chooseAdventureAvailable() {
    return !homeMenuOpen_ && (!adapterActive() || !activeModule_ || activeModule_->recentsAllowed())
        && !menuOpen_ && notice_.isEmpty() && !keyboard_.isOpen()
        && !localModalOpen() && !moduleActivityBusy() && !navigationLocked();
}
bool ShellController::navigationLocked(bool primaryRecovery) const {
    return settings_.updates()->transitionPending() || diagnostics_.checking() || modulesBlocked(primaryRecovery) || launchPreparation_.busy() || settings_.clock()->busy() || settings_.storage()->busy() || libraryTools_.busy();
}
bool ShellController::modulesBlocked(bool recovery) const {for(const auto& module:modules_)if(module->navigationBlocked(recovery))return true;return false;}
bool ShellController::moduleWriting() const {for(const auto& module:modules_)if(module->writing())return true;return false;}
bool ShellController::moduleActivityBusy(bool recovery) const {for(const auto& module:modules_)if(module->activityBusy(recovery))return true;return false;}
ExperienceModule* ShellController::module(const QString& id) const {for(const auto& module:modules_)if(module->descriptor().id==id)return module.get();return nullptr;}
QVariantMap ShellController::moduleInvitation() const {for(const auto& module:modules_){auto request=module->invitation();if(request["open"].toBool())return request;}return {};}

QString ShellController::primaryId() const {
    return QStringList{"home","games","gamehub","online","apps"}.value(page_,"home");
}
QStringList ShellController::primaryNames() const {
    return {"Home","Games","GameHub","Online","Apps"};
}
QStringList ShellController::hubFaceIds() const {
    QStringList result{"spaces"};
    if(experience_.hasAdapter())result.append("adapter");
    return result;
}
void ShellController::goToSection(const QString& id) {
    if(id=="social"){openOnlineFace("chats");return;}
    if(id=="lobby" || id=="crates" || id=="bazaar"){openOnlineFace(id);return;}
    const auto index=QStringList{"home","games","gamehub","online","apps"}.indexOf(id);
    if(index>=0)goToPage(index);
}
void ShellController::setHubFace(const QString& requested) {
    const auto face=hubFaceIds().contains(requested)?requested:QString("spaces");
    const bool switching=hubFace_!=face;
    if(switching && adapterActive() && activeModule_)
        experience_.rememberNavigation({{"module",activeModule_->navigation()},{"hall",hall_.navigationState()}});
    if(hubFace_!=face && activeModule_)activeModule_->leave(false);
    hubFace_=face;quickAppsOpen_=false;
    if(face=="adapter") {
        showExperienceScreen(experience_.screen());
        if(switching && activeModule_)activeModule_->restoreNavigation(experience_.navigation()["module"].toObject());
    }
    emit changed();
}
void ShellController::openGameHubFace(const QString& face) {
    if(navigationLocked() || keyboard_.isOpen() || localModalOpen())return;
    if(face=="lobby"){openOnlineFace("lobby");return;}
    if(face=="overview" || face=="ra") {
        openProfile();activateProfile(face=="ra"?3:2);return;
    }
    goToPage(GameHubPage);setHubFace(face);
}
void ShellController::openOnlineFace(const QString& face) {
    if(!QStringList{"lobby","bazaar","crates","chats"}.contains(face) || navigationLocked(true) || keyboard_.isOpen() || localModalOpen())return;
    onlineFace_=face;chatNavigationFocused_=false;
    goToPage(OnlinePage);
    if(face=="crates")crates_.enter();
}
void ShellController::selectChatFace(const QString& face) {
    if(page_!=OnlinePage || onlineFace_!="chats" || !pairedNavigationAvailable() || navigationLocked()
       || !QStringList{"chats","communities","friends"}.contains(face))return;
    socialFace_=face;social_.setFace(face);chatNavigationFocused_=false;emit changed();
}
void ShellController::selectFace(int index) {
    if(index<0 || index>=faceNames().size() || index==faceIndex()
       || !pairedNavigationAvailable() || navigationLocked())return;
    quickAppsOpen_=false;
    if(page_==GameHubPage)setHubFace(hubFaceIds().value(index));
    else if(page_==OnlinePage) {
        crates_.leave(true);
        onlineFace_=QStringList{"lobby","bazaar","crates","chats"}.value(index,"lobby");
        chatNavigationFocused_=false;
        if(socialActive())social_.setFace(socialSurface());
        else if(onlineFace_=="crates")crates_.enter();
    } else if(page_==AppsPage){
        const auto next=QStringList{"apps","browser","files"}.value(index,"apps");
        if(next==appsFace_)return;
        files_.leave();appsFace_=next;
        if(appsFace_=="files")files_.enter();else if(appsFace_=="browser")browser_.enter();
    }
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
    if(service_=="profile") {if(profileRoute_!="story" || !profileContentActive())return;}
    else if(page_!=GameHubPage || hubFace_!="overview" || !pairedNavigationAvailable())return;
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
    if(target["type"]=="lobbies"){if(social_.account()["state"]!="connected")social_.openLobby();else if(social_.myLobbies().isEmpty())social_.createAnotherLobby();else social_.showMyLobbies();return;}
    if(target["type"]=="play"){confirm();emit changed();return;}
    if(target["type"]=="screens") {
        openProfile();activateProfile(5);
        album_.forGame(currentAdventureId());
        dashboardReturn_=true;
    } else if(target["type"]=="person") {
        socialFace_="chats";openOnlineFace("chats");
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
    if(hubFace_=="spaces") {
        if(action==Action::Back)goToPage(HomePage);
        else if(action==Action::Confirm)goToPage(GamesPage);
        return true;
    }
    if(hubFace_=="ra"){hall_.dispatch(action==Action::LocalAction?Action::ToggleContinue:action);return true;}
    if(adapterActive() && activeModule_) {
        if(adapterMenuOpen_) {
            const auto ids=experience_.screenIds();
            if(action==Action::Up || action==Action::Down)
                adapterMenuFocus_=std::clamp(adapterMenuFocus_+(action==Action::Down?1:-1),0,qMax(0,int(ids.size())-1));
            else if(action==Action::Confirm || action==Action::Right)selectAdapterScreen(ids.value(adapterMenuFocus_));
            else if(action==Action::Back)setHubFace("spaces");
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
    const bool profileOwnerChanged=experience_.owner()!=owner;
    if(profileOwnerChanged){overviewFocus_=0;overviewSessionId_.clear();overviewFace_="summary";profileSnapshot_.clear();profileReturnNavigation_={};profileRoute_="summary";achievementPhotoReturn_=false;}
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
        if(hubFace_=="adapter" && !experience_.hasAdapter())hubFace_="spaces";
        adapterMenuFocus_=qMax(0,experience_.index());
        ExperienceContext context{owner,currentAdventureId(),game,trainer_.profile(),repository_.home(),progress_,experience_.generation()};
        for(auto& module:modules_)module->refresh(context);
        if(changing) {
            const auto restored=experience_.navigation();
            activeModule_->restoreNavigation(restored.contains("module")?restored["module"].toObject():restored.contains(descriptor.id)?restored.value(descriptor.id).toObject():restored);
            hall_.setCurrentAdventure(currentAdventureId());
            if(!hall_.browsingGames())hall_.restoreNavigation(restored["hall"].toObject());
        }
        if(adapterActive())showExperienceScreen(experience_.screen());
        else if(hubFace_=="ra")hall_.showAchievementLibrary();
    }
    if(profileOwnerChanged && service_=="profile")profileSnapshot_=story_.profile();
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
        QStringList result{"Game spaces"};
        if(experience_.hasAdapter())result.append(experience_.descriptor().adapter.label);
        return result;
    }
    if(page_==OnlinePage)return {"Lobby","Bazaar","Crates","Chats"};
    if(page_==AppsPage)return {"Apps","Browser","Files"};
    return {};
}
int ShellController::faceIndex() const {
    if(page_==GamesPage) {
        return gamesRootZone_=="platforms"?gamesPlatformIndex():gamesCollectionIndex();
    }
    if(page_==GameHubPage)return hubFaceIds().indexOf(hubFace_);
    if(page_==OnlinePage)return QStringList{"lobby","bazaar","crates","chats"}.indexOf(onlineFace_);
    if(page_==AppsPage)return QStringList{"apps","browser","files"}.indexOf(appsFace_);
    return 0;
}
QString ShellController::trainerFace() const {return hubFace_=="ra"?QString("ra"):experience_.screen();}
void ShellController::showTrainerFace(const QString& face) {
    if(face=="ra")openGameHubFace("ra");else {setHubFace("adapter");selectAdapterScreen(face);}
}
void ShellController::goToTrainerFace(const QString& face) {
    syncExperience();
    if(navigationLocked() || (face!="ra" && !experience_.screenIds().contains(face)))return;
    if(face=="ra"){openGameHubFace("ra");return;}
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
    if(service_=="profile")return trainer_.editing()?trainer_.focusIndex():profileRoute_=="ra"?hall_.focusIndex():profileRoute_=="story"?(overviewFace_=="screens"?album_.index():overviewFocus_):profileFocus_;
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
    const QStringList pages{"home","games","gamehub","online","apps"};
    auto experience=experience_;
    if(activeModule_)experience.rememberNavigation({{"module",activeModule_->navigation()},{"hall",hall_.navigationState()}});
    QJsonObject state{{"version", ShellNavigationVersion}, {"experienceVersion",2}, {"experienceNavigation",experience.state()}, {"page", pages[page_]},
            {"hubFace",hubFace_},{"adapterMenuOpen",adapterMenuOpen_},
            {"overviewFace",overviewFace_},{"appsFace",appsFace_},{"socialFace",socialFace_},{"onlineFace",onlineFace_},
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
    QScopedValueRollback<bool> restoring(restoringNavigation_,true);
    homeMenuOpen_=quickAppsOpen_=drawerOpen_=false;
    const int version=state["version"].toInt();
    if(version<1 || version>ShellNavigationVersion) {
        drawerFavorites_=favoriteSelectionLost_=false;drawerFocus_=favoriteFocus_=0;
        drawerScroll_[0]=drawerScroll_[1]=-1;emit drawerSourceChanged();
        experience_.clear();socialFace_="chats";hubFace_="spaces";appsFace_="apps";onlineFace_="lobby";
        social_.setFace(socialFace_);refreshExperience();goToPage(HomePage);return;
    }
    if(!state.contains("experienceNavigation"))experience_.clear();
    const QStringList roots{"home","games","gamehub","online","apps"};
    const QStringList legacyV1{"home","worlds","pokedex","trainer","hall"};
    const QStringList legacyV2{"home","worlds","companions","trainer","social","apps"};
    QString target=state["page"].toString();
    if(state["page"].isDouble())
        target=(version==1?legacyV1:version==2?legacyV2:roots).value(state["page"].toInt(),"home");
    const QString oldTarget=target;
    onlineFace_=state["onlineFace"].toString("lobby");
    if(!QStringList{"lobby","bazaar","crates","chats"}.contains(onlineFace_))onlineFace_="lobby";
    if(target=="social" || (version<4 && target=="online")){target="online";onlineFace_="chats";}
    const bool oldFirst=target=="pokedex" || target=="companions" || target=="first";
    const bool oldSecond=target=="trainer" || target=="hall" || target=="history" || target=="second";
    hubFace_=version>=3?state["hubFace"].toString("overview"):"overview";
    overviewFace_=state["overviewFace"].toString("summary");
    if(!QStringList{"summary","timeline","screens"}.contains(overviewFace_))overviewFace_="summary";
    appsFace_=state["appsFace"].toString("apps");
    if(appsFace_=="crates") {
        if(target=="apps"){target="online";onlineFace_="crates";}
        appsFace_="apps";
    }
    if(!QStringList{"apps","browser","files"}.contains(appsFace_))appsFace_="apps";
    if(target=="worlds" || target=="collections" || target=="multiverse")target="games";
    if(oldFirst || oldSecond){target="gamehub";hubFace_="adapter";}
    socialFace_=state["socialFace"].toString("chats");
    if(!QStringList{"chats","messages","groups","communities","friends","discover","lobby","recent"}.contains(socialFace_))socialFace_="chats";
    if(socialFace_=="messages")socialFace_="chats";
    if(socialFace_=="discover")socialFace_="friends";
    if(socialFace_=="lobby"){if(target=="online")onlineFace_="lobby";socialFace_="chats";}
    if(target=="gamehub" && hubFace_=="lobby"){target="online";onlineFace_="lobby";}
    bool legacyProfile=target=="gamehub" && (hubFace_=="overview" || hubFace_=="ra");
    QString legacyProfileFace=hubFace_;
    if(legacyProfile)target="home";
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
            if(requested=="ra"){legacyProfile=true;legacyProfileFace="ra";goToPage(HomePage);}
            else if(!experience_.hasAdapter()){hubFace_="spaces";overviewFace_="timeline";}
        } else if(oldFirst) {
            if(requested.isEmpty())requested=state["pokemonFace"].toString("dex");
            if(!experience_.hasAdapter())hubFace_="spaces";
        }
    }
    if(!requested.isEmpty())experience_.show(requested);
    if(!hubFaceIds().contains(hubFace_))hubFace_="spaces";
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
    else if(socialActive())social_.setFace(socialSurface());
    if(legacyProfile){
        const auto savedHall=hall_.navigationState();
        const auto savedOverview=overviewFace_;
        openProfile();activateProfile(legacyProfileFace=="ra"?3:2);
        if(legacyProfileFace=="ra")hall_.restoreNavigation(savedHall);
        else overviewFace_=savedOverview;
    }
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
void ShellController::closeControls() {
    device_.closeAudio();
    closeControlsNetwork();
    downloads_.close();controls_.leave();
    if(gameControlsOpen_){gameControlsOpen_=false;emit changed();emit gameControlsClosed(false);return;}
    menuOpen_=powerMenu_=false;
    if(optionsReturn_){optionsReturn_=false;homeMenuOpen_=true;}
    emit changed();
}
void ShellController::setGameControlsOpen(bool value){
    if(gameControlsOpen_==value)return;gameControlsOpen_=value;device_.setMonitoring(value || menuOpen_);
    if(value){controls_.setOwner(trainer_.profile()["id"].toString());controls_.begin();}
    else {device_.closeAudio();closeControlsNetwork();controls_.leave();}emit changed();
}
void ShellController::closeControlsNetwork(){
    if(!controlsNetworkOpen_)return;
    controlsNetworkOpen_=false;network_.setActive(false);emit changed();
}
void ShellController::dispatchControls(Action action){
    if(action==Action::SystemMenu||action==Action::Home){closeControls();return;}
    if(device_.audioOpen()){device_.dispatchAudio(action);return;}
    if(controlsNetworkOpen_){
        if(keyboard_.isOpen()){
            keyboard_.dispatch(action);
            if(action==Action::Back){textTarget_=TextTarget::None;network_.cancelText();}
        }else if(action!=Action::Left&&action!=Action::Right)network_.dispatch(action);
        return;
    }
    controls_.dispatch(action);
}
void ShellController::dispatchGameControls(Action action){
    if(!gameControlsOpen_)return;
    if(action==Action::Home){downloads_.close();setGameControlsOpen(false);emit gameControlsClosed(true);return;}
    if(downloads_.isOpen()){downloads_.dispatch(action);return;}
    dispatchControls(action);
}
void ShellController::systemAction(const QString& id){
    if(id=="audio-output"){
        if((menuOpen_||gameControlsOpen_)&&!controls_.editing()&&!controlsNetworkOpen_)device_.beginAudio();
        return;
    }
    if(id=="volume"){device_.adjustQuick(0,Action::Confirm);return;}
    if(id=="wifi"||id=="bluetooth"){
        controlsNetworkOpen_=true;
        if(id=="bluetooth")network_.openBluetooth();else network_.openWifi();
        emit changed();return;
    }
    if(id=="airplane"){
        device_.adjustQuick(4,Action::Confirm);return;
    }
    if(id=="clock"||id=="battery"||id=="storage")return;
    if(id=="downloads"){downloads_.begin();return;}
    if(gameControlsOpen_){setGameControlsOpen(false);emit gameSystemRequested(id);return;}
    if(id=="scraper"){scraper_.beginMissing();scraper_.showOverview();return;}
    if((id=="desktop"||id=="steam")&&!liveGame_.isEmpty()){showNotice("Save and exit the running game before switching environments.");return;}
    const QMap<QString,int> actions{{"settings",0},{"power",6},{"desktop",4},{"steam",5}};
    if(!actions.contains(id))return;
    menuOpen_=true;menuFocus_=actions[id];activate(menuFocus_);emit changed();
}
QStringList ShellController::menuItems() const {
    if (powerMenu_) {
        QStringList items{"Sleep", powerControl_.keepAwake()?"Keep awake: On":"Keep awake: Off", "Restart", "Power off", "Cancel"};
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
    device_.closeAudio();
    closeControlsNetwork();
    // Closing transient controllers emits their local notifications. Publish
    // only the completed shell transition, not every intermediate close, so
    // hidden Home/drawer bindings do not rebuild the library repeatedly.
    QSignalBlocker transition(this);
    homeMenuOpen_ = false;dashboardReturn_=false;profileRoute_="summary";
    if(adapterActive() && activeModule_)
        experience_.rememberNavigation({{"module",activeModule_->navigation()},{"hall",hall_.navigationState()}});
    for(auto& module:modules_)module->leave(page!=page_);
    collectionManager()->close();
    { PerformanceTrace::Scope phase("navigation.tools"); libraryTools_.close(); }
    { PerformanceTrace::Scope phase("navigation.storage"); settings_.storage()->close(); }
    { PerformanceTrace::Scope phase("navigation.keyboard"); if(textTarget_==TextTarget::Social)social_.preserveText(keyboard_.text()); keyboard_.cancel(); }
    if(textModule_){textModule_->cancelText();textModule_=nullptr;}
    textTarget_ = TextTarget::None;
    apps_.leave(true);files_.leave();crates_.leave(true);
    social_.closeMenu();
    { PerformanceTrace::Scope phase("navigation.archive"); hall_.editor()->cancel(); }
    { PerformanceTrace::Scope phase("navigation.account"); hall_.account()->close(); }
    { PerformanceTrace::Scope phase("navigation.trainer"); trainer_.cancel(); }
    { PerformanceTrace::Scope phase("navigation.setup"); trainerSetup_.close(); }
    { PerformanceTrace::Scope phase("navigation.manager"); libraryManager_.close(); service_.clear(); }
    page_ = std::clamp(page, 0, int(primaryNames().size())-1); // No wrapping until physical-device testing.
    quickAppsOpen_=false;
    if(page_==AppsPage){apps_.refresh();if(appsFace_=="files")files_.enter();else if(appsFace_=="browser")browser_.enter();}
    if(socialActive())social_.setFace(socialSurface());
    if(page_==OnlinePage && onlineFace_=="crates" && !restoringNavigation_)crates_.enter();
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
    pcAddFocused_=false;
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
    if(type.isEmpty() && page_==HomePage && dashboard_.focus().startsWith("event:")){
        homeMenuOpen_=notificationsOpen_=true;notificationSelection_=dashboard_.focus().mid(6);emit changed();return;
    }
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
    else if(page_==OnlinePage && onlineFace_=="crates")crates_.dispatch(Action::ContextMenu);
    else if(socialActive() && !chatNavigationFocused_)social_.dispatch(Action::LocalAction);
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
    if(libraryTools_.busy()&&libraryTools_.canCancel()){libraryTools_.activate(index);return;}
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
    else if(service_=="profile") {
        if(trainer_.editing())trainer_.activate(index);
        else if(profileRoute_=="ra") {if(area.isEmpty())hall_.activate(index);else hall_.activateControl(area,index);}
        else if(profileRoute_=="story") {
            if(overviewFace_=="screens" && area=="overview-media")album_.select(index);
            else {
                overviewFocus_=std::clamp(index,0,qMax(0,int(overviewFace_=="timeline"?storyHistory().size():2)-1));
                if(area=="overview-timeline")showOverview("timeline");
                else if(area=="overview-screens")showOverview("screens");
            }
            emit changed();
        } else activateProfile(index);
        return;
    }
    else if (service_ == "settings") { if(hall_.account()->isOpen()) { hall_.account()->activate(index); return; } if(trainer_.editing()) { trainer_.activate(index); return; } if(settings_.controlsFocused()) {if(settings_.category()==10)network_.activate(index);else settings_.activateRow(index);} else {settings_.selectSection(index);settings_.dispatch(Action::Confirm);} return; }
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
    if (index < 0 || index > 5) return;
    trainerSettingsFocus_ = index;
    if(index==5||(index==4&&service_=="trainer-settings")){backendAccount_.activate();emit changed();return;}
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
            if(scraper_.busy())scraper_.showOverview();else scraper_.beginSystems();
            return;
        }
        if(scraper_.busy()&&((powerMenu_&&(menuFocus_==2||menuFocus_==3||menuFocus_==5))||(!powerMenu_&&(menuFocus_==2||menuFocus_==4||menuFocus_==5)))){showNotice("Finish or cancel downloads before switching Trainer or leaving this system session.");return;}
        if (powerMenu_) {
            if (menuFocus_ == 0) { powerControl_.sleep(); return; }
            if (menuFocus_ == 1) { powerControl_.toggleKeepAwake(); return; }
            if (menuFocus_ == 4) { powerMenu_ = menuOpen_ = false; return; }
            if (menuFocus_ == 5) {
                if (platform_.canSwitchSession()) { mode_ = "traineros"; notice_ = "Enter the TrainerOS session?"; }
                else { mode_ = "development-exit"; notice_ = "Close the development app?"; }
                return;
            }
            device_.requestPower(menuFocus_ == 2);
            return;
        }
        if (menuFocus_ >= 7) { if(menuFocus_<9 || !network_.busy())device_.adjustQuick(menuFocus_ - 7, Action::Confirm); return; }
        if (menuFocus_ == 6) { powerMenu_ = true; menuFocus_ = 0; return; }
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
    } else if (page_ == OnlinePage) {
        dispatch(Action::Confirm); return;
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
bool ShellController::profileContentActive() const {
    return service_=="profile" && !trainer_.editing() && !homeMenuOpen_ && !menuOpen_
        && !drawerOpen_ && !quickAppsOpen_ && !keyboard_.isOpen() && !downloads_.isOpen()
        && !scraper_.isOpen() && !libraryTools_.isOpen() && notice_.isEmpty();
}
QVariantMap ShellController::profileAccount() const {
    const auto identity=backendAccount_.snapshot();
    const auto progress=backendAccount_.progression(),mastery=progress["mastery"].toMap();
    const bool known=progress["known"].toBool();
    return {{"signedIn",identity["state"].toString()=="signed_in"},
        {"levelKnown",known},{"balanceKnown",known},{"previewKnown",!progress["support_preview"].toMap().isEmpty()},
        {"stale",progress["stale"]},{"xp",mastery["xp"]},{"levelXp",mastery["level_xp"]},{"nextLevelXp",mastery["next_level_xp"]},
        {"levelText",known?QString::number(mastery["level"].toLongLong()):QString::fromUtf8("\xe2\x80\x94")},
        {"sparksText",known?QString::number(progress["balance"].toLongLong()):QString::fromUtf8("\xe2\x80\x94")}};
}
void ShellController::openProfile() {
    if(navigationLocked() || moduleWriting() || keyboard_.isOpen() || trainer_.saving() || trainer_.editing())return;
    if(service_=="profile")return;
    profileReturnNavigation_={{"hall",hall_.navigationState()},{"overviewFace",overviewFace_},
        {"overviewFocus",overviewFocus_},{"overviewSession",overviewSessionId_},
        {"album",QJsonObject::fromVariantMap(album_.navigationState())}};
    homeMenuOpen_=false;quickAppsOpen_=drawerOpen_=false;service_="profile";trainer_.reload();profileFocus_=0;
    profileRoute_="summary";profileRaAccountReturn_=false;profileSnapshot_=story_.profile();backendAccount_.refreshProgression();backendAccount_.refreshPrizes();emit changed();
}
void ShellController::closeProfile() {
    backendAccount_.closeCelebration(false);
    outfits_.close();
    hall_.closeHistory();hall_.closeGalleryMenu();album_.close();
    achievementPhotoReturn_=false;service_.clear();
    const auto previous=profileReturnNavigation_;profileReturnNavigation_={};
    profileRoute_="summary";
    // A transient profile does not leave its primary page. Restore only the
    // shared presenters it borrowed, without replaying startup migrations.
    if(!previous.isEmpty()) {
        hall_.restoreNavigation(previous["hall"].toObject());
        overviewFace_=previous["overviewFace"].toString("summary");
        overviewFocus_=previous["overviewFocus"].toInt();overviewSessionId_=previous["overviewSession"].toString();
        album_.restoreNavigation(previous["album"].toObject().toVariantMap());
    }
    emit changed();
}
void ShellController::activateProfile(int index) {
    if(!profileContentActive())return;
    if(profileRoute_=="prizes") {
        if(backendAccount_.prizes()["celebrating"].toBool()){backendAccount_.closeCelebration(true);return;}
        if(index>=100){profileFocus_=index;backendAccount_.selectPrize(index-100);}
        else {profileFocus_=index;
            if(index==0){if(backendAccount_.snapshot()["state"]!="signed_in")backendAccount_.activate();else backendAccount_.refreshPrizes();}
            else if(index==1)backendAccount_.celebratePrizes();
            else if(index==2)backendAccount_.togglePrizeShelf();
            else if(index==3)backendAccount_.refreshPrizes(true);
            else if(index==4){profileRoute_="outfits";outfits_.open();}
        }
        emit changed();return;
    }
    if(profileRoute_=="support") {
        if(backendAccount_.snapshot()["state"]!="signed_in")backendAccount_.activate();
        else backendAccount_.refreshProgression(false);
        return;
    }
    if(profileRoute_=="account") {
        if(index==0)backendAccount_.activate();
        else if(index==1)backendAccount_.activateRewards();
        else if(index==2){profileRaAccountReturn_=true;profileRoute_="ra";hall_.showAchievementLibrary();hall_.account()->begin();}
        else if(index==3)backendAccount_.copyRewardChallenge();
        emit changed();return;
    }
    if(profileRoute_!="summary")return;
    if(index<0 || index>11)return;
    profileFocus_=index;
    if(index==11){profileRoute_="prizes";profileFocus_=0;backendAccount_.refreshPrizes();}
    else if(index==10){profileRoute_="support";profileFocus_=0;backendAccount_.refreshProgression(false);}
    else if(index==9){profileRoute_="shop";profileFocus_=1;backendAccount_.refreshShop();}
    else if(index==1){profileRoute_="outfits";outfits_.open();}
    else if(index==0)trainer_.activate(index);
    else if(index==4){profileRoute_="account";profileFocus_=0;backendAccount_.refreshProgression(false);}
    else if(index==3 || index>=7) {
        hall_.showAchievementLibrary();profileRoute_="ra";
        if(index>=7) {
            const auto award=profileSnapshot_["badges"].toList().value(index-7).toMap();
            if(!award.isEmpty()) {
                auto state=hall_.navigationState();state["route"]="achievement-detail";
                state["set"]=award["set"].toString();state["zone"]="actions";
                auto ids=state["achievements"].toObject();ids[award["set"].toString()]=award["achievement"].toString();state["achievements"]=ids;
                hall_.restoreNavigation(state);
            }
        }
    } else {
        profileRoute_="story";overviewFace_=index==5?"screens":index==6?"timeline":"summary";overviewFocus_=0;
        if(index==5)album_.chooseFilter(0);
        album_.setMedia(storyMedia());
    }
    emit changed();
}
void ShellController::dispatchProfile(Action action) {
    if(trainer_.editing()){trainer_.dispatch(action,true);return;}
    if(profileRoute_=="prizes") {
        if(backendAccount_.prizes()["celebrating"].toBool()) {
            if(action==Action::Back||action==Action::Confirm)backendAccount_.closeCelebration(action==Action::Confirm);
        }else if(action==Action::Back){profileRoute_="summary";profileFocus_=11;}
        else if(action==Action::Confirm)activateProfile(profileFocus_>=100?2:profileFocus_);
        else if(action==Action::Up||action==Action::Down) {
            const auto count=backendAccount_.prizes()["items"].toList().size();
            if(count){backendAccount_.selectPrize(backendAccount_.prizes()["index"].toInt()+(action==Action::Up?-1:1));profileFocus_=100+backendAccount_.prizes()["index"].toInt();}
        }else if(action==Action::Left||action==Action::Right)profileFocus_=qBound(0,(profileFocus_>=100?0:profileFocus_)+(action==Action::Left?-1:1),4);
        emit changed();return;
    }
    if(profileRoute_=="outfits"){outfits_.dispatch(action);return;}
    if(profileRoute_=="support") {
        if(action==Action::Back){profileRoute_="summary";profileFocus_=10;}
        else if(action==Action::Confirm)activateProfile(0);
        emit changed();return;
    }
    if(dashboardReturn_ && !achievementPhotoReturn_ && action==Action::Back
       && ((profileRoute_=="ra" && hall_.route()=="achievement-detail" && !hall_.account()->isOpen() && !hall_.historyOpen() && !hall_.galleryMenuOpen())
           || (profileRoute_=="story" && overviewFace_=="screens" && !album_.filtering()))) {
        dashboardReturn_=false;closeProfile();return;
    }
    if(profileRoute_=="shop") {
        const bool reviewing=!backendAccount_.shop()["review"].toMap().isEmpty();
        if(action==Action::Back){if(reviewing){backendAccount_.cancelPurchaseReview();profileFocus_=1;}else {profileRoute_="summary";profileFocus_=9;}}
        else if(reviewing) {
            if(action==Action::Left)profileFocus_=3;
            else if(action==Action::Right)profileFocus_=4;
            else if(action==Action::Confirm){if(profileFocus_==4)backendAccount_.confirmPurchase();else backendAccount_.cancelPurchaseReview();profileFocus_=1;}
        } else if(action==Action::Up || action==Action::Down){backendAccount_.selectShopItem(backendAccount_.shop()["index"].toInt()+(action==Action::Up?-1:1));profileFocus_=1;}
        else if(action==Action::Left)profileFocus_=0;
        else if(action==Action::Right)profileFocus_=1;
        else if(action==Action::Confirm){if(profileFocus_==0)backendAccount_.refreshShop();else backendAccount_.activateShopItem();if(!backendAccount_.shop()["review"].toMap().isEmpty())profileFocus_=3;}
        emit changed();return;
    }
    if(profileRoute_=="ra") {
        if(profileRaAccountReturn_ && action==Action::Back && hall_.account()->isOpen()) {
            hall_.dispatch(action);
            if(!hall_.account()->isOpen()){profileRaAccountReturn_=false;profileRoute_="account";profileFocus_=2;}
            emit changed();return;
        }
        if(action==Action::Back && hall_.route()=="sets" && !hall_.account()->isOpen() && !hall_.historyOpen() && !hall_.galleryMenuOpen())profileRoute_="summary";
        else hall_.dispatch(action==Action::LocalAction?Action::ToggleContinue:action);
    } else if(profileRoute_=="story") {
        if(achievementPhotoReturn_ && action==Action::Back && !album_.filtering()) {
            achievementPhotoReturn_=false;profileRoute_="ra";hall_.restoreNavigation(achievementPhotoNavigation_);
        } else if(overviewFace_=="screens" && (action!=Action::Back || album_.filtering()))album_.dispatch(action);
        else if(action==Action::Back) {
            if(overviewFace_=="summary" || profileFocus_==5 || profileFocus_==6)profileRoute_="summary";
            else showOverview("summary");
        } else if(action==Action::Confirm && overviewFace_=="summary")showOverview(overviewFocus_==0?"timeline":"screens");
        else if(action==Action::Up || action==Action::Down || action==Action::Left || action==Action::Right) {
            const int count=overviewFace_=="summary"?2:storyHistory().size();
            const int direction=action==Action::Down || action==Action::Right?1:-1;
            overviewFocus_=std::clamp(overviewFocus_+(overviewFace_=="timeline"?-direction:direction),0,qMax(0,count-1));
        }
    } else if(profileRoute_=="account") {
        if(action==Action::Back){profileRoute_="summary";profileFocus_=4;}
        else if(action==Action::Confirm && profileFocus_<4)activateProfile(profileFocus_);
        else if(action==Action::Left && profileFocus_>=4)profileFocus_=1;
        else if(action==Action::Right && !backendAccount_.progression()["history"].toList().isEmpty())profileFocus_=4;
        else if(action==Action::Up || action==Action::Down) {
            const int low=profileFocus_>=4?4:0;
            const int high=low?3+backendAccount_.progression()["history"].toList().size():backendAccount_.progression()["challenge"].toMap().isEmpty()?2:3;
            profileFocus_=std::clamp(profileFocus_+(action==Action::Up?-1:1),low,qMax(low,high));
        }
    } else if(action==Action::Back)closeProfile();
    else if(action==Action::Confirm)activateProfile(profileFocus_);
    else if(action==Action::Up || action==Action::Left || action==Action::Down || action==Action::Right) {
        QList<int> order{0,1,4,9,10,11,3};
        for(int i=0;i<profileSnapshot_["badges"].toList().size();++i)order.append(7+i);
        order.append(2);if(!profileSnapshot_["latestSession"].toMap().isEmpty())order.append(6);order.append(5);
        const int delta=action==Action::Up || action==Action::Left?-1:1;
        profileFocus_=order[std::clamp(int(order.indexOf(profileFocus_))+delta,0,int(order.size())-1)];
    }
    emit changed();
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
    QVariantList rows;
    if(!social_.homeLobby()["id"].toString().isEmpty())rows.append(QVariantMap{{"id","lobby"},{"label","My lobby"}});
    if(trainer_.exists()&&trainer_.avatarAvailable())rows.append(QVariantMap{{"id","outfits"},{"label","Looks"},{"detail","Quick wardrobe"}});
    if(liveGame_.isEmpty())if(const auto r=repository_.registration(currentAdventureId());r)rows.append(QVariantMap{{"id","pc-settings"},{"label","Game settings"},{"detail",r->adventure.title}});
    if(!liveGame_.isEmpty()) {
        rows.prepend(QVariantMap{{"id","live-options"},{"label","Game Options"},{"detail",liveGame_.value("title")}});
        rows.prepend(QVariantMap{{"id","live-return"},{"label","Return to game"},{"detail","Still running · "+liveGame_.value("title").toString()}});
    }
    if(!voice["channel"].toString().isEmpty())rows.append(QVariantMap{{"id","call"},{"label","Voice call"},{"detail",voice["name"]}});
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
    if(id=="pc-settings"){closeHomeMenu();gamePerformance_.begin(currentAdventureId());return;}
    if(id=="together"){emit togetherRequested(liveGame_.value("id").toString());return;}
    if(id=="lobby"){if(liveGame_.isEmpty())social_.openLobby();else social_.openLobbyForGame(liveGame_.value("id").toString());return;}
    if(id=="live-return"||id=="live-options"){returnToLiveGame(id=="live-options");return;}
    if(id=="profile"){openProfile();return;}
    if(id=="outfits"){if(trainer_.editing()||trainer_.saving())return;closeHomeMenu();openProfile();if(service_=="profile"){profileRoute_="outfits";outfits_.open();emit changed();}return;}
    if(id=="game-party"){closeHomeMenu();openOnlineFace("lobby");emit social_.runtimeAction("multiplayer-party");return;}
    if(id.startsWith("answer-call:")||id.startsWith("decline-call:")) {
        const bool accept=id.startsWith("answer-call:");
        if(social_.answerCall(id.section(':',1),accept,true)&&accept)closeHomeMenu();
        emit changed();return;
    }
    if(homeCallOpen_){if(id=="back"){homeCallOpen_=false;homeMenuSelection_="call";}else social_.controlCall(id);emit changed();return;}
    if(id=="call"){homeCallOpen_=true;homeMenuSelection_="voice-mute";emit changed();return;}
    if(id=="notifications"){notificationsOpen_=true;notificationFocus_=0;notificationSelection_=notifications().value(0).toMap()["id"].toString();emit changed();return;}
    if(id=="chats"&&!social_.notificationFace().isEmpty()){openSocialNotification();return;}
    if (id!="home") socialFace_ = "chats";
    social_.setFace(socialFace_);
    if(id=="home")goToPage(HomePage);else openOnlineFace("chats");
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
    if(row["provider"]=="traineros-prizes"){closeHomeMenu();openProfile();activateProfile(11);backendAccount_.celebratePrizes();return;}
    if(row["provider"]=="traineros-software"){settings_.updates()->dismissNotification();closeHomeMenu();service_="settings";settings_.openUpdates();emit changed();return;}
    if(row["provider"]=="achievements"){dismissNotificationById(row["id"].toString());showNotice(row["name"].toString()+"\n"+row["detail"].toString());closeHomeMenu();return;}
    if(row["answerable"].toBool()) {
        notificationsOpen_=false;homeCallOpen_=false;homeMenuSelection_="answer-call:"+row["id"].toString();emit changed();
        return;
    }
    const auto face=social_.notificationFaceAt(index);if(face.isEmpty())return;
    if(face=="lobby")openGameHubFace("lobby");else {socialFace_=face;openOnlineFace("chats");}
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
    settings_.updates()->dismissNotification();
    achievementNotifications_.clear();achievementToast_.clear();++achievementToastGeneration_;
    social_.dismissNotifications();emit changed();
}
void ShellController::dismissNotificationById(const QString& id) {
    const auto rows=notifications();
    for(int i=0;i<rows.size();++i) {
        const auto row=rows[i].toMap();
        if(row["id"]!=id||row["ringing"].toBool()||row["pending"].toBool())continue;
        if(row["provider"]=="achievements"){
            if(achievementNotifications_.value(0).toMap()["id"]==id){achievementToast_.clear();++achievementToastGeneration_;}
            achievementNotifications_.removeIf([&](const QVariant& v){return v.toMap()["id"]==id;});
        }
        else if(row["provider"]=="traineros-software")settings_.updates()->dismissNotification();
        else social_.dismissNotificationAt(i);
        const auto remaining=notifications();notificationFocus_=qMin(i,int(remaining.size())-1);
        notificationSelection_=remaining.value(notificationFocus_).toMap()["id"].toString();
        emit changed();return;
    }
}
void ShellController::openSocialNotification() {
    if(navigationLocked()||(!homeMenuOpen_&&!canReceiveNearby()))return;
    const auto face=social_.notificationFace();if(face.isEmpty())return;
    if(face=="lobby")openGameHubFace("lobby");else {socialFace_=face;openOnlineFace("chats");}
    social_.openNotification();emit changed();
}
void ShellController::dispatch(Action action) {
    if(action==Action::ToggleKeyboard) {
        if(!notice_.isEmpty())return;
        menuOpen_=homeMenuOpen_=quickAppsOpen_=false;
        if(keyboard_.isOpen()) {
            if(keyboard_.live())keyboard_.cancel();else keyboard_.toggleVisibility();
        } else if(keyboard_.hidden())keyboard_.toggleVisibility();
        else {
            if(page_==AppsPage && appsFace_=="browser")browser_.requestPageKeyboard();
            if(!keyboard_.isOpen()){textTarget_=TextTarget::None;keyboard_.beginLive(0,"Keyboard");}
        }
        return;
    }
    if(social_.dispatchIncomingCall(action))return;
    if(page_==AppsPage && appsFace_=="browser" && service_.isEmpty() && !keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && !drawerOpen_ && !quickAppsOpen_ && notice_.isEmpty() && !localModalOpen()) {
        if(action==Action::Up||action==Action::Down||action==Action::Left||action==Action::Right||action==Action::Confirm||action==Action::ContextMenu||action==Action::Back||action==Action::LocalAction||action==Action::ScrollUp||action==Action::ScrollDown){browser_.dispatch(action);return;}
        if(action==Action::ChangeView||action==Action::ToggleFavorite)return;
    }
    if(social_.files()->picker()->isOpen()){social_.files()->dispatch(action);return;}
    if(service_=="library" && libraryManager_.files()->isOpen() && !keyboard_.isOpen()){libraryManager_.dispatch(action);return;}
    if(gamePerformance_.isOpen() && !keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && notice_.isEmpty()
       && action!=Action::Home && action!=Action::SystemMenu){gamePerformance_.dispatch(action);return;}
    if(pcGames_.isOpen() && !keyboard_.isOpen() && !downloads_.isOpen() && !serviceOpen() && !menuOpen_ && !homeMenuOpen_ && notice_.isEmpty()) {pcGames_.dispatch(action);return;}
    if((social_.myLobbiesOpen() || social_.lobbyCreateOpen() || social_.lobbyPicker()["open"].toBool() || social_.lobbyPicker()["voteOpen"].toBool())&&!keyboard_.isOpen()){social_.dispatch(action);return;}
    if(page_==OnlinePage && onlineFace_=="lobby" && (action==Action::ToggleFavorite || action==Action::ChangeView)) {
        if(pairedNavigationAvailable() && !navigationLocked() && !localModalOpen())social_.dispatch(action);
        return;
    }
    if(page_==OnlinePage && onlineFace_=="chats" && !keyboard_.isOpen() && (action==Action::ToggleFavorite || action==Action::ChangeView)) {
        if(pairedNavigationAvailable() && !navigationLocked() && !localModalOpen())social_.dispatch(action);
        return;
    }
    if(service_=="profile" && (action==Action::ToggleFavorite || action==Action::ChangeView)) {
        if(profileContentActive() && !navigationLocked()) {
            if(profileRoute_=="story" && overviewFace_=="screens" && !album_.filtering()) {
                if(action==Action::ToggleFavorite)album_.toggleFavorite();else album_.switchLibrary();
            } else if(profileRoute_=="ra")hall_.dispatch(action);
        }
        return;
    }
    if(action==Action::ToggleFavorite) {
        if(page_==GameHubPage && hubFace_=="overview" && overviewFace_=="screens") {
            if(pairedNavigationAvailable() && !navigationLocked() && !album_.filtering())album_.toggleFavorite();
        } else toggleLibraryFavorite(multiverse_.detail().value("id").toString());
        return;
    }
    if(action==Action::ChangeView && page_==GameHubPage && hubFace_=="ra") {
        if(!keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && !serviceOpen() && !drawerOpen_ && !quickAppsOpen_ && notice_.isEmpty())hall_.dispatch(action);
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
        if(service_=="profile"&&profileRoute_=="outfits"&&!menuOpen_&&!homeMenuOpen_&&!keyboard_.isOpen()&&!drawerOpen_&&notice_.isEmpty()){
            outfits_.dispatch(action);return;
        }
        if(page_==HomePage && pairedNavigationAvailable() && !navigationLocked()
           && !social_.online()["open"].toBool() && !moduleInvitation()["open"].toBool())
            emit dashboard_.scrollRequested(action==Action::ScrollDown?1:-1);
        else if(page_==GamesPage && !collectionsRoot_ && multiverseFace_ && multiverse_.route()=="games"
                && pairedNavigationAvailable() && !navigationLocked()
                && !social_.online()["open"].toBool() && !moduleInvitation()["open"].toBool())
            emit multiverse_.scrollRequested(action==Action::ScrollDown?1:-1);
        else if(service_.isEmpty() && page_==AppsPage && appsFace_=="files" && !keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && !files_.modal() && !drawerOpen_ && !quickAppsOpen_)files_.dispatch(action);
        return;
    }
    if(keyboard_.isOpen()&&!menuOpen_&&!homeMenuOpen_&&notice_.isEmpty()&&!downloads_.isOpen()&&!social_.online()["open"].toBool()
       &&(action==Action::PreviousFace||action==Action::NextFace)){keyboard_.dispatch(action);return;}
    if(downloads_.isOpen()){downloads_.dispatch(action);return;}
    if(files_.modal()){files_.dispatch(action);return;}
    if(scraper_.isOpen()&&action==Action::SystemMenu&&!keyboard_.isOpen()){menuFocus_=scraper_.selectingSystems()?13:12;scraper_.hide();menuOpen_=true;emit changed();return;}
    if(social_.online()["open"].toBool()) {
        if(action==Action::Confirm)social_.answerOnline(true);
        else if(action==Action::Back)social_.answerOnline(false);
        return;
    }
    for(auto& module:modules_)if(module->intercept(action))return;
    if(scraper_.isOpen()){if(keyboard_.isOpen())keyboard_.dispatch(action);else scraper_.dispatch(action);return;}
    if(libraryTools_.busy()&&libraryTools_.canCancel()){libraryTools_.dispatch(action);return;}
    if(launchPreparation_.busy() || libraryTools_.busy())return;
    if(navigationLocked(action==Action::PreviousPage || action==Action::NextPage) && (action==Action::Home || action==Action::PreviousPage || action==Action::NextPage || action==Action::SystemMenu || action==Action::PreviousFace || action==Action::NextFace))return;
    if(menuOpen_ && !powerMenu_ && notice_.isEmpty()) {
        dispatchControls(action);return;
    }
    if (homeMenuOpen_) {
        if(notificationsOpen_) {
            if(action==Action::Home)closeHomeMenu();
            else if(action==Action::Back){notificationsOpen_=false;emit changed();}
            else if(action==Action::Confirm)activateNotification(notificationFocus());
            else if(action==Action::Secondary) {
                const auto row=notifications().value(notificationFocus()).toMap();
                if(row["ringing"].toBool())social_.answerCall(row["id"].toString(),false);
                else dismissNotificationById(row["id"].toString());
                emit changed();
            }
            else if(action==Action::ContextMenu||action==Action::LocalAction)dismissNotifications();
            else if(action==Action::SystemMenu){homeMenuOpen_=false;optionsReturn_=true;dispatch(action);}
            else if(action==Action::Left){notificationsOpen_=false;emit changed();}
            else if(action==Action::Up||action==Action::Down){notificationFocus_=std::clamp(notificationFocus()+(action==Action::Up?-1:1),0,std::max(0,int(notifications().size())-1));notificationSelection_=notifications().value(notificationFocus_).toMap()["id"].toString();emit changed();}
            return;
        }
        if(action==Action::Back&&homeCallOpen_){homeCallOpen_=false;homeMenuSelection_="call";emit changed();}
        else if (action == Action::Home || action == Action::Back) closeHomeMenu();
        else if (action == Action::Confirm) activateHomeMenu(homeMenuFocus());
        else if (action == Action::Up || action == Action::Down) {
            const auto actions=homeMenuActions();
            const int direction=action==Action::Up?-1:1;
            bool hasAction=false;for(const auto& row:actions)if(row.toMap().value("enabled",true).toBool())hasAction=true;
            if(!hasAction){homeMenuSelection_.clear();notificationsOpen_=true;notificationSelection_=notifications().value(0).toMap()["id"].toString();}
            else for(int index=homeMenuFocus()+direction;index>=0&&index<actions.size();index+=direction)
                if(actions[index].toMap().value("enabled",true).toBool()){homeMenuSelection_=actions[index].toMap()["id"].toString();break;}
            emit changed();
        } else if(action==Action::Right && !homeCallOpen_) {
            notificationsOpen_=true;notificationSelection_=notifications().value(0).toMap()["id"].toString();emit changed();
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
    if(service_.isEmpty() && page_==AppsPage && appsFace_=="files" && !keyboard_.isOpen() && !menuOpen_ && !homeMenuOpen_ && notice_.isEmpty() && !quickAppsOpen_ && !drawerOpen_) {
        if(files_.menuOpen() && action!=Action::PreviousPage && action!=Action::NextPage && action!=Action::PreviousFace && action!=Action::NextFace && action!=Action::Secondary && action!=Action::ToggleContinue && action!=Action::SystemMenu && action!=Action::Home){files_.dispatch(action);return;}
        if(action==Action::LocalAction || action==Action::ContextMenu){files_.dispatch(action);return;}
        if(action==Action::Secondary || action==Action::ToggleContinue)files_.leave();
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
        homeMenuSelection_.clear();homeMenuOpen_ = true; notificationsOpen_=homeCallOpen_=false; homeMenuSelection_ = homeMenuActions().value(0).toMap()["id"].toString();
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
        if(menuOpen_ && !powerMenu_){controls_.setOwner(trainer_.profile()["id"].toString());controls_.begin();}
        else if(!menuOpen_ && optionsReturn_){optionsReturn_=false;homeMenuOpen_=true;}
        emit changed();
        return;
    }
    if (action == Action::PreviousFace || action == Action::NextFace) {
        if(service_=="profile" && !menuOpen_ && !homeMenuOpen_ && !keyboard_.isOpen() && !drawerOpen_ && notice_.isEmpty()) {
            if(profileRoute_=="outfits"&&!trainer_.editing())outfits_.dispatch(action);
            else if(trainer_.avatarEditing())trainer_.dispatch(Action::Back,true);
            else if(profileRoute_=="summary" && !trainer_.editing())activateProfile(1);
            return;
        }
        if(service_=="settings" && !menuOpen_ && !homeMenuOpen_ && !keyboard_.isOpen() && !drawerOpen_ && notice_.isEmpty()) {
            if(!hall_.account()->isOpen() && !trainer_.editing())settings_.dispatch(action);
            return;
        }
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
        if(service_=="profile") {dispatchProfile(action);return;}
        if (service_ == "trainer-settings") {
            if (hall_.account()->isOpen()) hall_.account()->dispatch(action);
            else if (action == Action::Back) service_ = "settings";
            else if (action == Action::Confirm) { trainerSettingsAction(trainerSettingsFocus_); return; }
            else if (action == Action::Up) trainerSettingsFocus_ = std::max(0, trainerSettingsFocus_ - 1);
            else if (action == Action::Down) trainerSettingsFocus_ = std::min(4, trainerSettingsFocus_ + 1);
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
            if(multiverseFace_ && multiverse_.systemId()=="pc" && multiverse_.route()=="games") {
                if(action==Action::Up && multiverse_.focusIndex()==0){pcAddFocused_=true;emit changed();return;}
                if(pcAddFocused_ && action==Action::Down){pcAddFocused_=false;emit changed();return;}
                if(action==Action::Confirm && (pcAddFocused_ || multiverse_.games().isEmpty())){pcGames_.begin();return;}
            }
            if(action==Action::Back) {
                pcAddFocused_=false;
                collectionsRoot_=true;emit changed();return;
            }
            if (multiverseFace_) multiverse_.dispatch(action); else worlds_.dispatch(action);
            return;
        }
        if(dispatchGameHub(action))return;
        if (page_ == AppsPage) {
            if(appsFace_=="apps")apps_.dispatch(action);
            else if(appsFace_=="files")files_.dispatch(action);
            else if(action==Action::Back)selectFace(0);
            return;
        }
        if (page_ == OnlinePage) {
            if(onlineFace_=="crates"){if(!crates_.dispatch(action)&&action==Action::Back)goToPage(HomePage);}
            else if(onlineFace_=="bazaar"){if(action==Action::Back)goToPage(HomePage);}
            else if(onlineFace_=="chats" && chatNavigationFocused_) {
                if(action==Action::Left || action==Action::Right) {
                    const QStringList faces{"chats","communities","friends"};
                    selectChatFace(faces[(qMax(0,faces.indexOf(socialFace_))+(action==Action::Right?1:2))%3]);
                    chatNavigationFocused_=true;
                } else if(action==Action::Down || action==Action::Confirm)chatNavigationFocused_=false;
                else if(action==Action::Back)goToPage(HomePage);
                emit changed();
            } else if(onlineFace_=="chats" && action==Action::Up && !social_.reading()
                      && (social_.focusIndex()<0 || social_.account()["state"]!="connected"
                          || (socialFace_=="friends" && social_.discover()["area"]=="search" && social_.discoverPreview().isEmpty()))) {
                chatNavigationFocused_=true;emit changed();
            } else social_.dispatch(action);
            return;
        }
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
