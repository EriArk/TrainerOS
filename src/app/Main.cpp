#include "platform/device/VolumeKeys.h"
#include "core/input/ControllerInput.h"
#include "core/input/PointerVisibility.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/AdapterRouter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include "integrations/adventure/standalone/MelonDsSave.h"
#include "integrations/progress/GameProgressService.h"
#include <QJsonDocument>
#include "integrations/achievements/RetroAchievementsProvider.h"
#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "core/navigation/AdventureLaunchController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "platform/input/AdventureOverlayService.h"
#include "features/home/PlayHistoryController.h"
#include "features/home/ExitImage.h"
#include "features/pokedex/ClassicArt.h"
#include "core/repository/CollectionRepository.h"
#include "core/repository/OfflinePokedex.h"
#include "integrations/adventure/retroarch/RetroArchSave.h"
#include "platform/storage/SaveBackupStorage.h"
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
        const auto frame = frameExitImage(store_ ? store_->exitImage(id) : QImage{});
        if (size) *size = frame.size();
        return frame;
    }
private:
    LocalStateStore* store_;
};
class ExitFrameImages final : public QQuickImageProvider {
public:
    explicit ExitFrameImages(AdventureExitController& controller) : QQuickImageProvider(Image), controller_(controller) {}
    QImage requestImage(const QString&, QSize* size, const QSize&) override {
        const auto frame = controller_.capturedFrame();
        if (size) *size = frame.size();
        return frame;
    }
private:
    AdventureExitController& controller_;
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
    parser.addOption({"ephemeral", "Use isolated in-memory sample data; do not open a persistent store."});
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
    int result = 0;
    bool smokeCompleted = false;
    {
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
        const bool personalLibrary = store && (!smoke || persistencePhase.startsWith("library-") || persistencePhase == "collection");
        CollectionRepository collection(personalLibrary ? static_cast<LibraryRepository&>(*store) : repository);
        LibraryRepository& baseLibrary = personalLibrary ? (!smoke || persistencePhase == "collection" ? static_cast<LibraryRepository&>(collection) : *store) : repository;
        // State thumbnails are migration evidence, not normal launch targets.
        LibraryRepository& activeLibrary = baseLibrary;
        std::unique_ptr<RetroAchievementsProvider> realAchievements;
        if (personalLibrary && !smoke) realAchievements = std::make_unique<RetroAchievementsProvider>(activeLibrary, stateDirectory);
        AdventureAdapter* selectedAdapter = personalLibrary ? static_cast<AdventureAdapter*>(&unconfiguredAdapter) : &adapter;
        const auto retroarchInstallation = personalLibrary && !smoke
            ? RetroArchInstallation::load(QDir(stateDirectory).filePath("integrations/retroarch.json")) : RetroArchInstallation{};
        RetroArchAdapter retroarch(activeLibrary, retroarchInstallation);
        const auto standaloneInstallation = [&](const QString& id) {
            return personalLibrary && !smoke
                ? StandaloneInstallation::load(QDir(stateDirectory).filePath("integrations/" + id + ".json"), id) : StandaloneInstallation{};
        };
        const auto melonDsInstallation = standaloneInstallation("melonds");
        StandaloneAdapter melonDs("melonds", activeLibrary, melonDsInstallation);
        StandaloneAdapter dolphin("dolphin", activeLibrary, standaloneInstallation("dolphin"));
        AdapterRouter adapters({&retroarch, &melonDs, &dolphin});
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
        if (personalLibrary && !smoke) {
            shell.libraryManager()->prepareInstallation = [&adapters](AdventureRegistration& record) { adapters.prepareInstallation(record); };
            shell.libraryManager()->setInitialFolder(QDir::home().filePath("Emulation/roms"));
        }
        if (store) QObject::connect(store.get(), &LocalStateStore::libraryChanged, &shell, &ShellController::refreshLibrary);
        if (realAchievements) QObject::connect(store.get(), &LocalStateStore::opened, realAchievements.get(), [&](bool success) {
            if (success) realAchievements->refreshAll();
        });
        SessionState session(shell, store.get());
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
        VolumeKeys volumeKeys(!smoke && !parser.isSet("ephemeral") && platform.dedicatedSession());
        QObject::connect(&volumeKeys, &VolumeKeys::adjustmentRequested, &deviceService,
            [&deviceService](int delta) { deviceService.hardwareVolume(delta); });

        std::unique_ptr<LocalSaveBackupService> saveBackups;
        if (personalLibrary && !smoke) {
            saveBackups=std::make_unique<LocalSaveBackupService>(QDir(stateDirectory).filePath("backups"),
                [retroarchInstallation, melonDsInstallation](const AdventureRegistration& record){return record.adventure.adapterId == "melonds"
                    ? resolveMelonDsSave(record, melonDsInstallation) : resolveRetroArchSave(record,retroarchInstallation);},
                [retroarchInstallation, melonDsInstallation](const AdventureRegistration& record){return supportsMelonDsSave(record, melonDsInstallation)
                    || (retroarchInstallation.saveBackups && record.adventure.adapterId=="retroarch" && record.integrationConfig["core"].toString()=="mgba");});
        }
#ifdef TRAINEROS_UI_TESTS
        if(persistencePhase=="library-center") {
            saveBackups=std::make_unique<LocalSaveBackupService>(QDir(stateDirectory).filePath("backups"),
                [](const AdventureRegistration& record){
                    QFile content(record.contentPath); if(!content.open(QIODevice::ReadOnly))return SaveTarget{};
                    const auto revision=QString::fromLatin1(QCryptographicHash::hash(content.readAll(),QCryptographicHash::Sha256).toHex());
                    return SaveTarget{record.adventure.id,record.adventure.title,record.contentPath+".srm",revision,record.contentPath,{},true};
                },[](const AdventureRegistration& record){return record.adventure.adapterId=="backup-fixture";});
        }
#endif
        if(saveBackups) {
            shell.center()->configure(saveBackups.get());
            QObject::connect(saveBackups.get(),&SaveBackupService::operationFailed,&session,&SessionState::cancelPendingExit);
        }
        const auto updateServiceActivity = [&] {
            session.setServiceActive(deviceService.busy() || (saveBackups && saveBackups->busy()));
        };
        QObject::connect(&deviceService, &DeviceService::changed, &session, updateServiceActivity);
        if (saveBackups) QObject::connect(saveBackups.get(), &SaveBackupService::busyChanged, &session, updateServiceActivity);
        ProcessService adventureProcess;
        AdventureLaunchController adventureLaunch(adventureProcess);
        AdventureExitPresentation exitPresentation(adventureLaunch.exitController());
        std::unique_ptr<AdventureOverlayService> adventureOverlay;
#ifdef Q_OS_LINUX
        if (personalLibrary && !smoke && platform.dedicatedSession()) {
            QFile configuration(QDir(stateDirectory).filePath("integrations/overlay.json"));
            if (configuration.open(QIODevice::ReadOnly) && configuration.size() <= 8192) {
                const auto object = QJsonDocument::fromJson(configuration.readAll()).object();
                if (object["version"].toInt() == 1 && object["enabled"].toBool())
                    adventureOverlay = std::make_unique<AdventureOverlayService>(adventureProcess, adventureLaunch,
                        exitPresentation, object["helper"].toString());
            }
        }
#endif
        std::unique_ptr<GameProgressService> gameProgress;
        QByteArray progressSelection;
        bool progressHomeVisible = false;
        if (personalLibrary && !smoke) {
            gameProgress = std::make_unique<GameProgressService>([retroarchInstallation](const AdventureRegistration& record) {
                return resolveRetroArchSave(record, retroarchInstallation);
            });
            shell.configureProgress(gameProgress.get());
            const auto refreshProgress = [&, provider = gameProgress.get()](bool force) {
                if (adventureLaunch.active() || (saveBackups && saveBackups->busy())) return;
                const auto id = shell.home()["adventureId"].toString();
                const auto record = activeLibrary.registration(id);
                if (!record) {
                    if (!progressSelection.isEmpty()) { progressSelection.clear(); provider->invalidate(); }
                    return;
                }
                const auto key = QJsonDocument(QJsonObject{{"id", id}, {"revision", record->revision},
                    {"path", record->contentPath}, {"config", record->integrationConfig}}).toJson(QJsonDocument::Compact);
                if (!force && key == progressSelection) return;
                progressSelection = key;
                provider->refresh(*record);
            };
            QObject::connect(&shell, &ShellController::changed, gameProgress.get(), [&, refreshProgress] {
                const bool homeVisible = shell.page() == 0;
                const bool enteredHome = homeVisible && !progressHomeVisible;
                progressHomeVisible = homeVisible;
                refreshProgress(enteredHome);
            });
            QObject::connect(store.get(), &LocalStateStore::opened, gameProgress.get(), [refreshProgress](bool success) {
                if (success) refreshProgress(true);
            });
            QObject::connect(store.get(), &LocalStateStore::libraryChanged, gameProgress.get(), [refreshProgress] { refreshProgress(true); });
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
            return ExitMediaSource{profile->id, "pokemon", *record};
        });
        ControllerInput input(nullptr, preferred);
        const auto reportBase = parser.isSet("data-dir") ? QDir(parser.value("data-dir")).absolutePath()
            : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        ClassicArt classicArt(parser.isSet("art-dir") ? parser.value("art-dir") : smoke || parser.isSet("ephemeral")
            ? QString() : QDir(reportBase).filePath("artwork/bootstrap"));
        shell.pokedex()->configureArtwork(&classicArt);
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
        QQmlApplicationEngine engine;
        engine.addImageProvider("exit-frame", new ExitFrameImages(adventureLaunch.exitController()));
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
            if (platform.dedicatedSession() && readyDescriptorValid && readyDescriptor >= 3) {
                auto notified = std::make_shared<bool>(false);
                QObject::connect(window, &QQuickWindow::frameSwapped, &app, [readyDescriptor, notified] {
                    if (*notified) return;
                    *notified = true;
                    QFile ready;
                    if (ready.open(readyDescriptor, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
                        ready.write("R", 1); ready.flush();
                    }
                }, Qt::QueuedConnection);
            }
#endif
            deviceReports.setWindow(window);
            session.start();
            if (!parser.isSet("windowed") && !smoke) window->showFullScreen();
            if (personalLibrary && !smoke) {
                auto returnFullscreen = std::make_shared<bool>(false);
                const auto requestAdventure = [&, window, returnFullscreen](const ProcessCommand& command, const QString& id) {
                    if (session.blocked() || adventureLaunch.active() || (saveBackups && saveBackups->busy())) return false;
                    *returnFullscreen = window->visibility() == QWindow::FullScreen;
                    return adventureLaunch.launch(command, shell.navigationState(), id);
                };
                retroarch.requestLaunch = requestAdventure;
                melonDs.requestLaunch = requestAdventure;
                dolphin.requestLaunch = requestAdventure;
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
                                 [&, window, returnFullscreen](const QJsonObject& state) {
                    shell.restoreNavigation(state);
                    if (!adventureLaunch.error().isEmpty()) shell.showNotice(adventureLaunch.error());
                    else if (playHistory && !playHistory->error().isEmpty()) shell.showNotice(playHistory->error());
                    if (*returnFullscreen) window->showFullScreen(); else window->show();
                    window->requestActivate();
                    input.setEnabled(app.applicationState() == Qt::ApplicationActive);
                    realAchievements->refreshAll();
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
                        capture("home"); press(SDL_CONTROLLER_BUTTON_Y); break;
                    case 1:
                        check(shell.drawerOpen() && focusIs("resume-0"), "Y opens drawer and focuses first card");
                        capture("continue"); press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT); press(SDL_CONTROLLER_BUTTON_B); break;
                    case 2:
                        check(shell.notice().isEmpty() && !shell.drawerOpen() && focusIs("home-launch"), "Selection returns to Home without launch");
                        check(shell.home()["adventureId"] == "crystal-demo", "Selected Adventure rebuilds Home");
                        capture("home-selected");
                        press(SDL_CONTROLLER_BUTTON_Y); break;
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
                        press(SDL_CONTROLLER_BUTTON_Y);
                        check(shell.keyboard()->text() == "ERI 2", "Y must not change text under menu");
                        taps(down, 4); press(a); break; // Unavailable Desktop mode overlays the keyboard; Center is now usable.
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
                        check(shell.page() == 4 && focusIs("journey-primary"), "R1 escapes keyboard to Hall of Fame");
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
                        check(shell.page() == 4 && focusIs("journey-primary"), "Hall of Fame focus");
                        press(b);
                        check(shell.page() == 4, "Back must not leave primary page");
                        capture("landscape-letterbox");
                        window->resize(1920, 1080); break;
                    case 27:
                        capture("hall-of-fame-1080p");
                        taps(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 4);
                        adapter.setSource("crystal-1", ""); shell.refreshLibrary();
                        press(SDL_CONTROLLER_BUTTON_Y); break;
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
                        press(a); press(SDL_CONTROLLER_BUTTON_X); press(a);
                        press(SDL_CONTROLLER_BUTTON_Y); press(a);
                        check(shell.keyboard()->text() == "aA!", "Controller case and symbol entry");
                        break;
                    case 37:
                        check(focusIs("key-A"), "Changing key layout preserves visible focus");
                        if (auto* field = window->findChild<QObject*>("keyboard-text"))
                            check(field->property("text").toString() == QString(3, QChar(0x2022)) + QChar(0x2502), "Rendered password contains only bullets and cursor");
                        else check(false, "Password display exists");
                        capture("keyboard-secret-symbols");
                        press(SDL_CONTROLLER_BUTTON_Y); break;
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
    }
    if (joystick) SDL_JoystickClose(joystick);
    if (virtualIndex >= 0) SDL_JoystickDetachVirtual(virtualIndex);
    if (smoke) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    return result;
}
