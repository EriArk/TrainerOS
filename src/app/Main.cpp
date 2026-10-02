#include "core/PerformanceTrace.h"
#include "integrations/achievements/TrainerAchievementProvider.h"
#include "integrations/achievements/RetroArchAchievementSession.h"
#include "platform/emulation/EmulatorDiscovery.h"
#include "platform/emulation/EmulatorRefresh.h"
#include <atomic>
#include "core/repository/BatoceraLibrary.h"
#include "platform/device/VolumeKeys.h"
#include "core/input/ControllerInput.h"
#include "core/input/PointerVisibility.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/AdapterRouter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include "integrations/adventure/standalone/MelonDsSave.h"
#include "integrations/progress/GameProgressService.h"
#include <QSettings>
#include "integrations/progress/Gen3Progress.h"
#include <QJsonDocument>
#include "integrations/achievements/RetroAchievementsProvider.h"
#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include "core/navigation/AdventureLaunchController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "integrations/adventure/retroarch/RetroArchAppearance.h"
#include "platform/input/AdventureOverlayService.h"
#include "features/home/PlayHistoryController.h"
#include "features/home/ExitImage.h"
#include "features/pokedex/ClassicArt.h"
#include "features/pokedex/SpriteImages.h"
#include "core/repository/CollectionRepository.h"
#include "core/repository/OfflinePokedex.h"
#include "integrations/adventure/retroarch/RetroArchSave.h"
#include "platform/storage/SaveBackupStorage.h"
#include "platform/storage/LinkSaveStore.h"
#include <QSaveFile>
#include <QUuid>
#include <QSysInfo>
#include "platform/power/PowerStatus.h"
#include "platform/ArmadaPlatformService.h"
#include <QCryptographicHash>
#include <QQuickImageProvider>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QCommandLineParser>
#include <QImage>
#include <QTimer>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFontInfo>
#include <QFontDatabase>
#include <QStandardPaths>
#include <memory>
#ifdef TRAINEROS_UI_TESTS
#include "WorldsSmokeScenario.h"
#include "PokedexSmokeScenario.h"
#include "ArtworkSmokeScenario.h"
#include "HallSmokeScenario.h"
#include "PersistenceSmokeScenario.h"
#include "LibrarySmokeScenario.h"
#include "LaunchSmokeScenario.h"
#include "HomeSmokeScenario.h"
#include "DiagnosticsSmokeScenario.h"
#include "CenterSmokeScenario.h"
#include "ExitSmokeScenario.h"
#endif

using namespace trainer;

class SavedExitImages final : public QQuickImageProvider {
public:
    explicit SavedExitImages(LocalStateStore* store) : QQuickImageProvider(Image), store_(store) {}
    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        PerformanceTrace::Scope perf("SavedExitImages.requestImage");
        const auto frame = frameExitImage(store_ ? store_->exitImage(id) : QImage{});
        if (size) *size = frame.size();
        return frame;
    }
private:
    LocalStateStore* store_;
};
class ExitFrameImages final : public QQuickImageProvider {
public:
    explicit ExitFrameImages(AdventureExitPresentation& controller) : QQuickImageProvider(Image), controller_(controller) {}
    QImage requestImage(const QString&, QSize* size, const QSize&) override {
        const auto frame = controller_.frame();
        if (size) *size = frame.size();
        return frame;
    }
private:
    AdventureExitPresentation& controller_;
};

int main(int argc, char* argv[]) {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    QGuiApplication app(argc, argv);
    // Qt's generic "Sans Serif" can resolve to a decorative face with tiny
    // numerals. Prefer an installed UI font consistently across all QML text.
    app.setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    const auto installedFonts = QFontDatabase::families();
    for (const auto& family : {QString("Noto Sans"), QString("DejaVu Sans"), QString("Segoe UI")}) {
        if (installedFonts.contains(family)) { app.setFont(QFont(family)); break; }
    }
    app.setApplicationName("TrainerOS");
    app.setOrganizationName("TrainerOS");
    app.setDesktopFileName("org.traineros.TrainerOS");
    app.setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("TrainerOS native shell prototype. Safe application mode.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"windowed", "Run in a development window instead of full-screen."});
    parser.addOption({"data-dir", "Use an explicit local data folder (development / isolated validation).", "directory"});
    parser.addOption({"roms-dir", "Use a Batocera-compatible ROM folder (default: ~/Emulation/roms).", "directory"});
    parser.addOption({"ephemeral", "Use isolated in-memory sample data; do not open a persistent store."});
    parser.addOption({"sprite-dir", "Use optional private sprite detail assets.", "directory"});
    parser.addOption({"art-dir", "Use a private development artwork bootstrap directory.", "directory"});
    parser.addOption({"smoke-test", "Verify the QML shell with an isolated SDL virtual controller, then exit."});
#ifdef TRAINEROS_UI_TESTS
    parser.addOption({"exit-smoke-test", "Verify the isolated Adventure exit window through SDL input, then exit."});
    parser.addOption({"worlds-smoke-test", "Verify Worlds browsing and mock actions through SDL input, then exit."});
    parser.addOption({"pokedex-smoke-test", "Verify Pokédex filters, search and records through SDL input, then exit."});
    parser.addOption({"art-smoke-test", "Exercise private artwork bootstrap on the complete offline reference, then exit."});
    parser.addOption({"hall-smoke-test", "Verify Hall of Fame archive and achievement states through SDL input, then exit."});
    parser.addOption({"diagnostics-smoke-test", "Verify controller/display checks and a local report through SDL input, then exit."});
    parser.addOption({"persistence-smoke-test", "Verify persistent controller flows in a test data directory.", "phase"});
#endif
    parser.addOption({"screenshot-dir", "Save smoke-test screenshots to this directory.", "directory"});
    parser.process(app);
    bool worldsSmoke = false;
    bool pokedexSmoke = false;
    bool artSmoke = false;
    bool hallSmoke = false;
    bool diagnosticsSmoke = false;
    bool exitSmoke = false;
    QString persistencePhase;
#ifdef TRAINEROS_UI_TESTS
    exitSmoke = parser.isSet("exit-smoke-test");
    worldsSmoke = parser.isSet("worlds-smoke-test");
    pokedexSmoke = parser.isSet("pokedex-smoke-test");
    artSmoke = parser.isSet("art-smoke-test");
    hallSmoke = parser.isSet("hall-smoke-test");
    diagnosticsSmoke = parser.isSet("diagnostics-smoke-test");
    persistencePhase = parser.value("persistence-smoke-test");
    if (parser.isSet("persistence-smoke-test") && (!QStringList{"seed", "verify", "error", "library-seed", "library-verify", "library-final", "library-launch", "library-home", "library-home-reopen", "library-center", "collection"}.contains(persistencePhase)
            || parser.value("data-dir").isEmpty() || parser.isSet("ephemeral"))) return 2;
#endif
    const bool smoke = parser.isSet("smoke-test") || artSmoke || exitSmoke || worldsSmoke || pokedexSmoke || hallSmoke || diagnosticsSmoke || !persistencePhase.isEmpty();
    auto smokeBattery = std::make_shared<std::atomic_int>(65);
    PowerStatus powerStatus([smoke, smokeBattery] {
        if (!smoke) return systemBatteryStatus();
        const int value = smokeBattery->load();
        return value < 0 ? BatterySnapshot{} : BatterySnapshot{value % 1000, value >= 1000 ? "Charging" : "Discharging"};
    });
    powerStatus.start();
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &powerStatus, [&powerStatus](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) powerStatus.refresh();
    });

    // This virtual device exercises exactly the same polling/mapping path as hardware.
    int virtualIndex = -1;
    SDL_Joystick* joystick = nullptr;
    SDL_JoystickID preferred = -1;
    if (smoke) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return 2;
        virtualIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                               SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        if (virtualIndex < 0 || !(joystick = SDL_JoystickOpen(virtualIndex))) {
            qCritical() << "Cannot create the smoke-test controller:" << SDL_GetError();
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
            return 2;
        }
        preferred = SDL_JoystickInstanceID(joystick);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    }
    app.setQuitOnLastWindowClosed(false);
    bool restartTrainer = false;
    QString trainerGrant;
    bool chooseTrainer = false;
    bool sessionReadyNotified = false;
    int result = 0;
    bool smokeCompleted = false;
    do {
        restartTrainer = false;
        MockLibraryRepository repository;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        UnconfiguredAdventureAdapter unconfiguredAdapter;
        LocalFileCatalog files;
        ArmadaPlatformService platform(!smoke && !parser.isSet("ephemeral"));
        MockPokedexRepository dex;
        OfflinePokedex offlineDex;
        if (pokedexSmoke) dex.failNextLoad();
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        if (hallSmoke || diagnosticsSmoke) shellAchievements.enableAccountPreview();
        std::unique_ptr<LocalStateStore> store;
        QString stateDirectory;
        if ((!smoke && !parser.isSet("ephemeral")) || !persistencePhase.isEmpty()) {
            const auto directory = parser.isSet("data-dir") ? QDir(parser.value("data-dir")).absolutePath()
                : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            stateDirectory = directory;
            store = std::make_unique<LocalStateStore>(directory, nullptr, smoke && !persistencePhase.startsWith("library-") ? "prototype-library-v1" : "user-library-v1");
        }
        if(store && !smoke)store->enforceAccess(trainerGrant,chooseTrainer);
        chooseTrainer=false;
        trainerGrant.clear();
        const bool personalLibrary = store && (!smoke || persistencePhase.startsWith("library-") || persistencePhase == "collection");
        CollectionRepository collection(personalLibrary ? static_cast<LibraryRepository&>(*store) : repository);
        LibraryRepository& baseLibrary = personalLibrary ? (!smoke || persistencePhase == "collection" ? static_cast<LibraryRepository&>(collection) : *store) : repository;
        // State thumbnails are migration evidence, not normal launch targets.
        const auto libraryRoot = parser.isSet("roms-dir") ? QDir(parser.value("roms-dir")).absolutePath()
            : FirstRunController::libraryRoot(stateDirectory, QDir::home().filePath("Emulation/roms"));
        BatoceraLibrary folders(baseLibrary, libraryRoot);
        LibraryRepository& activeLibrary = personalLibrary && !smoke ? static_cast<LibraryRepository&>(folders) : baseLibrary;
        std::unique_ptr<TrainerAchievementProvider> realAchievements;
        if (personalLibrary && !smoke) realAchievements = std::make_unique<TrainerAchievementProvider>(activeLibrary);
        AdventureAdapter* selectedAdapter = personalLibrary ? static_cast<AdventureAdapter*>(&unconfiguredAdapter) : &adapter;
        const auto emulatorEnvironment = personalLibrary && !smoke
            ? installedEmulators(stateDirectory, libraryRoot) : EmulatorEnvironment{};
        const auto discovered = personalLibrary && !smoke
            ? prepareEmulators(emulatorEnvironment) : EmulatorDiscovery{};
        for (const auto& notice : discovered.notices) qWarning().noquote() << "Emulator preparation:" << notice;
        if (personalLibrary && !smoke) qInfo() << "Emulator integration snapshots:" << discovered.profiles.keys();
        auto retroarchInstallation = RetroArchInstallation::fromJson(discovered.profiles.value("retroarch"));
        if(personalLibrary && !smoke) {
            retroarchInstallation.saves=std::make_shared<RetroArchSaveSession>();
            retroarchInstallation.lineageRoot=QDir(stateDirectory).filePath("backups");
            QObject::connect(store.get(), &LocalStateStore::opened, store.get(),
                [&, saves=retroarchInstallation.saves](bool success) {
                    if(success)saves->bind({store->ownerId(),stateDirectory,store->usesLegacyStorage()});
                });
        }
        RetroArchAdapter retroarch(activeLibrary, retroarchInstallation);
        const auto standaloneInstallation = [&](const QString& id) {
            return StandaloneInstallation::fromJson(discovered.profiles.value(id), id);
        };
        const auto melonDsInstallation = standaloneInstallation("melonds");
        StandaloneAdapter melonDs("melonds", activeLibrary, melonDsInstallation);
        StandaloneAdapter dolphin("dolphin", activeLibrary, standaloneInstallation("dolphin"));
        StandaloneAdapter ppsspp("ppsspp", activeLibrary, standaloneInstallation("ppsspp"));
        StandaloneAdapter armsx2("armsx2", activeLibrary, standaloneInstallation("armsx2"));
        struct SaveRuntimes { RetroArchInstallation retroarch; StandaloneInstallation melonds; };
        auto saveRuntimes = std::make_shared<std::atomic<std::shared_ptr<const SaveRuntimes>>>(
            std::make_shared<const SaveRuntimes>(SaveRuntimes{retroarchInstallation, melonDsInstallation}));
        AdapterRouter adapters({&ppsspp, &armsx2, &melonDs, &dolphin, &retroarch});
        if (personalLibrary && !smoke) selectedAdapter = &adapters;
#ifdef TRAINEROS_UI_TESTS
        ProbeAdventureAdapter probeAdapter;
        if (persistencePhase == "library-launch" || persistencePhase.startsWith("library-home")) selectedAdapter = &probeAdapter;
#endif
        ShellController shell(activeLibrary,
                              store ? static_cast<TrainerRepository&>(*store) : profiles,
                              *selectedAdapter, platform,
                              (personalLibrary && !smoke) || artSmoke ? static_cast<PokedexReferenceProvider&>(offlineDex) : dex,
                              store ? static_cast<PokedexProgressRepository&>(*store) : dex,
                              personalLibrary && !smoke ? static_cast<HallOfFameRepository&>(*store) : shellArchive,
                              realAchievements ? static_cast<AchievementProvider&>(*realAchievements) : shellAchievements);
        shell.configureServices(&files, store.get());
        if(personalLibrary && !smoke && store) {
            QObject::connect(store.get(), &LocalStateStore::opened, &shell, [&](bool ready) {
                shell.social()->setOwner(ready ? store->ownerId() : QString());
            });
            QObject::connect(store.get(), &LocalStateStore::accessNeeded, &shell, [&] {
                shell.social()->setOwner({});
            });
        }
        if (personalLibrary && !smoke) {
            folders.prepareInstallation = [&adapters](AdventureRegistration& record) { adapters.prepareInstallation(record); };
            folders.prepareFileMove = [saveRuntimes](const AdventureRegistration& record,LibraryEdit& edit)->QString {
                return retroarch::prepareFileMove(record,edit,saveRuntimes->load()->retroarch);
            };
            QObject::connect(store.get(), &LocalStateStore::opened, &folders, [&](bool ready) { if(ready)folders.refreshContentAvailability(); });
            shell.settings()->setLibraryScanState(true,false);
            QObject::connect(&folders, &BatoceraLibrary::busyChanged, &shell, [&] {
                shell.settings()->setLibraryScanState(true,folders.busy());
            });
            QObject::connect(&folders, &BatoceraLibrary::scanFinished, &shell, [&](int added,const QStringList& warnings) {
                shell.refreshLibrary();
                qInfo() << "Library discovery:" << added << "updated;" << warnings;
                const QString result=warnings.isEmpty()
                    ? added?QString("Library updated · %1 items").arg(added):QString("Library is up to date")
                    : QString("Refresh incomplete · %1").arg(warnings.first());
                shell.settings()->setLibraryScanState(true,folders.busy(),result);
            });
            shell.libraryManager()->prepareInstallation = [&adapters](AdventureRegistration& record) { adapters.prepareInstallation(record); };
            shell.libraryManager()->setInitialFolder(QDir::home().filePath("Emulation/roms"));
        }
        if (store) QObject::connect(store.get(), &LocalStateStore::libraryChanged, &shell, [&] {
            // A folder import can bind dozens of games. Rebuild pages once when
            // that batch finishes instead of after every individual SQLite write.
            if(!folders.busy())shell.refreshLibrary();
        });
        if(realAchievements)QObject::connect(realAchievements.get(),&AchievementProvider::achievementsEarned,&shell,&ShellController::showAchievements);
        if (realAchievements) QObject::connect(store.get(), &LocalStateStore::opened, realAchievements.get(), [&](bool success) {
            if (success) realAchievements->bind(store->accountDirectory());
        });
        SessionState session(shell, store.get());
        if(personalLibrary && !smoke) {
            session.firstRun()->configure(stateDirectory,libraryRoot);
            const auto applyLibraryRoot=[&](const QString& root)->QString {
                if(folders.busy() || folders.writing() || store->pending() || store->opening())return "The library is busy. Wait a moment and try again.";
                const auto error=saveLibraryRoot(stateDirectory,root);
                if(!error.isEmpty())return error;
                folders.setRoot(root);
                shell.libraryManager()->setInitialFolder(root);
                shell.settings()->storage()->configure(root);
                return {};
            };
            session.firstRun()->useLibraryRoot=applyLibraryRoot;
            shell.settings()->storage()->configure(libraryRoot);
            shell.settings()->storage()->apply=[&,applyLibraryRoot](const QString& root)->QString {
                const auto error=applyLibraryRoot(root);
                if(error.isEmpty())folders.rescan();
                return error;
            };
            shell.libraryManager()->setInitialFolder(libraryRoot);
        }
        session.setTrainerSwitchGuard([&]{return !realAchievements || !realAchievements->accountBusy();});
        session.setTrainerRemovalPreparation([&]() -> QString {
            if(!realAchievements)return {};
            return realAchievements->disconnectForRemoval()?QString():"Could not remove the saved sign-in. Try again.";
        });
        QObject::connect(&session,&SessionState::trainerRestartReady,&app,[&]{trainerGrant=session.restartGrant();chooseTrainer=trainerGrant.isEmpty();restartTrainer=true;app.exit();});
        DeviceSnapshot deviceFixture;
        deviceFixture.volume = 35; deviceFixture.brightness = 60; deviceFixture.network = "Connected";
        deviceFixture.internalFree = 32LL << 30; deviceFixture.internalTotal = 100LL << 30;
        deviceFixture.libraryFree = 200LL << 30; deviceFixture.libraryTotal = 400LL << 30;
        DeviceService deviceService(!smoke && !parser.isSet("ephemeral")
            ? systemDeviceBackend(stateDirectory, QDir::home().filePath("Emulation"))
            : DeviceBackend{[&deviceFixture] { return deviceFixture; }, [&deviceFixture](const QString& control, int value) {
                if (control == "volume") deviceFixture.volume = value;
                else if (control == "brightness") deviceFixture.brightness = value;
                else if (control == "mute") deviceFixture.muted = value != 0;
                return QString();
            }});
        shell.device()->configure(&deviceService, platform.canSwitchSession());
        NetworkService networkService;
        if(!smoke && !parser.isSet("ephemeral")) shell.network()->configure(&networkService);
        VolumeKeys volumeKeys(!smoke && !parser.isSet("ephemeral") && platform.dedicatedSession());
        QObject::connect(&volumeKeys, &VolumeKeys::adjustmentRequested, &deviceService,
            [&deviceService](int delta) { deviceService.hardwareVolume(delta); });

        std::unique_ptr<LocalSaveBackupService> saveBackups;
        if (personalLibrary && !smoke) {
            saveBackups=std::make_unique<LocalSaveBackupService>(QDir(stateDirectory).filePath("backups"),
                [saveRuntimes](const AdventureRegistration& record){const auto snapshot=saveRuntimes->load(); return record.adventure.adapterId == "melonds"
                    ? resolveMelonDsSave(record, snapshot->melonds) : resolveRetroArchSave(record,snapshot->retroarch);},
                [saveRuntimes](const AdventureRegistration& record){const auto snapshot=saveRuntimes->load(); return supportsMelonDsSave(record, snapshot->melonds)
                    || (snapshot->retroarch.saveBackups && record.adventure.adapterId=="retroarch" && record.integrationConfig["core"].toString()=="mgba");});
        }
#ifdef TRAINEROS_UI_TESTS
        if(persistencePhase=="library-center") {
            saveBackups=std::make_unique<LocalSaveBackupService>(QDir(stateDirectory).filePath("backups"),
                [](const AdventureRegistration& record){
                    QFile content(record.contentPath); if(!content.open(QIODevice::ReadOnly))return SaveTarget{};
                    const auto revision=QString::fromLatin1(QCryptographicHash::hash(content.readAll(),QCryptographicHash::Sha256).toHex());
                    return SaveTarget{record.adventure.id,record.adventure.title,record.contentPath+".srm",revision,record.contentPath,{},true};
                },[](const AdventureRegistration& record){return record.adventure.adapterId=="backup-fixture";});
            saveBackups->configureHealing([](const QByteArray&,const QString&){return SaveHealing{"HEALED SAVE",{},3};});
        }
#endif
        if(saveBackups) {
            if (!smoke) {
                saveBackups->configureHealing(healGen3Party);
                saveBackups->configureMovement(moveEmeraldPokemon);
                saveBackups->configureRelease(releaseEmeraldPokemon);
                saveBackups->configureHeldItems(changeEmeraldHeldItem);
                saveBackups->configureBoxNames(renameEmeraldBox);
                saveBackups->configureShops(readEmeraldShops,buyEmeraldItems);
            }
            shell.center()->configure(saveBackups.get());
            shell.party()->configureMovement(saveBackups.get(),&activeLibrary);
            shell.settings()->configureSavePolicy(saveBackups.get());
            QObject::connect(saveBackups.get(),&SaveBackupService::operationFailed,&session,&SessionState::cancelPendingExit);
        }
        const auto updateServiceActivity = [&] {
            session.setServiceActive(shell.party()->activities()->link()->navigationBlocked() || folders.writing() || shell.settings()->storage()->busy() || deviceService.busy() || shell.network()->busy() || (saveBackups && saveBackups->busy()));
        };
        QObject::connect(&folders, &BatoceraLibrary::writingChanged, &session, updateServiceActivity);
        QObject::connect(&deviceService, &DeviceService::changed, &session, updateServiceActivity);
        QObject::connect(shell.network(), &NetworkController::changed, &session, updateServiceActivity);
        QObject::connect(shell.settings()->storage(), &LibraryStorageController::changed, &session, updateServiceActivity);
        if (saveBackups) QObject::connect(saveBackups.get(), &SaveBackupService::busyChanged, &session, updateServiceActivity);
        QObject::connect(shell.party()->activities()->link(),&LinkController::changed,&session,updateServiceActivity);
        ProcessService adventureProcess;
        AdventureLaunchController adventureLaunch(adventureProcess);
        AdventureExitPresentation exitPresentation(adventureLaunch.exitController());
        auto gameNotifications=[&]{
            QVariantList rows;
            for(const auto& value:shell.social()->notifications()) {
                const auto row=value.toMap();
                rows.append(QVariantMap{{"id","notice:"+row["id"].toString()},
                    {"label",row["name"]},{"detail",row["detail"]},{"readOnly",true}});
            }
            const bool empty=rows.isEmpty();
            if(!empty)rows.append(QVariantMap{{"id","dismiss-notifications"},{"label","Dismiss notifications"}});
            rows.append(QVariantMap{{"id","back"},{"label","Back"}});
            exitPresentation.setPanel("notifications",empty?"You're all caught up!":"Your updates",rows);
        };
        auto gameMenuActions=[&]{
            QVariantList actions{QVariantMap{{"id","notifications"},{"label","Notifications · "+QString::number(shell.social()->notifications().size())}}};
            if(adventureProcess.runtimeControls()["kind"]=="retroarch")actions.append(QVariantMap{{"id","display"},{"label","Screen & graphics"}});
            if(!shell.social()->account()["voice"].toMap()["channel"].toString().isEmpty())actions.append(QVariantMap{{"id","call"},{"label","Voice call"},{"detail",shell.social()->account()["voice"].toMap()["name"]}});
            actions=shell.social()->incomingCallActions()+actions;
            exitPresentation.setExtraActions(actions);
            if(exitPresentation.menuOpen()&&exitPresentation.panel()=="notifications")gameNotifications();
            if(exitPresentation.menuOpen()&&exitPresentation.panel()=="call") {
                const auto voice=shell.social()->account()["voice"].toMap();
                QVariantList rows;
                if(!voice["channel"].toString().isEmpty())rows={QVariantMap{{"id","voice-mute"},{"label",voice["muted"].toBool()?"Turn microphone on":"Mute microphone"}},
                    QVariantMap{{"id","voice-output"},{"label",voice["deaf"].toBool()?"Enable call sound":"Silence call sound"}},QVariantMap{{"id","voice-leave"},{"label","Leave call"}}};
                rows.append(QVariantMap{{"id","back"},{"label","Back"}});exitPresentation.setPanel("call",voice["summary"].toString(),rows);
            }
        };
        QObject::connect(shell.social(),&SocialController::changed,&exitPresentation,gameMenuActions);
        QObject::connect(&adventureLaunch,&AdventureLaunchController::adventureStarted,&exitPresentation,gameMenuActions);
        QObject::connect(shell.social(),&SocialController::backgroundNotification,&exitPresentation,[&](QString title,QString text){
            adventureProcess.runtimeCommand("notify",title+" · "+text);
        });
        QObject::connect(&exitPresentation,&AdventureExitPresentation::menuActionRequested,&exitPresentation,[&](const QString& action){
            const auto back=QVariantMap{{"id","back"},{"label","Back"}};
            const auto runtime=adventureProcess.runtimeControls();
            if(action.startsWith("answer-call:")||action.startsWith("decline-call:")) {
                shell.social()->answerCall(action.section(':',1),action.startsWith("answer-call:"));return;
            }
            if(action=="notifications") {
                gameNotifications();return;
            }
            if(action=="dismiss-notifications"){shell.social()->dismissNotifications();gameNotifications();return;}
            if(action=="call"||action.startsWith("voice-")) {
                if(action!="call")shell.social()->controlCall(action);
                const auto voice=shell.social()->account()["voice"].toMap();
                QVariantList rows{QVariantMap{{"id","voice-mute"},{"label",voice["muted"].toBool()?"Turn microphone on":"Mute microphone"}},
                    QVariantMap{{"id","voice-output"},{"label",voice["deaf"].toBool()?"Enable call sound":"Silence call sound"}},
                    QVariantMap{{"id","voice-leave"},{"label","Leave call"}},back};
                exitPresentation.setPanel("call",voice["summary"].toString(),rows);return;
            }
            if(runtime["kind"]!="retroarch")return;
            const auto game=runtime["game"].toString();
            if(action=="choose-shader"||action=="choose-bezel") {
                const auto family=action=="choose-shader"?QString("shader"):QString("bezel");
                auto rows=retroarch::appearanceChoices(game,family,runtime);
                rows.append(QVariantMap{{"id","display"},{"label","Back"}});
                exitPresentation.setPanel("display-choice",family=="shader"?"Choose shader · next launch":"Choose frame · next launch",rows,"display");return;
            }
            QString caption="Display changes apply next launch";
            if(action=="display"&&!runtime["appearanceNotice"].toString().isEmpty())caption=runtime["appearanceNotice"].toString();
            if(action=="shader"||action=="next-shader"||action=="previous-shader")caption=adventureProcess.runtimeCommand(action)?"Shader preview":"Couldn't change the shader";
            else if(action.startsWith("shader:")||action.startsWith("bezel:")){if(!retroarch::chooseAppearance(game,action,runtime))caption="Couldn't save display settings";}
            else if(action!="display"&&!retroarch::changeAppearance(runtime["game"].toString(),action))caption="Couldn't save display settings";
            auto rows=retroarch::appearanceActions(runtime["game"].toString());
            rows.append(QVariantMap{{"id","shader"},{"label","Toggle shader now"}});rows.append(QVariantMap{{"id","next-shader"},{"label","Next shader now"}});rows.append(back);
            exitPresentation.setPanel("display",caption,rows);
        });
        QObject::connect(&adventureLaunch, &AdventureLaunchController::adventureStarted, &exitPresentation,
            [&](const QString& id) {
                QString title = "Adventure";
                for (const auto& adventure : activeLibrary.adventures())
                    if (adventure.id == id) { title = adventure.title; break; }
                exitPresentation.setGameTitle(title);
            });
        std::unique_ptr<AdventureOverlayService> adventureOverlay;
#ifdef Q_OS_LINUX
        if (personalLibrary && !smoke && platform.dedicatedSession()) {
            QFile configuration(QDir(stateDirectory).filePath("integrations/overlay.json"));
            QString overlayHelper;
            const QFileInfo configurationInfo(configuration.fileName());
            if (!configurationInfo.exists() && !configurationInfo.isSymLink())
                overlayHelper = "/var/opt/traineros/integrations/adventure-overlay.py";
            if (configuration.open(QIODevice::ReadOnly) && configuration.size() <= 8192) {
                const auto object = QJsonDocument::fromJson(configuration.readAll()).object();
                if (object["version"].toInt() == 1 && object["enabled"].toBool())
                    overlayHelper = object["helper"].toString();
            }
            if (QFileInfo(overlayHelper).isFile())
                adventureOverlay = std::make_unique<AdventureOverlayService>(adventureProcess, adventureLaunch,
                    exitPresentation, overlayHelper);
        }
#endif
        std::unique_ptr<GameProgressService> gameProgress;
        QByteArray progressSelection;
        bool progressHomeVisible = false;
        if (personalLibrary && !smoke) {
            gameProgress = std::make_unique<GameProgressService>([saveRuntimes](const AdventureRegistration& record) {
                return resolveRetroArchSave(record, saveRuntimes->load()->retroarch);
            });
            shell.configureProgress(gameProgress.get());
            auto* practice=shell.party()->activities()->practice();
            practice->configureRuntime(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/practice/emerald-v1");
            const PracticeController::Verifier verifyParty=[&,provider=gameProgress.get()](const PracticeSource& source,const GameProgress& expected,QObject* receiver,std::function<void(bool)> done) {
                const auto record=activeLibrary.registration(source.adventureId);
                if(!record || source.trainerId!=store->ownerId() || source.adventureId!=shell.currentAdventureId()
                    || adventureLaunch.active() || (saveBackups && saveBackups->busy())) {done(false);return;}
                provider->verifySnapshot(*record,expected,receiver,[&,source,done=std::move(done)](bool matches){
                    done(matches && source.trainerId==store->ownerId() && source.adventureId==shell.currentAdventureId()
                        && !adventureLaunch.active() && !(saveBackups && saveBackups->busy()));
                });
            };
            practice->configureVerification(verifyParty);
            if(saveBackups) {
                const auto identityPath=QDir(stateDirectory).filePath("link-device-id");QFile identity(identityPath);QString deviceId;
                if(identity.open(QIODevice::ReadOnly))deviceId=QString::fromUtf8(identity.read(100)).trimmed();
                if(QUuid(deviceId).isNull()) {
                    deviceId=QUuid::createUuid().toString(QUuid::WithoutBraces);QSaveFile file(identityPath);
                    if(!file.open(QIODevice::WriteOnly) || file.write(deviceId.toUtf8())<0 || !file.commit())deviceId.clear();
                }
                auto* link=shell.party()->activities()->link();
                QObject::connect(link,&LinkController::connectionFailed,&shell,&ShellController::showNotice);
                const QString linkName=shell.trainer()->profile().value("name").toString();
                link->configure(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/practice/emerald-v1",
                    deviceId,linkName,[&,service=saveBackups.get()](const QString& op,const QJsonObject& args,QObject* receiver,std::function<void(QJsonObject)> done){
                        const auto record=activeLibrary.registration(args["adventure"].toString());
                        if(adventureLaunch.active() || !record){done({{"error","Close the game and select the Emerald Adventure used for this trade."}});return;}
                        service->linkOperation(*record,op,args,receiver,std::move(done));
                    },verifyParty,linkSaveStatus(QDir(stateDirectory).filePath("backups")));
                updateServiceActivity();
                const auto nearbyGate=[&,link]{
                    link->setInvitationsAllowed(!adventureLaunch.active() && !session.blocked()
                        && shell.canReceiveNearby());
                    shell.social()->setOnlineContext(!adventureLaunch.active() && !session.blocked()
                        && (!link->active() || link->pending()) && !link->connected()
                        && shell.canReceiveOnline(),
                        !shell.settings()->readOnlySaves());
                };
                QObject::connect(&adventureLaunch,&AdventureLaunchController::changed,link,nearbyGate);
                QObject::connect(&session,&SessionState::changed,link,nearbyGate);
                QObject::connect(&shell,&ShellController::changed,link,nearbyGate);
                QObject::connect(link,&LinkController::changed,link,nearbyGate);
                nearbyGate();
                QObject::connect(link,&LinkController::saveChanged,gameProgress.get(),[&,provider=gameProgress.get()]{
                    if(const auto r=activeLibrary.registration(shell.currentAdventureId()))provider->refresh(*r);
                });
            }
            const auto refreshProgress = [&, provider = gameProgress.get()](bool force) {
                if (adventureLaunch.active() || (saveBackups && saveBackups->busy())) return;
                const auto id = shell.currentAdventureId();
                const auto record = activeLibrary.registration(id);
                if (!record) {
                    if (!progressSelection.isEmpty()) { progressSelection.clear(); provider->invalidate(); }
                    return;
                }
                const auto key = QJsonDocument(QJsonObject{{"id", id}, {"owner", store->ownerId()}, {"revision", record->revision},
                    {"path", record->contentPath}, {"config", record->integrationConfig}}).toJson(QJsonDocument::Compact);
                if (!force && key == progressSelection) return;
                const bool sameContext = key == progressSelection;
                progressSelection = key;
                provider->refresh(*record, sameContext);
            };
            QObject::connect(&shell, &ShellController::changed, gameProgress.get(), [&, refreshProgress] {
                const bool homeVisible = shell.page() == 0 || shell.page() == 2;
                const bool enteredHome = homeVisible && !progressHomeVisible;
                progressHomeVisible = homeVisible;
                refreshProgress(enteredHome);
            });
            QObject::connect(store.get(), &LocalStateStore::opened, gameProgress.get(), [refreshProgress](bool success) {
                if (success) refreshProgress(true);
            });
            QObject::connect(store.get(), &LocalStateStore::libraryChanged, gameProgress.get(), [&,refreshProgress] { if(!folders.busy())refreshProgress(true); });
            QObject::connect(&folders, &BatoceraLibrary::scanFinished, gameProgress.get(), [refreshProgress] { refreshProgress(true); });
            QObject::connect(&adventureLaunch, &AdventureLaunchController::changed, gameProgress.get(), [&, refreshProgress] {
                if (adventureLaunch.active()) { progressSelection.clear(); gameProgress->invalidate(); }
                else refreshProgress(true);
            });
            if (saveBackups) QObject::connect(saveBackups.get(), &SaveBackupService::busyChanged, gameProgress.get(), [&, refreshProgress] {
                if (saveBackups->busy()) { progressSelection.clear(); gameProgress->invalidate(); }
                else refreshProgress(true);
            });
        }
        std::unique_ptr<PlayHistoryController> playHistory;
        if (personalLibrary) playHistory = std::make_unique<PlayHistoryController>(adventureLaunch, *store);
        if (playHistory) playHistory->setMediaSource([&](const QString& id) -> std::optional<ExitMediaSource> {
            const auto profile = store->load();
            const auto record = activeLibrary.registration(id);
            if (!profile || !record) return {};
            return ExitMediaSource{profile->id, record->adventure.domain, *record};
        });
        std::unique_ptr<EmulatorRefresh> emulatorRefresh;
        QTimer emulatorPoll;
        if (personalLibrary && !smoke) {
            emulatorRefresh = std::make_unique<EmulatorRefresh>([stateDirectory, emulatorEnvironment](const QString& root) {
                // SDL input stays on its original thread; inventory only reads
                // filesystem/Flatpak state. Existing mappings are never replaced.
                auto environment = installedEmulators(stateDirectory, root, false);
                environment.controllerName = emulatorEnvironment.controllerName;
                environment.controllerButtons = emulatorEnvironment.controllerButtons;
                return environment;
            });
            emulatorRefresh->idle = [&] {
                return !adventureLaunch.active() && !shell.runtimeChangeBlocked() && !session.blocked() && !folders.busy()
                    && !store->pending() && !store->opening() && !shell.libraryManager()->saving()
                    && !shell.libraryTools()->busy() && !shell.settings()->storage()->busy()
                    && !(saveBackups && saveBackups->busy());
            };
            emulatorRefresh->apply = [&](const EmulatorEnvironment& environment, bool scanLibrary) {
                if (environment.libraryRoot != folders.root()) {
                    emulatorRefresh->request(folders.root(), scanLibrary); return;
                }
                const auto prepared = prepareEmulators(environment);
                for (const auto& notice : prepared.notices) qWarning().noquote() << "Emulator refresh:" << notice;
                auto next = RetroArchInstallation::fromJson(prepared.profiles.value("retroarch"));
                next.saves = retroarchInstallation.saves;
                next.lineageRoot = retroarchInstallation.lineageRoot;
                const auto standalone = [&](const QString& id) { return StandaloneInstallation::fromJson(prepared.profiles.value(id), id); };
                const auto ds = standalone("melonds");
                bool changed = retroarch.updateInstallation(next);
                changed |= melonDs.updateInstallation(ds);
                changed |= dolphin.updateInstallation(standalone("dolphin"));
                changed |= ppsspp.updateInstallation(standalone("ppsspp"));
                changed |= armsx2.updateInstallation(standalone("armsx2"));
                if (changed) {
                    retroarchInstallation = next;
                    saveRuntimes->store(std::make_shared<const SaveRuntimes>(SaveRuntimes{next, ds}));
                    progressSelection.clear(); gameProgress->invalidate();
                    qInfo() << "Emulator readiness refreshed at idle";
                    shell.refreshLibrary();
                }
                if (scanLibrary || changed) folders.refreshContentAvailability();
            };
            const auto refreshEmulators = [&] { emulatorRefresh->request(folders.root(), true); };
            QObject::connect(shell.settings(), &SettingsController::libraryRefreshRequested, emulatorRefresh.get(), refreshEmulators);
            QObject::connect(&app, &QGuiApplication::applicationStateChanged, emulatorRefresh.get(), [&](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive) emulatorRefresh->request(folders.root());
            });
            auto previousPage = std::make_shared<int>(shell.page());
            QObject::connect(&shell, &ShellController::changed, emulatorRefresh.get(), [&, previousPage, refreshEmulators] {
                const auto previous = *previousPage; *previousPage = shell.page();
                if (shell.page() == 1 && previous != 1) refreshEmulators();
            });
            emulatorPoll.setInterval(60000);
            QObject::connect(&emulatorPoll, &QTimer::timeout, emulatorRefresh.get(), [&] { emulatorRefresh->request(folders.root()); });
            emulatorPoll.start();
        }
        ControllerInput input(nullptr, preferred);
        bool homeMenuWasOpen = false;
        QObject::connect(&shell, &ShellController::changed, &input, [&] {
            if (homeMenuWasOpen == shell.homeMenuOpen()) return;
            homeMenuWasOpen = shell.homeMenuOpen(); input.requireNeutral();
        });
        QObject::connect(&shell,&ShellController::changed,&input,[&]{input.setHoldConfirmEnabled(shell.canHoldConfirm());});
        input.setHoldConfirmEnabled(shell.canHoldConfirm());
        const auto reportBase = parser.isSet("data-dir") ? QDir(parser.value("data-dir")).absolutePath()
            : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        ClassicArt classicArt(parser.isSet("art-dir") ? parser.value("art-dir") : smoke || parser.isSet("ephemeral")
            ? QString() : QDir(reportBase).filePath("artwork/bootstrap"));
        shell.pokedex()->configureArtwork(&classicArt);
        shell.hall()->configureArtwork(&classicArt);
        shell.trainer()->picker()->setArtwork(&classicArt);
        DiagnosticsService deviceReports(diagnosticsSmoke ? QDir(parser.value("screenshot-dir")).absoluteFilePath("reports")
                                                         : QDir(reportBase).filePath("diagnostics"));
        shell.diagnostics()->configure(&input, &deviceReports);
        QObject::connect(&input, &ControllerInput::action, &session, [&](Action action) {
            // An exit prompt must never navigate the hidden shell. The platform
            // input provider feeds its own isolated snapshots to the presenter.
            if (adventureLaunch.exitController().phase() != AdventureExitController::Phase::Idle) return;
            if (adventureLaunch.active()) {
                if (adventureLaunch.preparing() && action == Action::Back) adventureLaunch.cancel();
                return;
            }
            session.dispatch(action);
        });
        QString pendingMode;
        bool transitionStarted = false;
        QObject::connect(&shell, &ShellController::modeRequested, &session, [&](const QString& mode) {
            if (adventureLaunch.active()) return;
            pendingMode = mode; session.requestExit();
        });
        QObject::connect(&session, &SessionState::changed, &session, [&] {
            if (!session.blocked() && !transitionStarted) pendingMode.clear();
        });
        QObject::connect(&session, &SessionState::exitReady, &app, [&] {
            if (pendingMode.isEmpty()) { app.exit(); return; }
            if (transitionStarted) return;
            transitionStarted = true;
            platform.switchMode(pendingMode);
        });
        QObject::connect(&platform, &ArmadaPlatformService::transitionFinished, &session, [&](bool success) {
            if (success) { app.exit(); return; }
            transitionStarted = false; pendingMode.clear(); session.cancelPendingExit();
            shell.showNotice("The mode could not be changed. Your journal is saved; TrainerOS remains open.");
        });
        app.installEventFilter(&input);
        if (!smoke) {
            input.setEnabled(app.applicationState() == Qt::ApplicationActive);
            QObject::connect(&app, &QGuiApplication::applicationStateChanged, &input,
                             [&input, &adventureLaunch](Qt::ApplicationState state) {
                input.setEnabled(state == Qt::ApplicationActive && (!adventureLaunch.active() || adventureLaunch.preparing()));
            });
        }
        SpriteArt sprites(parser.isSet("sprite-dir") ? parser.value("sprite-dir") : smoke || parser.isSet("ephemeral")
            ? QString() : QDir(reportBase).filePath("artwork/sprites"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("sprite-detail", new SpriteImages(sprites));
        shell.pokedex()->configureSprites(&sprites);
        shell.party()->configureArtwork(&classicArt, &sprites);
        engine.addImageProvider("exit-frame", new ExitFrameImages(exitPresentation));
        engine.addImageProvider("exit-media", new SavedExitImages(store.get()));
        int qmlWarnings = 0;
        QStringList diagnostics;
        QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                         [&](const QList<QQmlError>& errors) {
            qmlWarnings += errors.size();
            for (const auto& error : errors) diagnostics.append(error.toString());
        });
        engine.rootContext()->setContextProperty("shellController", &shell);
        engine.rootContext()->setContextProperty("controllerInput", &input);
        engine.rootContext()->setContextProperty("sessionState", &session);
        engine.rootContext()->setContextProperty("adventureLaunch", &adventureLaunch);
        engine.rootContext()->setContextProperty("adventureExitPresentation", &exitPresentation);
        engine.rootContext()->setContextProperty("powerStatus", &powerStatus);
        engine.load(QUrl("qrc:/TrainerOS/Main.qml"));
        if (engine.rootObjects().isEmpty()) result = 2;
        else {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            if (!window) return 2;
            auto* pointerVisibility = new PointerVisibility(*window, platform.dedicatedSession());
            if (auto* exitWindow = window->findChild<QQuickWindow*>("adventure-exit-window"))
                new PointerVisibility(*exitWindow, platform.dedicatedSession());
            QObject::connect(&input, &ControllerInput::observedAction, pointerVisibility,
                             [pointerVisibility](Action, bool fromController) {
                if (fromController) pointerVisibility->hide();
            });
#ifdef Q_OS_LINUX
            bool readyDescriptorValid = false;
            const int readyDescriptor = qEnvironmentVariableIntValue("TRAINEROS_READY_FD", &readyDescriptorValid);
            if (!sessionReadyNotified && platform.dedicatedSession() && readyDescriptorValid && readyDescriptor >= 3) {
                auto notified = std::make_shared<bool>(false);
                QObject::connect(window, &QQuickWindow::frameSwapped, window, [readyDescriptor, notified, &sessionReadyNotified] {
                    if (*notified) return;
                    *notified = true; sessionReadyNotified = true;
                    QFile ready;
                    if (ready.open(readyDescriptor, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
                        ready.write("R", 1); ready.flush();
                    }
                }, Qt::QueuedConnection);
            }
#endif
            if(PerformanceTrace::enabled()) {
                auto beat=std::make_shared<QElapsedTimer>();beat->start();
                auto* timer=new QTimer(window);timer->setInterval(100);timer->setTimerType(Qt::PreciseTimer);
                QObject::connect(timer,&QTimer::timeout,window,[beat]{
                    PerformanceTrace::sample("eventLoop.delay",qMax(0.0,beat->nsecsElapsed()/1e6-100));beat->restart();
                });timer->start();
                auto frame=std::make_shared<QElapsedTimer>();
                QObject::connect(window,&QQuickWindow::frameSwapped,window,[frame]{
                    if(frame->isValid())PerformanceTrace::sample("frame.interval",frame->nsecsElapsed()/1e6);
                    frame->restart();
                },Qt::DirectConnection);
                auto* report=new QTimer(window);report->setInterval(1000);
                QObject::connect(report,&QTimer::timeout,window,[&shell]{PerformanceTrace::flush(shell.page());});report->start();
            }
            deviceReports.setWindow(window);
            session.start();
            if (!parser.isSet("windowed") && !smoke) window->showFullScreen();
            if (personalLibrary && !smoke) {
                auto returnFullscreen = std::make_shared<bool>(false);
                auto returnedAdventure=std::make_shared<QString>();
                const auto requestAdventure = [&, window, returnFullscreen, returnedAdventure](const ProcessCommand& command, const QString& id) -> AdventureResult {
                    if (pendingLinkSave(QDir(stateDirectory).filePath("backups")))
                        return {false, "Reconnect with your friend to finish the exchange before playing."};
                    if (shell.party()->activities()->link()->active())
                        return {false, "Leave Together before starting a game."};
                    if (session.blocked() || adventureLaunch.active())
                        return {false, "An Adventure is already opening. Try again after returning."};
                    if (saveBackups && saveBackups->busy())
                        return {false, "Wait for the save operation to finish before playing."};
                    *returnFullscreen = window->visibility() == QWindow::FullScreen;
                    *returnedAdventure=id;realAchievements->refreshAdventure(id);
                    if (!adventureLaunch.launch(command, shell.navigationState(), id))
                        return {false, "This Adventure could not start. Try again."};
                    return {true, {}, true};
                };
                retroarch.requestLaunch = [&, requestAdventure](const ProcessCommand& command, const QString& id) {
                    auto launch = command;
                    useRetroArchAchievementAccount(launch, realAchievements->launchAccount());
                    return requestAdventure(launch, id);
                };
                melonDs.requestLaunch = requestAdventure;
                dolphin.requestLaunch = requestAdventure;
                ppsspp.requestLaunch = requestAdventure;
                armsx2.requestLaunch = requestAdventure;
                QObject::connect(&adventureLaunch, &AdventureLaunchController::changed, &session, [&] {
                    session.setAdventureActive(adventureLaunch.active());
                    input.setEnabled(app.applicationState() == Qt::ApplicationActive && (!adventureLaunch.active() || adventureLaunch.preparing()));
                });
                QObject::connect(&adventureLaunch, &AdventureLaunchController::checkpointRequested, store.get(),
                                 [&](quint64 token, const QJsonObject& state) {
                    store->saveNavigation(state, &adventureLaunch, [&, token](const QString& error) {
                        adventureLaunch.checkpointCompleted(token, error);
                    });
                });
                QObject::connect(&adventureLaunch, &AdventureLaunchController::suspendRequested, window, [window] { window->hide(); });
                QObject::connect(&adventureLaunch, &AdventureLaunchController::restoreRequested, window,
                                 [&, window, returnFullscreen, returnedAdventure](const QJsonObject& state) {
                    shell.restoreNavigation(state);
                    if (!adventureLaunch.error().isEmpty()) shell.showNotice(adventureLaunch.error());
                    else if (playHistory && !playHistory->error().isEmpty()) shell.showNotice(playHistory->error());
                    if (*returnFullscreen) window->showFullScreen(); else window->show();
                    window->requestActivate();
                    input.setEnabled(app.applicationState() == Qt::ApplicationActive);
                    realAchievements->refreshAdventure(*returnedAdventure,true);
                    if(gameProgress&&adventureLaunch.error().isEmpty())if(const auto record=activeLibrary.registration(*returnedAdventure)) {
                        const auto owner=store->ownerId();
                        gameProgress->inspectCompletion(*record,&shell,[&,owner](AdventureCompletion result){
                            if(!result.completed||adventureLaunch.active()||store->ownerId()!=owner)return;
                            QSettings settings;const auto key="reviews/invited/"+owner+"/"+result.identity;
                            if(settings.value(key,false).toBool())return;
                            settings.setValue(key,true);shell.showAchievements("Adventure complete",{"You can share a review in Game Properties"});
                        });
                    }
                });
                QObject::connect(playHistory.get(), &PlayHistoryController::writeFailed, &shell, [&](const QString& error) {
                    if (!adventureLaunch.active()) shell.showNotice(error);
                });
            }
            if (smoke) {
                const QString screenshotDir = parser.value("screenshot-dir");
                if (!screenshotDir.isEmpty() && !QDir().mkpath(screenshotDir)) return 2;
#ifdef TRAINEROS_UI_TESTS
                if (exitSmoke) {
                    startExitSmoke(window, shell, adventureLaunch, exitPresentation, input, joystick,
                                   screenshotDir, smokeCompleted, qmlWarnings, diagnostics);
                } else if(persistencePhase=="library-center") {
                    startCenterSmoke(window,shell,session,*store,input,joystick,stateDirectory,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if (diagnosticsSmoke) {
                    startDiagnosticsSmoke(window, shell, session, input, deviceReports, joystick, screenshotDir,
                                          smokeCompleted, qmlWarnings, diagnostics);
                } else if (persistencePhase.startsWith("library-home")) {
                    startHomeSmoke(window, shell, session, *store, input, probeAdapter, joystick, persistencePhase.endsWith("reopen"),
                                   screenshotDir, smokeCompleted, qmlWarnings, diagnostics);
                } else if (persistencePhase == "library-launch") {
                    startLaunchSmoke(window, shell, session, *store, input, probeAdapter, joystick, screenshotDir,
                                     smokeCompleted, qmlWarnings, diagnostics);
                } else if (persistencePhase.startsWith("library-") || persistencePhase == "collection") {
                    startLibrarySmoke(window, shell, session, *store, input, joystick, persistencePhase,
                                      QDir(parser.value("data-dir")).filePath("content"), screenshotDir, smokeCompleted, qmlWarnings, diagnostics);
                } else if (!persistencePhase.isEmpty()) {
                    startPersistenceSmoke(window, shell, session, input, joystick, persistencePhase, screenshotDir,
                                          smokeCompleted, qmlWarnings, diagnostics);
                } else if (hallSmoke) {
                    startHallSmoke(window, shell, input, shellArchive, shellAchievements, joystick, screenshotDir,
                                   smokeCompleted, qmlWarnings, diagnostics);
                } else if (artSmoke) {
                    startArtworkSmoke(window, shell, input, joystick, screenshotDir, smokeCompleted, qmlWarnings, diagnostics);
                } else if (pokedexSmoke) {
                    startPokedexSmoke(window, shell, input, dex, joystick, screenshotDir,
                                      smokeCompleted, qmlWarnings, diagnostics);
                } else if (worldsSmoke) {
                    startWorldsSmoke(window, shell, input, adapter, joystick, screenshotDir,
                                     smokeCompleted, qmlWarnings, diagnostics);
                } else
#endif
                {
                auto step = std::make_shared<int>(0);
                auto failed = std::make_shared<bool>(false);
                auto savedId = std::make_shared<QString>();
                auto drawerWait = std::make_shared<int>(0);
                auto timer = new QTimer(&app);
                timer->setInterval(400);
                QObject::connect(timer, &QTimer::timeout, &app, [&, window, step, failed, savedId, drawerWait, timer, screenshotDir] {
                    if (powerStatus.busy()) return;
                    const auto press = [&](SDL_GameControllerButton button) {
                        SDL_JoystickSetVirtualButton(joystick, button, 1); input.poll();
                        SDL_JoystickSetVirtualButton(joystick, button, 0); input.poll();
                    };
                    const auto check = [&](bool condition, const char* reason) {
                        if (!condition) {
                            *failed = true;
                            diagnostics.append(QString("Step %1: %2").arg(*step - 1).arg(reason));
                            qCritical() << "Smoke test:" << reason;
                        }
                    };
                    const auto focusIs = [&](const QString& name) {
                        return window->activeFocusItem() && window->activeFocusItem()->objectName() == name;
                    };
                    const auto capture = [&](const QString& name) {
                        const QImage frame = window->grabWindow();
                        check(!frame.isNull(), "rendered frame is empty");
                        if (!screenshotDir.isEmpty()) check(frame.save(screenshotDir + "/" + name + ".png"), "cannot save screenshot");
                    };
                    const auto taps = [&](SDL_GameControllerButton button, int count = 1) {
                        for (int i = 0; i < count; ++i) press(button);
                    };
                    constexpr auto up = SDL_CONTROLLER_BUTTON_DPAD_UP;
                    constexpr auto down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
                    constexpr auto left = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
                    constexpr auto right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
                    // Raw SDL east/south positions model the printed Switch A/B.
                    constexpr auto a = SDL_CONTROLLER_BUTTON_B;
                    constexpr auto b = SDL_CONTROLLER_BUTTON_A;
                    if (*step == 1 || *step == 28) {
                        // On ARM/software rendering a timer tick can arrive
                        // before the drawer's final animation frame is drawn.
                        auto* card = window->activeFocusItem();
                        bool exposed = card && card->isVisible() && card->objectName().startsWith("resume-");
                        if (exposed) {
                            const QRectF bounds(-4, -4, card->width() + 8, card->height() + 8);
                            for (auto* ancestor = card->parentItem(); ancestor; ancestor = ancestor->parentItem()) {
                                const auto mapped = ancestor->mapRectFromItem(card, bounds);
                                if (ancestor->clip() && !QRectF(-1, -1, ancestor->width() + 2, ancestor->height() + 2).contains(mapped)) {
                                    exposed = false;
                                    if (*drawerWait == 8) diagnostics.append(QString("Continue bounds %1,%2 %3x%4 exceed %5x%6")
                                        .arg(mapped.x()).arg(mapped.y()).arg(mapped.width()).arg(mapped.height()).arg(ancestor->width()).arg(ancestor->height()));
                                }
                            }
                        }
                        if (!exposed && ++*drawerWait <= 8) return;
                        check(exposed, "Continue card and focus outline must fit after animation");
                        *drawerWait = 0;
                    }
                    switch ((*step)++) {
                    case 0:
                        check(input.connected(), "virtual controller not connected");
                        check(shell.page() == 0 && focusIs("home-launch"), "initial Home focus");
                        capture("home"); press(SDL_CONTROLLER_BUTTON_X); break;
                    case 1:
                        check(shell.drawerOpen() && focusIs("resume-0"), "Y opens drawer and focuses first card");
                        capture("continue"); press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT); press(SDL_CONTROLLER_BUTTON_B); break;
                    case 2:
                        check(shell.notice().isEmpty() && !shell.drawerOpen() && focusIs("home-launch"), "Selection returns to Home without launch");
                        check(shell.home()["adventureId"] == "crystal-demo", "Selected Adventure rebuilds Home");
                        capture("home-selected");
                        press(SDL_CONTROLLER_BUTTON_X); break;
                    case 3:
                        check(shell.focusIndex() == 1 && focusIs("resume-1"), "Back restores resume card");
                        press(SDL_CONTROLLER_BUTTON_START); break;
                    case 4:
                        check(shell.menuOpen() && focusIs("menu-0"), "Start traps system-menu focus");
                        capture("system"); press(SDL_CONTROLLER_BUTTON_A); break;
                    case 5:
                        check(shell.drawerOpen() && focusIs("resume-1"), "Back restores drawer from system menu");
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
                    case 6:
                        check(shell.page() == 1 && !shell.drawerOpen() && focusIs("world-0"), "R1 changes page and closes transient layers");
                        press(SDL_CONTROLLER_BUTTON_DPAD_DOWN); press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT); break;
                    case 7:
                        check(shell.focusIndex() == 4 && focusIs("world-4"), "spatial World focus");
                        capture("worlds"); press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
                    case 8:
                        check(shell.page() == 2 && focusIs("dex-entry-bulbasaur"), "Pokedex list focus");
                        capture("pokedex"); press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); break;
                    case 9:
                        check(shell.page() == 1 && focusIs("world-4"), "per-page focus restored");
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
                    case 10:
                        check(shell.page() == 3 && focusIs("trainer-open"), "empty Trainer focus");
                        capture("trainer-empty"); press(a); break;
                    case 11:
                        check(shell.trainer()->editing() && focusIs("trainer-field-0"), "create Trainer starts at Name");
                        capture("trainer-create");
                        taps(down, 3); press(a);
                        check(!shell.trainer()->error().isEmpty() && focusIs("trainer-field-0"), "empty name restores editable field");
                        capture("trainer-validation"); press(a); break;
                    case 12:
                        check(shell.keyboard()->isOpen() && focusIs("key-A"), "keyboard starts at A");
                        capture("keyboard-empty");
                        // Enter ERI 2 using physical-style button events only.
                        taps(right, 4); press(a); // E
                        press(down); taps(right, 3); press(a); // R
                        press(up); press(right); press(a); // I
                        taps(down, 3); press(left); press(a); // Space
                        taps(right, 2); taps(up, 3); press(a); // 2
                        break;
                    case 13:
                        check(shell.keyboard()->text() == "ERI 2" && focusIs("key-2"), "mixed controller text entry");
                        if (auto* key = window->activeFocusItem()) {
                            for (auto* item : key->findChildren<QQuickItem*>()) {
                                if (item->property("text").toString() != "2") continue;
                                const auto font = item->property("font").value<QFont>();
                                const QFontInfo info(font);
                                diagnostics.append(QString("Numeric key font: %1; resolved %2 %3; %4px")
                                    .arg(font.toString(), info.family(), info.styleName()).arg(info.pixelSize()));
                            }
                        }
                        check(shell.trainer()->draftName().isEmpty(), "keyboard buffer must not modify the form before Apply");
                        capture("keyboard-name"); press(SDL_CONTROLLER_BUTTON_START); break;
                    case 14:
                        check(shell.menuOpen() && focusIs("menu-0"), "Start traps focus above keyboard");
                        press(SDL_CONTROLLER_BUTTON_X);
                        check(shell.keyboard()->text() == "ERI 2", "Y must not change text under menu");
                        taps(down, 2); press(a); break; // Unavailable Desktop mode overlays the keyboard; retired actions are skipped.
                    case 15:
                        check(focusIs("notice-close"), "notice traps focus above menu and keyboard");
                        press(b); break;
                    case 16:
                        check(focusIs("menu-4"), "Back restores system-menu opener");
                        press(b);
                        check(focusIs("key-2") && shell.keyboard()->text() == "ERI 2", "Back restores exact key and draft");
                        taps(down, 3); press(left); press(a); // Apply
                        break;
                    case 17:
                        check(!shell.keyboard()->isOpen() && focusIs("trainer-field-0"), "Apply restores Name control");
                        check(shell.trainer()->draftName() == "ERI 2" && !profiles.load(), "Apply changes draft only");
                        press(down); press(a); // leaf
                        press(down); press(a); // Open favorite picker.
                        press(right); taps(down, 2); press(a); // Treecko in the reference list.
                        capture("trainer-edit");
                        press(down); press(a); // Save
                        break;
                    case 18:
                        check(shell.trainer()->exists() && focusIs("trainer-open"), "Save creates Trainer and restores opener");
                        check(shell.trainer()->profile()["name"] == "ERI 2", "saved name");
                        check(shell.trainer()->profile()["emblem"] == "leaf", "saved emblem");
                        check(shell.trainer()->profile()["favorite"] == "Treecko", "saved favorite");
                        *savedId = shell.trainer()->profile()["id"].toString();
                        check(!savedId->isEmpty() && shell.home()["trainer"] == "ERI 2", "stable identity and Home projection");
                        capture("trainer-profile"); press(a); break;
                    case 19:
                        press(a); press(a); // Edit Name, append A.
                        check(shell.keyboard()->text() == "ERI 2A", "editing starts from saved name");
                        press(b);
                        check(shell.trainer()->draftName() == "ERI 2" && focusIs("trainer-field-0"), "B discards only keyboard buffer");
                        press(down); press(a); // Unsaved emblem.
                        press(b);
                        check(shell.trainer()->profile()["emblem"] == "leaf", "B discards form changes");
                        press(a); press(a); // Reopen name for numeric tour.
                        break;
                    case 20:
                        // Reach and enter all ten numeric keys through the separate block.
                        taps(right, 10); press(a); press(right); press(a); press(right); press(a);
                        press(down); press(a); press(left); press(a); press(left); press(a);
                        press(down); press(a); press(right); press(a); press(right); press(a);
                        press(down); press(a);
                        check(shell.keyboard()->text() == "ERI 21236547890" && focusIs("key-0"), "all numeric keys reachable");
                        press(left); press(up); press(left); press(a); // Delete
                        check(shell.keyboard()->text() == "ERI 2123654789", "Delete removes last character");
                        press(right); press(a); // Clear
                        check(shell.keyboard()->text().isEmpty(), "Clear empties buffer");
                        press(left); press(down); press(left); press(a); // Space
                        check(shell.keyboard()->text() == " ", "Space reachable from numeric block");
                        press(b); press(b); // Discard buffer and form.
                        check(shell.trainer()->profile()["name"] == "ERI 2", "cancel numeric tour preserves profile");
                        press(a); press(a); press(a);
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); // Global page switch cancels both drafts.
                        break;
                    case 21:
                        check(shell.page() == 4 && focusIs("social-empty"), "R1 escapes keyboard to Social");
                        check(!shell.keyboard()->isOpen() && !shell.trainer()->editing(), "page switch closes both drafts");
                        check(shell.trainer()->profile()["id"] == *savedId && shell.trainer()->profile()["name"] == "ERI 2", "switch preserves saved identity and name");
                        press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); break;
                    case 22:
                        check(focusIs("trainer-open"), "L1 restores Trainer opener");
                        // Save another edit and ensure identity survives.
                        press(a); press(down); press(a); taps(down, 2); press(a);
                        check(shell.trainer()->profile()["id"] == *savedId && shell.trainer()->profile()["emblem"] == "spark", "editing preserves identity");
                        window->resize(1920, 1080); break;
                    case 23:
                        capture("trainer-profile-1080p"); press(a); press(a); break;
                    case 24:
                        capture("keyboard-1080p");
                        window->resize(1024, 768); break;
                    case 25:
                        capture("keyboard-letterbox"); press(b); press(b);
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
                    case 26:
                        check(shell.page() == 4 && focusIs("social-empty"), "Unlinked Social has visible shell focus");
                        check(shell.socialFace()=="chats" && !shell.chooseAdventureAvailable(), "Social has no game selector");
                        press(b);
                        check(shell.page() == 4, "Back must not leave primary page");
                        capture("social-friends-letterbox");
                        SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);input.poll();
                        SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);input.poll();
                        check(shell.socialFace()=="groups", "Physical R2 cycles Social peers");
                        capture("landscape-letterbox");
                        window->resize(1920, 1080); break;
                    case 27:
                        capture("social-chats-1080p");
                        taps(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 4);
                        adapter.setSource("crystal-1", ""); shell.refreshLibrary();
                        press(SDL_CONTROLLER_BUTTON_X); break;
                    case 28:
                        check(focusIs("resume-1"), "Refresh keeps Continue focus by identity");
                        check(shell.resumePoints()[1].toMap()["previewLabel"] == "Saved moment unavailable", "Missing source is labelled");
                        capture("continue-missing-1080p"); press(a); break;
                    case 29:
                        check(focusIs("home-launch") && shell.notice().isEmpty(), "Unavailable selection still chooses Home without launching");
                        check(shell.home()["milestone"] == "Saved moment unavailable", "Home explains unavailable selection");
                        capture("home-missing-1080p"); press(a); break;
                    case 30:
                        check(!shell.notice().isEmpty() && !shell.notice().contains("Demo launch"), "Unavailable resume cannot silently become a launch");
                        capture("resume-fallback-1080p"); press(b); press(left); press(a); break;
                    case 31:
                        check(shell.notice().contains("Demo launch ready"), "Explicit Home action opens normal Adventure after fallback");
                        press(b); check(focusIs("home-launch"), "Back restores fixed Home action");
                        smokeBattery->store(0); powerStatus.refresh(); break;
                    case 32:
                        check(powerStatus.available() && powerStatus.percent() == 0 && focusIs("home-launch"), "Zero battery is valid and never steals focus");
                        if (const auto label = window->findChild<QObject*>("power-percent")) check(label->property("text").toString() == "0%", "Zero charge is rendered");
                        else check(false, "Battery label exists");
                        capture("battery-zero"); smokeBattery->store(15); powerStatus.refresh(); break;
                    case 33:
                        check(powerStatus.percent() == 15 && !powerStatus.charging(), "Low charge remains a reported percentage");
                        capture("battery-low"); smokeBattery->store(1100); powerStatus.refresh(); break;
                    case 34:
                        check(powerStatus.percent() == 100 && powerStatus.charging(), "Charging/full-width gauge");
                        if (const auto bolt = window->findChild<QQuickItem*>("power-charging")) check(bolt->isVisible(), "Charging bolt is visible");
                        else check(false, "Charging indicator exists");
                        capture("battery-charging"); smokeBattery->store(-1); powerStatus.refresh(); break;
                    case 35:
                        check(!powerStatus.available() && !powerStatus.charging() && focusIs("home-launch"), "Unavailable charge clears old value without changing focus");
                        if (const auto bolt = window->findChild<QQuickItem*>("power-charging")) check(!bolt->isVisible(), "Unknown charge clears the charging bolt");
                        else check(false, "Charging indicator exists");
                        if (const auto warning = window->findChild<QQuickItem*>("controller-warning")) check(!warning->isVisible(), "Healthy controller does not display technical status text");
                        else check(false, "Controller warning exists");
                        capture("battery-unavailable"); break;
                    case 36:
                        shell.keyboard()->begin("Password preview", "", 64, true);
                        press(a); press(SDL_CONTROLLER_BUTTON_Y); press(a);
                        press(SDL_CONTROLLER_BUTTON_X); press(a);
                        check(shell.keyboard()->text() == "aA!", "Controller case and symbol entry");
                        break;
                    case 37:
                        check(focusIs("key-A"), "Changing key layout preserves visible focus");
                        if (auto* field = window->findChild<QObject*>("keyboard-text"))
                            check(field->property("text").toString() == QString(3, QChar(0x2022)) + QChar(0x2502), "Rendered password contains only bullets and cursor");
                        else check(false, "Password display exists");
                        capture("keyboard-secret-symbols");
                        press(SDL_CONTROLLER_BUTTON_X); break;
                    case 38:
                        capture("keyboard-symbols-second-page");
                        taps(right, 6); press(a);
                        check(shell.keyboard()->text() == "aA!1", "Numeric block remains reachable past hidden symbol slots");
                        press(b);
                        check(!shell.keyboard()->isOpen() && shell.keyboard()->text().isEmpty(), "Back discards secret input");
                        break;
                    default:
                        check(qmlWarnings == 0, "QML warnings were emitted");
                        timer->stop();
                        smokeCompleted = true;
                        if (!screenshotDir.isEmpty()) {
                            QFile report(screenshotDir + "/verification.txt");
                            if (report.open(QIODevice::WriteOnly | QIODevice::Truncate))
                                report.write(((*failed ? QString("FAILED\n") : QString("PASSED\n")) + diagnostics.join('\n')).toUtf8());
                        }
                        qInfo() << (*failed ? "QML smoke test FAILED" : "QML smoke test passed: SDL input, focus, overlays and rendering.");
                        app.exit(*failed ? 1 : 0);
                    }
                });
                timer->start();
                }
            }
            result = app.exec();
            if (smoke && !smokeCompleted) result = 2; // Early window/app exit is not a passing smoke test.
        }
    } while (restartTrainer);
    if (joystick) SDL_JoystickClose(joystick);
    if (virtualIndex >= 0) SDL_JoystickDetachVirtual(virtualIndex);
    if (smoke) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    return result;
}
