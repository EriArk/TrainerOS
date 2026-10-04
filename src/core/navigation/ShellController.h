#pragma once
#include "core/input/Action.h"
#include "features/social/SocialController.h"
#include "LaunchPreparation.h"
#include "core/input/TextEntryController.h"
#include "core/repository/LibraryRepository.h"
#include "core/model/GameProgressProvider.h"
#include <QPointer>
#include "features/trainer/TrainerController.h"
#include "features/trainer/TrainerSetupPresentation.h"
#include "features/worlds/WorldsController.h"
#include "features/worlds/MultiversePresentation.h"
#include "features/pokedex/PokedexController.h"
#include "features/halloffame/HallOfFameController.h"
#include "features/library/LibraryManagementController.h"
#include "features/library/LibraryToolsController.h"
#include "features/settings/SettingsController.h"
#include "features/device/DeviceController.h"
#include "features/device/NetworkController.h"
#include "features/diagnostics/DiagnosticsController.h"
#include "features/center/SaveCenterController.h"
#include "features/center/PartyPresentation.h"
#include "integrations/adventure/AdventureAdapter.h"
#include "platform/PlatformService.h"
#include <QObject>
#include <QVariantList>

namespace trainer {
class ShellController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(trainer::LibraryToolsController* libraryTools READ libraryTools CONSTANT)
    Q_PROPERTY(bool canHoldConfirm READ canHoldConfirm NOTIFY changed)
    Q_PROPERTY(bool canEditWorld READ canEditWorld NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(QStringList primaryNames READ primaryNames CONSTANT)
    Q_PROPERTY(QString trainerFace READ trainerFace NOTIFY changed)
    Q_PROPERTY(bool trainerHistoryFace READ trainerHistoryFace NOTIFY changed)
    Q_PROPERTY(trainer::SocialController* social READ social CONSTANT)
    Q_PROPERTY(QString socialFace READ socialFace NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool drawerOpen READ drawerOpen NOTIFY changed)
    Q_PROPERTY(bool chooseAdventureAvailable READ chooseAdventureAvailable NOTIFY changed)
    Q_PROPERTY(bool pairedNavigationAvailable READ pairedNavigationAvailable NOTIFY changed)
    Q_PROPERTY(QString pokemonFace READ pokemonFace NOTIFY changed)
    Q_PROPERTY(QStringList faceNames READ faceNames NOTIFY changed)
    Q_PROPERTY(int faceIndex READ faceIndex NOTIFY changed)
    Q_PROPERTY(bool centerFace READ centerFace NOTIFY changed)
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
    Q_PROPERTY(trainer::PokedexController* pokedex READ pokedex CONSTANT)
    Q_PROPERTY(trainer::HallOfFameController* hall READ hall CONSTANT)
    Q_PROPERTY(trainer::LibraryManagementController* libraryManager READ libraryManager CONSTANT)
    Q_PROPERTY(trainer::SettingsController* settings READ settings CONSTANT)
    Q_PROPERTY(trainer::DeviceController* device READ device CONSTANT)
    Q_PROPERTY(trainer::NetworkController* network READ network CONSTANT)
    Q_PROPERTY(trainer::DiagnosticsController* diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(trainer::SaveCenterController* center READ center CONSTANT)
    Q_PROPERTY(trainer::PartyPresentation* party READ party CONSTANT)
    Q_PROPERTY(QString service READ service NOTIFY changed)
    Q_PROPERTY(bool serviceOpen READ serviceOpen NOTIFY changed)
    Q_PROPERTY(bool sampleLibrary READ sampleLibrary CONSTANT)
    Q_PROPERTY(QVariantList resumePoints READ resumePoints NOTIFY changed)
    Q_PROPERTY(QStringList menuItems READ menuItems NOTIFY changed)
    Q_PROPERTY(trainer::TrainerSetupPresentation* trainerSetup READ trainerSetup CONSTANT)
    Q_PROPERTY(bool powerMenu READ powerMenu NOTIFY changed)
public:
    ShellController(LibraryRepository&, TrainerRepository&, AdventureAdapter&, PlatformService&,
                    PokedexReferenceProvider&, PokedexProgressRepository&, HallOfFameRepository&,
                    AchievementProvider&, QObject* parent = nullptr);
    TrainerSetupPresentation* trainerSetup() { return &trainerSetup_; }
    void setOnboardingConnections(bool active) { if(onboardingConnections_==active)return; onboardingConnections_=active; emit changed(); }
    SocialController* social() { return &social_; }
    TextEntryController* keyboard() { return &keyboard_; }
    Q_PROPERTY(bool unobstructed READ canReceiveNearby NOTIFY changed)
    bool canReceiveNearby() { return !homeMenuOpen_ && !menuOpen_ && !drawerOpen_ && !serviceOpen() && notice_.isEmpty()
        && !localModalOpen() && !keyboard_.isOpen() && !party_.activities()->practice()->running(); }
    bool canReceiveOnline() { return !menuOpen_ && !drawerOpen_ && !serviceOpen() && notice_.isEmpty()
        && (!localModalOpen() || (page_==4 && !social_.menu().isEmpty()))
        && !keyboard_.isOpen() && !party_.activities()->practice()->running(); }
    TrainerController* trainer() { return &trainer_; }
    WorldsController* worlds() { return &worlds_; }
    MultiversePresentation* multiverse() { return &multiverse_; }
    bool collectionsRoot() const { return collectionsRoot_; }
    QVariantList collections() const { return multiverse_.collections(); }
    bool multiverseFace() const { return multiverseFace_; }
    bool multiverseHome() const { return multiverseHome_; }
    PokedexController* pokedex() { return &pokedex_; }
    HallOfFameController* hall() { return &hall_; }
    LibraryManagementController* libraryManager() { return &libraryManager_; }
    LibraryToolsController* libraryTools() {return &libraryTools_;}
    bool canHoldConfirm() const;
    bool canEditWorld() const;
    SettingsController* settings() { return &settings_; }
    DeviceController* device() { return &device_; }
    NetworkController* network() { return &network_; }
    DiagnosticsController* diagnostics() { return &diagnostics_; }
    SaveCenterController* center() { return &center_; }
    PartyPresentation* party() { return &party_; }
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
    QStringList primaryNames() const { return {"Home", "Worlds", "Companions", "Trainer", "Social"}; }
    QString trainerFace() const;
    bool trainerHistoryFace() const { return page_ == 3 && !trainerProfile_; }
    QString socialFace() const { return socialFace_; }
    int focusIndex() const;
    bool drawerOpen() const { return drawerOpen_; }
    bool chooseAdventureAvailable();
    bool pairedNavigationAvailable();
    bool centerFace() const { return page_ == 2 && pokemonFace_ != "dex"; }
    QString pokemonFace() const { return pokemonFace_; }
    QStringList faceNames() const;
    int faceIndex() const;
    QString currentAdventureId() const;
    bool menuOpen() const { return menuOpen_; }
    bool homeMenuOpen() const { return homeMenuOpen_; }
    int homeMenuFocus() const;
    QVariantList homeMenuActions() const;
    QString homeMenuCaption() const;
    bool notificationsOpen() const { return homeMenuOpen_ && notificationsOpen_; }
    int notificationFocus() const { return qBound(0,notificationFocus_,qMax(0,int(social_.notifications().size())-1)); }
    Q_INVOKABLE void activateNotification(int index);
    Q_INVOKABLE void activateHomeMenu(int index);
    Q_INVOKABLE void openSocialNotification();
    Q_INVOKABLE void closeHomeMenu();
    bool powerMenu() const { return powerMenu_; }
    QString notice() const { return notice_; }
    bool runtimeChangeBlocked() const { return navigationLocked(); }
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
    void changed();
    void exitRequested();
    void trainersRequested();
    void pinRequested(bool family);
    void modeRequested(const QString& mode);
    void homeLaunchPressed();
private:
    void confirm();
    void openCollection(int index);
    void cycleCollection(int delta);
    void trainerSettingsAction(int);
    void refreshContinue();
    bool localModalOpen();
    void openCenter();
    void showPokemonFace(const QString& face);
    void showTrainerFace(const QString& face);
    bool navigationLocked(bool primaryRecovery = false) const;
    void refreshParty();
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
    PokedexController pokedex_;
    HallOfFameController hall_;
    LibraryManagementController libraryManager_;
    LibraryToolsController libraryTools_;
    SettingsController settings_;
    DeviceController device_;
    NetworkController network_;
    DiagnosticsController diagnostics_;
    SaveCenterController center_;
    PartyPresentation party_;
    SocialController social_;
    QString service_;
    enum class TextTarget { None, TrainerName, PokedexSearch, WorldsSearch, MultiverseSearch, Library, LibraryTools, Archive, TrainerFavorite, CenterSearch, ShopSearch, BoxName, AchievementAccount, SetupName, Network, Social, Communication };
    TextTarget textTarget_ = TextTarget::None;
    QList<ContinueEntry> points_;
    QString homeAdventureId_, homeResumeId_;
    ResumeSource homeResumeSource_;
    int page_ = 0;
    bool trainerProfile_ = true;
    QString socialFace_ = "chats";
    int drawerFocus_ = 0;
    int menuFocus_ = 0;
    int menuServiceFocus_ = 0;
    bool drawerOpen_ = false;
    QString pokemonFace_ = "dex";
    QString centerRoute_ = "clinic", playroomRoute_ = "playroom";
    bool collectionsRoot_ = true;
    int collectionFocus_ = 0;
    QString homeCollection_ = "pokemon", worldCollection_ = "pokemon";
    bool multiverseFace_ = false, multiverseHome_ = false;
    int multiverseDrawerFocus_ = 0;
    bool menuOpen_ = false;
    bool homeMenuOpen_ = false;
    bool notificationsOpen_ = false;
    bool homeCallOpen_ = false;
    int notificationFocus_ = 0;
    QString homeMenuSelection_ = "home";
    bool powerMenu_ = false;
    bool libraryFromWorlds_ = false;
    QString notice_;
    QString achievementToast_;
    quint64 achievementToastGeneration_ = 0;
    QString mode_;
};
}
