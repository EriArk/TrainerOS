#pragma once
#include "core/input/Action.h"
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
#include "features/settings/SettingsController.h"
#include "features/device/DeviceController.h"
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
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool drawerOpen READ drawerOpen NOTIFY changed)
    Q_PROPERTY(bool chooseAdventureAvailable READ chooseAdventureAvailable NOTIFY changed)
    Q_PROPERTY(bool pairedNavigationAvailable READ pairedNavigationAvailable NOTIFY changed)
    Q_PROPERTY(bool centerFace READ centerFace NOTIFY changed)
    Q_PROPERTY(bool multiverseFace READ multiverseFace NOTIFY changed)
    Q_PROPERTY(bool multiverseHome READ multiverseHome NOTIFY changed)
    Q_PROPERTY(trainer::MultiversePresentation* multiverse READ multiverse CONSTANT)
    Q_PROPERTY(QString currentAdventureId READ currentAdventureId NOTIFY changed)
    Q_PROPERTY(bool menuOpen READ menuOpen NOTIFY changed)
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
    TextEntryController* keyboard() { return &keyboard_; }
    TrainerController* trainer() { return &trainer_; }
    WorldsController* worlds() { return &worlds_; }
    MultiversePresentation* multiverse() { return &multiverse_; }
    bool multiverseFace() const { return multiverseFace_; }
    bool multiverseHome() const { return multiverseHome_; }
    PokedexController* pokedex() { return &pokedex_; }
    HallOfFameController* hall() { return &hall_; }
    LibraryManagementController* libraryManager() { return &libraryManager_; }
    SettingsController* settings() { return &settings_; }
    DeviceController* device() { return &device_; }
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
    void showNotice(const QString& message) { mode_.clear(); notice_ = message; emit changed(); }
    bool modeConfirmation() const { return !mode_.isEmpty(); }
    int page() const { return page_; }
    int focusIndex() const;
    bool drawerOpen() const { return drawerOpen_; }
    bool chooseAdventureAvailable();
    bool pairedNavigationAvailable();
    bool centerFace() const { return page_ == 2 && centerFace_; }
    QString currentAdventureId() const;
    bool menuOpen() const { return menuOpen_; }
    bool powerMenu() const { return powerMenu_; }
    QString notice() const { return notice_; }
    QVariantMap home() const;
    QVariantList resumePoints() const;
    QStringList menuItems() const;
    void dispatch(Action);
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
    Q_INVOKABLE void goToPage(int page);
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
    void trainerSettingsAction(int);
    void refreshContinue();
    bool localModalOpen();
    void openCenter();
    std::optional<Adventure> homeAdventure() const;
    std::optional<ResumePoint> homeResumePoint(const QString& adventureId) const;
    ResumeAvailability homeResumeAvailability(const Adventure&) const;
    LibraryRepository& repository_;
    QPointer<GameProgressProvider> progress_;
    AdventureAdapter& adapter_;
    PlatformService& platform_;
    TextEntryController keyboard_;
    TrainerController trainer_;
    TrainerSetupPresentation trainerSetup_;
    int trainerSettingsFocus_ = 0;
    bool trainerChooserFromPower_ = false;
    WorldsController worlds_;
    MultiversePresentation multiverse_;
    PokedexController pokedex_;
    HallOfFameController hall_;
    LibraryManagementController libraryManager_;
    SettingsController settings_;
    DeviceController device_;
    DiagnosticsController diagnostics_;
    SaveCenterController center_;
    PartyPresentation party_;
    QString service_;
    enum class TextTarget { None, TrainerName, PokedexSearch, WorldsSearch, MultiverseSearch, Library, Archive, PokedexNote, TrainerFavorite, CenterSearch, AchievementAccount, SetupName };
    TextTarget textTarget_ = TextTarget::None;
    QList<ContinueEntry> points_;
    QString homeAdventureId_, homeResumeId_;
    ResumeSource homeResumeSource_;
    int page_ = 0;
    int drawerFocus_ = 0;
    int menuFocus_ = 0;
    int menuServiceFocus_ = 0;
    bool drawerOpen_ = false;
    bool centerFace_ = false;
    bool multiverseFace_ = false, multiverseHome_ = false;
    int multiverseDrawerFocus_ = 0;
    bool menuOpen_ = false;
    bool powerMenu_ = false;
    bool libraryFromWorlds_ = false;
    QString notice_;
    QString mode_;
};
}
