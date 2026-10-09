#pragma once
#include "core/input/Action.h"
#include "features/social/SocialController.h"
#include "LaunchPreparation.h"
#include "core/input/TextEntryController.h"
#include "core/repository/LibraryRepository.h"
#include "core/model/GameProgressProvider.h"
#include "core/experience/ExperienceModule.h"
#include <QPointer>
#include "features/trainer/TrainerController.h"
#include "features/trainer/TrainerSetupPresentation.h"
#include "features/worlds/WorldsController.h"
#include "features/worlds/MultiversePresentation.h"
#include "features/halloffame/HallOfFameController.h"
#include "features/library/LibraryManagementController.h"
#include "features/library/LibraryToolsController.h"
#include "features/library/ScrapeController.h"
#include "features/downloads/DownloadsController.h"
#include "features/settings/SettingsController.h"
#include "features/device/DeviceController.h"
#include "features/device/NetworkController.h"
#include "features/diagnostics/DiagnosticsController.h"
#include "integrations/adventure/AdventureAdapter.h"
#include "platform/PlatformService.h"
#include <QObject>
#include <QVariantList>

namespace trainer {
class ShellController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(trainer::LibraryToolsController* libraryTools READ libraryTools CONSTANT)
    Q_PROPERTY(trainer::ScrapeController* scraper READ scraper CONSTANT)
    Q_PROPERTY(trainer::DownloadsController* downloads READ downloads CONSTANT)
    Q_PROPERTY(bool canHoldConfirm READ canHoldConfirm NOTIFY changed)
    Q_PROPERTY(bool canEditWorld READ canEditWorld NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(QStringList primaryNames READ primaryNames NOTIFY changed)
    Q_PROPERTY(QString homeView READ homeView NOTIFY changed)
    Q_PROPERTY(QString experienceView READ experienceView NOTIFY changed)
    Q_PROPERTY(QVariantList gameHistory READ gameHistory NOTIFY changed)
    Q_PROPERTY(QVariantMap liveGame READ liveGame NOTIFY changed)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY changed)
    Q_PROPERTY(QVariantMap passiveNotice READ passiveNotice NOTIFY changed)
    Q_PROPERTY(QString trainerFace READ trainerFace NOTIFY changed)
    Q_PROPERTY(trainer::SocialController* social READ social CONSTANT)
    Q_PROPERTY(QString socialFace READ socialFace NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool drawerOpen READ drawerOpen NOTIFY changed)
    Q_PROPERTY(bool chooseAdventureAvailable READ chooseAdventureAvailable NOTIFY changed)
    Q_PROPERTY(bool pairedNavigationAvailable READ pairedNavigationAvailable NOTIFY changed)
    Q_PROPERTY(QStringList faceNames READ faceNames NOTIFY changed)
    Q_PROPERTY(int faceIndex READ faceIndex NOTIFY changed)
    Q_PROPERTY(trainer::GameCollections* collectionManager READ collectionManager CONSTANT)
    Q_PROPERTY(QVariantMap homeGame READ homeGame NOTIFY changed)
    Q_PROPERTY(bool collectionsRoot READ collectionsRoot NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(bool multiverseFace READ multiverseFace NOTIFY changed)
    Q_PROPERTY(bool multiverseHome READ multiverseHome NOTIFY changed)
    Q_PROPERTY(trainer::MultiversePresentation* multiverse READ multiverse CONSTANT)
    Q_PROPERTY(QString currentAdventureId READ currentAdventureId NOTIFY changed)
    Q_PROPERTY(bool menuOpen READ menuOpen NOTIFY changed)
    Q_PROPERTY(bool homeMenuOpen READ homeMenuOpen NOTIFY changed)
    Q_PROPERTY(int homeMenuFocus READ homeMenuFocus NOTIFY changed)
    Q_PROPERTY(QVariantList homeMenuActions READ homeMenuActions NOTIFY changed)
    Q_PROPERTY(QString homeMenuCaption READ homeMenuCaption NOTIFY changed)
    Q_PROPERTY(bool notificationsOpen READ notificationsOpen NOTIFY changed)
    Q_PROPERTY(int notificationFocus READ notificationFocus NOTIFY changed)
    Q_PROPERTY(QString achievementToast READ achievementToast NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(bool modeConfirmation READ modeConfirmation NOTIFY changed)
    Q_PROPERTY(QVariantMap home READ home NOTIFY changed)
    Q_PROPERTY(trainer::TextEntryController* keyboard READ keyboard CONSTANT)
    Q_PROPERTY(trainer::TrainerController* trainer READ trainer CONSTANT)
    Q_PROPERTY(trainer::WorldsController* worlds READ worlds CONSTANT)
    Q_PROPERTY(trainer::HallOfFameController* hall READ hall CONSTANT)
    Q_PROPERTY(trainer::LibraryManagementController* libraryManager READ libraryManager CONSTANT)
    Q_PROPERTY(trainer::SettingsController* settings READ settings CONSTANT)
    Q_PROPERTY(trainer::DeviceController* device READ device CONSTANT)
    Q_PROPERTY(trainer::NetworkController* network READ network CONSTANT)
    Q_PROPERTY(trainer::DiagnosticsController* diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(QString service READ service NOTIFY changed)
    Q_PROPERTY(bool serviceOpen READ serviceOpen NOTIFY changed)
    Q_PROPERTY(bool sampleLibrary READ sampleLibrary CONSTANT)
    Q_PROPERTY(QVariantList resumePoints READ resumePoints NOTIFY changed)
    Q_PROPERTY(QStringList menuItems READ menuItems NOTIFY changed)
    Q_PROPERTY(trainer::TrainerSetupPresentation* trainerSetup READ trainerSetup CONSTANT)
    Q_PROPERTY(bool powerMenu READ powerMenu NOTIFY changed)
public:
    ShellController(LibraryRepository&, TrainerRepository&, AdventureAdapter&, PlatformService&,
                    ExperienceFactory, HallOfFameRepository&,
                    AchievementProvider&, QObject* parent = nullptr);
    Q_PROPERTY(QObject* experienceModel READ experienceModel NOTIFY changed)
    Q_PROPERTY(QUrl experiencePresenter READ experiencePresenter NOTIFY changed)
    Q_PROPERTY(QVariantMap moduleInvitation READ moduleInvitation NOTIFY changed)
    Q_PROPERTY(bool moduleControlsBlocked READ moduleControlsBlocked NOTIFY changed)
    QObject* experienceModel() const { return activeModule_; }
    QUrl experiencePresenter() const { return activeModule_?activeModule_->presenter():QUrl(); }
    ExperienceModule* module(const QString& id) const;
    QVariantList liveExperienceActions(const QString& game,const QString& session);
    bool invokeLiveExperienceAction(const QString& action,const QString& game,const QString& session);
    QVariantMap moduleInvitation() const;
    bool moduleControlsBlocked() const { return modulesBlocked() || moduleInvitation()["open"].toBool(); }
    void reloadModules() { for(auto& module:modules_)module->reload(); }
    TrainerSetupPresentation* trainerSetup() { return &trainerSetup_; }
    void setOnboardingConnections(bool active) { if(onboardingConnections_==active)return; onboardingConnections_=active; emit changed(); }
    SocialController* social() { return &social_; }
    TextEntryController* keyboard() { return &keyboard_; }
    Q_PROPERTY(bool unobstructed READ canReceiveNearby NOTIFY changed)
    bool canReceiveNearby() { return !homeMenuOpen_ && !menuOpen_ && !drawerOpen_ && !serviceOpen() && notice_.isEmpty()
        && !localModalOpen() && !keyboard_.isOpen() && !moduleActivityBusy(); }
    bool canReceiveOnline() { return !menuOpen_ && !drawerOpen_ && !serviceOpen() && notice_.isEmpty()
        && (!localModalOpen() || (page_==4 && !social_.menu().isEmpty()))
        && !keyboard_.isOpen() && !moduleActivityBusy(true); }
    TrainerController* trainer() { return &trainer_; }
    WorldsController* worlds() { return &worlds_; }
    MultiversePresentation* multiverse() { return &multiverse_; }
    bool collectionsRoot() const { return collectionsRoot_; }
    QVariantList collections() const { return multiverse_.collections(); }
    bool multiverseFace() const { return multiverseFace_; }
    bool multiverseHome() const { return homeView()=="game-home"; }
    QVariantMap homeGame() const { return multiverse_.game(currentAdventureId()); }
    GameCollections* collectionManager() { return multiverse_.collectionManager(); }
    Q_INVOKABLE void manageCollection(bool create=false);
    Q_INVOKABLE void editCollection(int index);
    HallOfFameController* hall() { return &hall_; }
    LibraryManagementController* libraryManager() { return &libraryManager_; }
    LibraryToolsController* libraryTools() {return &libraryTools_;}
    ScrapeController* scraper() {return &scraper_;}
    DownloadsController* downloads() {return &downloads_;}
    bool canHoldConfirm() const;
    bool canEditWorld() const;
    SettingsController* settings() { return &settings_; }
    DeviceController* device() { return &device_; }
    NetworkController* network() { return &network_; }
    DiagnosticsController* diagnostics() { return &diagnostics_; }
    QString service() const { return service_; }
    bool serviceOpen() const { return !service_.isEmpty(); }
    bool sampleLibrary() const { return !repository_.editable(); }
    void openTrainers();
    void configureServices(FileCatalog* files, PreferencesRepository* preferences);
    void configureProgress(GameProgressProvider* provider);
    void refreshLibrary();
    QString achievementToast() const {return achievementToast_;}
    void showAchievements(const QString& title,const QStringList& names);
    void showNotice(const QString& message) { mode_.clear(); notice_ = message; emit changed(); }
    bool modeConfirmation() const { return !mode_.isEmpty(); }
    int page() const { return page_; }
    QStringList primaryNames() const;
    QString homeView() const {return experience_.descriptor().homeView;}
    QString experienceView() const {return page_==2 || page_==3 ? experience_.view(page_-2) : QString();}
    QVariantList gameHistory() const;
    QVariantMap liveGame() const { return liveGame_; }
    QVariantList notifications() const;
    QVariantMap passiveNotice() const;
    void setLiveGame(const QVariantMap& game) { if(liveGame_!=game){liveGame_=game;emit changed();} }
    Q_INVOKABLE void returnToLiveGame(bool options=false) { if(!liveGame_.isEmpty())emit liveGameRequested(options); }
    Q_INVOKABLE void openContext(const QString& type={}, const QString& id={});
    Q_INVOKABLE void pressButton(const QString& button);
    Q_INVOKABLE void activateHomeAction(const QString& id);
    QString trainerFace() const;
    QString socialFace() const { return socialFace_; }
    int focusIndex() const;
    bool drawerOpen() const { return drawerOpen_; }
    bool chooseAdventureAvailable();
    bool pairedNavigationAvailable();
    QStringList faceNames() const;
    int faceIndex() const;
    QString currentAdventureId() const;
    bool menuOpen() const { return menuOpen_; }
    bool homeMenuOpen() const { return homeMenuOpen_; }
    int homeMenuFocus() const;
    QVariantList homeMenuActions() const;
    QString homeMenuCaption() const;
    bool notificationsOpen() const { return homeMenuOpen_ && notificationsOpen_; }
    int notificationFocus() const;
    Q_INVOKABLE void activateNotification(int index);
    Q_INVOKABLE void openNotificationById(const QString& id);
    Q_INVOKABLE void dismissNotifications();
    Q_INVOKABLE void activateHomeMenu(int index);
    Q_INVOKABLE void openSocialNotification();
    Q_INVOKABLE void closeHomeMenu();
    Q_INVOKABLE void openProfile();
    bool powerMenu() const { return powerMenu_; }
    QString notice() const { return notice_; }
    bool moduleServiceBusy() const { return moduleWriting() || modulesBlocked(); }
    bool runtimeChangeBlocked() const { return scraper_.busy() || moduleWriting() || navigationLocked(); }
    QVariantMap home() const;
    QVariantList resumePoints() const;
    QStringList menuItems() const;
    void dispatch(Action);
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
    Q_INVOKABLE void goToPage(int page);
    Q_INVOKABLE void goToTrainerFace(const QString& face);
    Q_INVOKABLE void activate(int index, const QString& area = {});
signals:
    void liveGameRequested(bool options);
    void togetherRequested(QString game);
    void changed();
    void exitRequested();
    void trainersRequested();
    void pinRequested(bool family);
    void modeRequested(const QString& mode);
    void homeLaunchPressed();
private:
    ExperienceModule* activeModule_ = nullptr;
    QPointer<ExperienceModule> textModule_;
    quint64 textGeneration_ = 0;
    bool modulesBlocked(bool recovery=false) const;
    bool moduleWriting() const;
    bool moduleActivityBusy(bool recovery=false) const;
    QVariantMap liveGame_;
    QVariantList achievementNotifications_;
    void syncExperience();
    ExperienceModule* resolveExperience(const std::optional<Adventure>&) const;
    void showExperienceFace(int slot,const QString& face);
    ExperienceNavigation experience_;
    bool syncingExperience_ = false;
    void confirm();
    void openCollection(int index);
    void cycleCollection(int delta);
    void trainerSettingsAction(int);
    void refreshContinue();
    bool localModalOpen();
    void showTrainerFace(const QString& face);
    bool navigationLocked(bool primaryRecovery = false) const;
    void refreshExperience();
    std::optional<Adventure> homeAdventure() const;
    std::optional<ResumePoint> homeResumePoint(const QString& adventureId) const;
    ResumeAvailability homeResumeAvailability(const Adventure&) const;
    LibraryRepository& repository_;
    QPointer<GameProgressProvider> progress_;
    AdventureAdapter& adapter_;
    LaunchPreparation launchPreparation_{repository_, adapter_, this};
    PlatformService& platform_;
    TextEntryController keyboard_;
    TrainerController trainer_;
    TrainerSetupPresentation trainerSetup_;
    bool onboardingConnections_ = false;
    int trainerSettingsFocus_ = 0;
    bool trainerChooserFromPower_ = false;
    WorldsController worlds_;
    MultiversePresentation multiverse_;
    HallOfFameController hall_;
    LibraryManagementController libraryManager_;
    LibraryToolsController libraryTools_;
    ScrapeController scraper_;
    DownloadsController downloads_;
    SettingsController settings_;
    DeviceController device_;
    NetworkController network_;
    DiagnosticsController diagnostics_;
    SocialController social_;
    QString service_;
    enum class TextTarget { None, TrainerName, WorldsSearch, MultiverseSearch, Library, LibraryTools, Collections, Archive, Module, AchievementAccount, SetupName, Network, Social, Communication, Scraper };
    TextTarget textTarget_ = TextTarget::None;
    QList<ContinueEntry> points_;
    QString homeAdventureId_, homeResumeId_;
    ResumeSource homeResumeSource_;
    int page_ = 0;
    QString socialFace_ = "chats";
    int drawerFocus_ = 0;
    int menuFocus_ = 0;
    int menuServiceFocus_ = 0;
    bool drawerOpen_ = false;
    bool collectionsRoot_ = true;
    int collectionFocus_ = 0;
    QString homeCollection_ = "pokemon", worldCollection_ = "pokemon";
    bool multiverseFace_ = true, multiverseHome_ = false;
    int multiverseDrawerFocus_ = 0;
    bool menuOpen_ = false;
    bool homeMenuOpen_ = false;
    bool optionsReturn_ = false;
    bool notificationsOpen_ = false;
    bool homeCallOpen_ = false;
    int notificationFocus_ = 0;
    QString notificationSelection_;
    QString homeMenuSelection_ = "home";
    bool powerMenu_ = false;
    bool libraryFromWorlds_ = false;
    QString notice_;
    QString achievementToast_;
    quint64 achievementToastGeneration_ = 0;
    QString mode_;
    struct LiveModuleAction { ExperienceModule* module; ExperienceLiveContext context; QString action; };
    QHash<QString,LiveModuleAction> liveModuleActions_;
    ExperienceModules modules_; // Destroy modules before the host services they reference.
};
}
