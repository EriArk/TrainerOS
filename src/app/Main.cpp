#include "adapters/pokemon/PokemonExperience.h"
#include "PlatformBinding.h"
#include "SessionConnections.h"
#include "integrations/adventure/CratesAdventureAdapter.h"
#include "integrations/adventure/PcAdventureAdapter.h"
#include "features/crates/CratesLibrary.h"
#ifdef TRAINER_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif
#include "core/PerformanceTrace.h"
#include "integrations/achievements/TrainerAchievementProvider.h"
#include "features/adventure/RemotePlayRuntime.h"
#include "features/adventure/RuntimeMultiplayer.h"
#include "integrations/achievements/RetroArchAchievementSession.h"
#include "features/halloffame/AchievementCelebration.h"
#include "platform/emulation/EmulatorDiscovery.h"
#include "platform/emulation/EmulatorRefresh.h"
#include <atomic>
#include "core/repository/BatoceraLibrary.h"
#include "platform/device/VolumeKeys.h"
#include "platform/device/ShellGraphics.h"
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
#include "features/adventure/LaunchPresentation.h"
#include "features/adventure/LobbyImportController.h"
#include "integrations/adventure/retroarch/RetroArchAppearance.h"
#include "platform/input/AdventureOverlayService.h"
#include "platform/input/InvitationOverlayService.h"
#include "features/social/PlayerOverlayController.h"
#include "features/home/PlayHistoryController.h"
#include "features/home/ExitImage.h"
#include "adapters/pokemon/pokedex/ClassicArt.h"
#include "adapters/pokemon/pokedex/SpriteImages.h"
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
#include <QIcon>
#include <QPainter>
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
#include "integrations/adventure/standalone/AzaharNetplay.h"
#include <QTemporaryDir>
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
#include "DownloadsSmokeScenario.h"
#include "AppsSmokeScenario.h"
#include "PlatformSmokeScenario.h"
#include "SettingsSmokeScenario.h"
#include "AvatarSmokeScenario.h"
#include "FirstRunSmokeScenario.h"
#include "SoftwareUpdateSmokeScenario.h"
#include "CratesSmokeScenario.h"
#include "ExperienceSmokeScenario.h"
#include "LobbySmokeScenario.h"
#include "OnlineSmokeScenario.h"
#include "PlayerOverlaySmokeScenario.h"
#endif

using namespace trainer;

class DesktopAppImages final : public QQuickImageProvider {
public:
    DesktopAppImages():QQuickImageProvider(Image){
        // Gamescope is not a desktop session and may provide no platform icon
        // paths (offscreen/minimal Qt supplies only :/icons). Populate XDG paths
        // while retaining platform/user overrides and Qt's theme inheritance.
        auto paths=QIcon::themeSearchPaths();
        auto data=QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        data<<QDir::homePath()+"/.local/share/flatpak/exports/share"<<"/var/lib/flatpak/exports/share";
        QStringList fallback=QIcon::fallbackSearchPaths();
        for(const auto& base:data){
            if(QDir(base+"/icons").exists())paths<<base+"/icons";
            if(QDir(base+"/pixmaps").exists())fallback<<base+"/pixmaps";
        }
        if(QDir(QDir::homePath()+"/.icons").exists())paths.prepend(QDir::homePath()+"/.icons");
        paths.removeDuplicates();fallback.removeDuplicates();
        QIcon::setThemeSearchPaths(paths);QIcon::setFallbackSearchPaths(fallback);
        if (QIcon::fallbackThemeName().isEmpty() || QIcon::fallbackThemeName()==QStringLiteral("hicolor")) {
            // Reuse an installed full application theme when no desktop supplies
            // one. Papirus also covers common Steam/Wine launcher icon names.
            for(const auto& theme:{QStringLiteral("Papirus"),QStringLiteral("breeze")}) {
                const bool installed=std::any_of(paths.cbegin(),paths.cend(),[&](const QString& path){return QFileInfo::exists(path+'/'+theme+"/index.theme");});
                if(installed){QIcon::setFallbackThemeName(theme);break;}
            }
        }
    }
    QImage requestImage(const QString& id,QSize* size,const QSize& requested) override {
        const auto name=QUrl::fromPercentEncoding(id.toUtf8());
        const QSize target=requested.isValid()?requested.boundedTo(QSize(256,256)):QSize(64,64);
        const QIcon icon=QFileInfo(name).isAbsolute()?QIcon(name):QIcon::fromTheme(name);
        auto result=icon.pixmap(target).toImage();
        if(result.isNull()) {
            result=QImage(target,QImage::Format_ARGB32_Premultiplied);result.fill(Qt::transparent);
            QPainter p(&result);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(QColor("#8cbaa5"));
            p.drawRoundedRect(result.rect().adjusted(2,2,-2,-2),10,10);p.setPen(QColor("#284337"));
            p.drawText(result.rect(),Qt::AlignCenter,QStringLiteral("▦"));
        }
        if(size)*size=result.size();return result;
    }
};

class SavedExitImages final : public QQuickImageProvider {
public:
    explicit SavedExitImages(LocalStateStore* store) : QQuickImageProvider(Image), store_(store) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override {
        PerformanceTrace::Scope perf("SavedExitImages.requestImage");
        const auto frame = frameExitImage(store_ ? store_->exitImage(id) : QImage{});
        if (size) *size = frame.size();
        if(requested.isValid() && (requested.width()<frame.width() || requested.height()<frame.height()))
            return frame.scaled(requested.boundedTo(frame.size()),Qt::KeepAspectRatio,Qt::SmoothTransformation);
        return frame;
    }
private:
    LocalStateStore* store_;
};
class SavedScreenImages final : public QQuickImageProvider {
public:
    explicit SavedScreenImages(LocalStateStore* store) : QQuickImageProvider(Image),store_(store) {}
    QImage requestImage(const QString& id,QSize* size,const QSize& requested) override {
        const auto frame=store_?store_->screenImage(id):QImage{};
        if(size)*size=frame.size();
        return requested.isValid() && (requested.width()<frame.width() || requested.height()<frame.height())
            ? frame.scaled(requested.boundedTo(frame.size()),Qt::KeepAspectRatio,Qt::SmoothTransformation):frame;
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
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#ifdef TRAINER_WEBENGINE
    QtWebEngineQuick::initialize();
#endif
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    QGuiApplication app(argc, argv);
    configureShellGraphics();
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
    parser.addOption({"settings-smoke-test", "Verify Settings and game input editor with isolated data."});
    parser.addOption({"avatar-smoke-test", "Verify local avatar editor and preview with isolated data."});
    parser.addOption({"platform-smoke-test", "Render and exercise the host-owned SDK widget with isolated data."});
    parser.addOption({"first-run-smoke-test", "Exercise first setup with disposable profiles and local service fixtures."});
    parser.addOption({"updates-smoke-test", "Exercise Settings Updates with disposable local process fixtures."});
    parser.addOption({"player-overlay-smoke-test", "Verify player bubbles and mini chat with isolated provider and input fixtures."});
    parser.addOption({"lobby-smoke-test", "Verify lobby and keyboard QML with isolated provider fixtures."});
    parser.addOption({"online-smoke-test", "Verify Online navigation, profile return and compact layouts with isolated data."});
    parser.addOption({"experience-smoke-test", "Verify independent experience modules in the real QML host."});
    parser.addOption({"apps-smoke-test", "Verify the Apps workspace with isolated providers and SDL input."});
    parser.addOption({"crates-smoke-test", "Verify the Crates catalogue and project popup with isolated preview data."});
    parser.addOption({"downloads-smoke-test", "Verify the Downloads layout and SDL navigation with isolated queue fixtures, then exit."});
    parser.addOption({"exit-smoke-test", "Verify the isolated Adventure exit window through SDL input, then exit."});
    parser.addOption({"worlds-smoke-test", "Verify Worlds browsing and mock actions through SDL input, then exit."});
    parser.addOption({"pokedex-smoke-test", "Verify Pokédex filters, search and records through SDL input, then exit."});
    parser.addOption({"art-smoke-test", "Exercise private artwork bootstrap on the complete offline reference, then exit."});
    parser.addOption({"hall-smoke-test", "Verify Hall of Fame archive and achievement states through SDL input, then exit."});
    parser.addOption({"achievement-gallery-smoke-test", "Verify the current-game RA gallery with isolated fixtures."});
    parser.addOption({"diagnostics-smoke-test", "Verify controller/display checks and a local report through SDL input, then exit."});
    parser.addOption({"persistence-smoke-test", "Verify persistent controller flows in a test data directory.", "phase"});
#endif
    parser.addOption({"screenshot-dir", "Save smoke-test screenshots to this directory.", "directory"});
    parser.process(app);
    bool worldsSmoke = false;
    bool pokedexSmoke = false;
    bool artSmoke = false;
    bool hallSmoke = false;
    bool diagnosticsSmoke = false, settingsSmoke = false;
    bool exitSmoke = false;
    bool downloadsSmoke = false, appsSmoke = false, cratesSmoke = false, platformSmoke = false, avatarSmoke = false, firstRunSmoke = false, updatesSmoke = false;
    bool experienceSmoke = false;
    bool lobbySmoke = false, playerOverlaySmoke = false, onlineSmoke = false;
    QString persistencePhase;
#ifdef TRAINEROS_UI_TESTS
    platformSmoke = parser.isSet("platform-smoke-test");
    avatarSmoke = parser.isSet("avatar-smoke-test");
    firstRunSmoke = parser.isSet("first-run-smoke-test");
    updatesSmoke = parser.isSet("updates-smoke-test");
    playerOverlaySmoke = parser.isSet("player-overlay-smoke-test");
    lobbySmoke = parser.isSet("lobby-smoke-test");
    onlineSmoke = parser.isSet("online-smoke-test");
    experienceSmoke = parser.isSet("experience-smoke-test");
    downloadsSmoke = parser.isSet("downloads-smoke-test");
    appsSmoke = parser.isSet("apps-smoke-test");
    cratesSmoke = parser.isSet("crates-smoke-test");
    exitSmoke = parser.isSet("exit-smoke-test");
    worldsSmoke = parser.isSet("worlds-smoke-test");
    pokedexSmoke = parser.isSet("pokedex-smoke-test");
    artSmoke = parser.isSet("art-smoke-test");
    hallSmoke = parser.isSet("hall-smoke-test") || parser.isSet("achievement-gallery-smoke-test");
    diagnosticsSmoke = parser.isSet("diagnostics-smoke-test");
    settingsSmoke = parser.isSet("settings-smoke-test");
    persistencePhase = parser.value("persistence-smoke-test");
    if (parser.isSet("persistence-smoke-test") && (!QStringList{"seed", "verify", "error", "library-seed", "library-verify", "library-final", "library-launch", "library-home", "library-home-reopen", "library-center", "collection"}.contains(persistencePhase)
            || parser.value("data-dir").isEmpty() || parser.isSet("ephemeral"))) return 2;
#endif
    const bool smoke = onlineSmoke || settingsSmoke || avatarSmoke || updatesSmoke || firstRunSmoke || platformSmoke || cratesSmoke || appsSmoke || playerOverlaySmoke || lobbySmoke || experienceSmoke || parser.isSet("smoke-test") || downloadsSmoke || artSmoke || exitSmoke || worldsSmoke || pokedexSmoke || hallSmoke || diagnosticsSmoke || !persistencePhase.isEmpty();
    std::unique_ptr<QTemporaryDir> setupTestData;
    if(firstRunSmoke || updatesSmoke) {
        setupTestData=std::make_unique<QTemporaryDir>();
        if(!setupTestData->isValid())return 2;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,setupTestData->path());
    }
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
        SessionConnections bindings;
        restartTrainer = false;
        MockLibraryRepository repository;
        MockTrainerRepository profiles;
#ifdef TRAINEROS_UI_TESTS
        if (platformSmoke || avatarSmoke || onlineSmoke) profiles.save({"sdk-demo", "Developer"});
#endif
        MockAdventureAdapter adapter;
        UnconfiguredAdventureAdapter unconfiguredAdapter;
        LocalFileCatalog files;
        ArmadaPlatformService platform(!smoke && !parser.isSet("ephemeral"));
        MockPokedexRepository dex;
        OfflinePokedex offlineDex;
        if (pokedexSmoke) dex.failNextLoad();
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        if (hallSmoke || diagnosticsSmoke || firstRunSmoke) shellAchievements.enableAccountPreview();
        if(firstRunSmoke)shellAchievements.setAccount({});
        std::unique_ptr<LocalStateStore> store;
        QString stateDirectory;
        if (firstRunSmoke || (!smoke && !parser.isSet("ephemeral")) || !persistencePhase.isEmpty()) {
            const auto directory = firstRunSmoke ? setupTestData->path() : parser.isSet("data-dir") ? QDir(parser.value("data-dir")).absolutePath()
                : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            stateDirectory = directory;
            store = std::make_unique<LocalStateStore>(directory, nullptr, smoke && !persistencePhase.startsWith("library-") ? "prototype-library-v1" : "user-library-v1");
        }
        if(store && (!smoke || firstRunSmoke))store->enforceAccess(trainerGrant,chooseTrainer);
        chooseTrainer=false;
        trainerGrant.clear();
        const bool personalLibrary = store && (!smoke || persistencePhase.startsWith("library-") || persistencePhase == "collection");
        CollectionRepository collection(personalLibrary ? static_cast<LibraryRepository&>(*store) : repository);
        LibraryRepository& baseLibrary = personalLibrary ? (!smoke || persistencePhase == "collection" ? static_cast<LibraryRepository&>(collection) : *store) : repository;
        // State thumbnails are migration evidence, not normal launch targets.
        const auto libraryRoot = parser.isSet("roms-dir") ? QDir(parser.value("roms-dir")).absolutePath()
            : FirstRunController::libraryRoot(stateDirectory, QDir::home().filePath("Emulation/roms"));
        BatoceraLibrary folders(baseLibrary, libraryRoot);
        folders.observeIdentity=builtinExperienceIdentity;
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
            bindings.connect(store.get(), &LocalStateStore::opened, store.get(),
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
        StandaloneAdapter azahar("azahar", activeLibrary, standaloneInstallation("azahar"));
        StandaloneAdapter vita3k("vita3k", activeLibrary, standaloneInstallation("vita3k"));
        StandaloneAdapter dolphin("dolphin", activeLibrary, standaloneInstallation("dolphin"));
        StandaloneAdapter ppsspp("ppsspp", activeLibrary, standaloneInstallation("ppsspp"));
        StandaloneAdapter armsx2("armsx2", activeLibrary, standaloneInstallation("armsx2"));
        StandaloneAdapter cemu("cemu", activeLibrary, standaloneInstallation("cemu"));
        StandaloneAdapter duckstation("duckstation", activeLibrary, standaloneInstallation("duckstation"));
        StandaloneAdapter rpcs3("rpcs3", activeLibrary, standaloneInstallation("rpcs3"));
        StandaloneAdapter eden("eden", activeLibrary, standaloneInstallation("eden"));
        StandaloneAdapter xemu("xemu", activeLibrary, standaloneInstallation("xemu"));
        struct SaveRuntimes { RetroArchInstallation retroarch; StandaloneInstallation melonds; };
        auto saveRuntimes = std::make_shared<std::atomic<std::shared_ptr<const SaveRuntimes>>>(
            std::make_shared<const SaveRuntimes>(SaveRuntimes{retroarchInstallation, melonDsInstallation}));
        CratesAdventureAdapter cratesRuntime;
        PcAdventureAdapter pcRuntime;
        cratesRuntime.root=QDir(stateDirectory).filePath("crates");
        cratesRuntime.helper="/var/opt/traineros/crates/launch.py";
        AdapterRouter adapters({&pcRuntime,&cratesRuntime,&ppsspp, &armsx2, &cemu, &duckstation, &rpcs3, &eden, &xemu, &melonDs, &dolphin, &azahar, &vita3k, &retroarch});
        if (personalLibrary && !smoke) selectedAdapter = &adapters;
#ifdef TRAINEROS_UI_TESTS
        ProbeAdventureAdapter probeAdapter;
        if (persistencePhase == "library-launch" || persistencePhase.startsWith("library-home")) selectedAdapter = &probeAdapter;
#endif
        ExperienceFactory experienceFactory=builtinExperiences(
                              (personalLibrary && !smoke) || artSmoke ? static_cast<PokedexReferenceProvider&>(offlineDex) : dex,
                              store ? static_cast<PokedexProgressRepository&>(*store) : dex);
#ifdef TRAINEROS_UI_TESTS
        if(experienceSmoke) {
            const auto builtin=experienceFactory;
            const auto games=activeLibrary.adventures();
            experienceFactory=[builtin,games](ExperienceServices services){
                auto modules=builtin(services);
                auto rpg=std::make_unique<test::CounterExperience>("rpg");rpg->matchGame=games.at(0).id;
                auto racing=std::make_unique<test::CounterExperience>("racing");racing->matchGame=games.at(1).id;
                modules.insert(modules.begin(),std::move(racing));modules.insert(modules.begin(),std::move(rpg));return modules;
            };
        }
#endif
        ShellController shell(activeLibrary,
                              store ? static_cast<TrainerRepository&>(*store) : profiles,
                              *selectedAdapter, platform,experienceFactory,
                              personalLibrary && !smoke ? static_cast<HallOfFameRepository&>(*store) : shellArchive,
                              realAchievements ? static_cast<AchievementProvider&>(*realAchievements) : shellAchievements);
        shell.configureServices(&files, store.get());
        shell.settings()->configureNearby(pokemonModule(shell).party()->activities()->link());
        shell.social()->setLink(pokemonModule(shell).party()->activities()->link());
        if(personalLibrary && !smoke) {
            shell.scraper()->configure(stateDirectory);
            folders.setDisplayPreferences(shell.scraper()->displayPreferences());
            bindings.connect(shell.scraper(),&ScrapeController::displayChanged,&shell,[&]{folders.setDisplayPreferences(shell.scraper()->displayPreferences());shell.refreshLibrary();});
            bindings.connect(shell.scraper(),&ScrapeController::saved,&folders,&BatoceraLibrary::rescan);
        }
        if(personalLibrary && !smoke && store) {
            bindings.connect(store.get(), &LocalStateStore::opened, &shell, [&](bool ready) {
                shell.social()->setOwner(ready ? store->ownerId() : QString());
                if(ready)shell.social()->configureLocalLobby(stateDirectory,shell.trainer()->profile()["name"].toString());
                shell.backendAccount()->configure(ready ? store->ownerId() : QString());
                shell.keyboard()->configure(stateDirectory,ready ? store->ownerId() : QString());
                shell.collectionManager()->configure(stateDirectory,ready?store->ownerId():QString());
                shell.apps()->configure(stateDirectory,ready?store->ownerId():QString());
                shell.files()->configure(stateDirectory,ready?store->ownerId():QString());
                shell.pcGames()->configure(stateDirectory,ready?store->ownerId():QString());
                shell.gamePerformance()->configure(stateDirectory,ready?store->ownerId():QString());
                shell.settings()->gameInput()->configure(&activeLibrary,stateDirectory,ready?store->ownerId():QString());
                shell.settings()->setMediaOwner(ready?store->ownerId():QString());
                shell.browser()->configure(stateDirectory,ready?store->ownerId():QString());
                shell.album()->configure(stateDirectory,ready?store->ownerId():QString());
            });
            bindings.connect(store.get(), &LocalStateStore::accessNeeded, &shell, [&] {
                shell.social()->setOwner({});
                shell.settings()->setMediaOwner({});
                shell.backendAccount()->configure({});
                shell.keyboard()->configure(stateDirectory,{});
                shell.collectionManager()->configure(stateDirectory,{});
                shell.apps()->configure(stateDirectory,{});
                shell.files()->configure(stateDirectory,{});
                shell.pcGames()->configure(stateDirectory,{});
                shell.gamePerformance()->configure(stateDirectory,{});
                shell.browser()->configure(stateDirectory,{});
                shell.album()->configure(stateDirectory,{});
            });
        }
        if (personalLibrary && !smoke) {
            folders.prepareInstallation = [&adapters](AdventureRegistration& record) { adapters.prepareInstallation(record); };
            folders.prepareFileMove = [saveRuntimes](const AdventureRegistration& record,LibraryEdit& edit)->QString {
                return retroarch::prepareFileMove(record,edit,saveRuntimes->load()->retroarch);
            };
            bindings.connect(store.get(), &LocalStateStore::opened, &folders, [&](bool ready) { if(ready)folders.refreshContentAvailability(); });
            shell.settings()->setLibraryScanState(true,false);
            bindings.connect(&folders, &BatoceraLibrary::busyChanged, &shell, [&] {
                shell.settings()->setLibraryScanState(true,folders.busy());
            });
            bindings.connect(&folders, &BatoceraLibrary::scanFinished, &shell, [&](int added,const QStringList& warnings) {
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
        if (store) bindings.connect(store.get(), &LocalStateStore::libraryChanged, &shell, [&] {
            // A folder import can bind dozens of games. Rebuild pages once when
            // that batch finishes instead of after every individual SQLite write.
            if(!folders.busy())shell.refreshLibrary();
        });
        if(realAchievements)bindings.connect(realAchievements.get(),&AchievementProvider::achievementsEarned,&shell,&ShellController::showAchievements);
        if (realAchievements) bindings.connect(store.get(), &LocalStateStore::opened, realAchievements.get(), [&](bool success) {
            if (success) realAchievements->bind(store->accountDirectory());
        });
        SessionState session(shell, store.get());
#ifdef TRAINEROS_UI_TESTS
        if(firstRunSmoke)prepareFirstRunSmoke(session,shell,stateDirectory);
#endif
        if(personalLibrary && !smoke) {
            session.firstRun()->configure(stateDirectory,libraryRoot);
            const auto applyLibraryRoot=[&](const QString& root)->QString {
                if(shell.scraper()->busy())return "Finish or cancel downloads before changing library storage.";
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
        session.setTrainerSwitchGuard([&]{return !shell.pcGames()->busy() && !shell.scraper()->busy() && !shell.moduleServiceBusy() && (!realAchievements || !realAchievements->accountBusy());});
        session.setTrainerRemovalPreparation([&]() -> QString {
            if(!realAchievements)return {};
            return realAchievements->disconnectForRemoval()?QString():"Could not remove the saved sign-in. Try again.";
        });
        bindings.connect(&session,&SessionState::trainerRestartReady,&app,[&]{trainerGrant=session.restartGrant();chooseTrainer=trainerGrant.isEmpty();restartTrainer=true;app.exit();});
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
        bindings.connect(&volumeKeys, &VolumeKeys::adjustmentRequested, &deviceService,
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
            pokemonModule(shell).center()->configure(saveBackups.get());
            pokemonModule(shell).party()->configureMovement(saveBackups.get(),&activeLibrary);
            shell.settings()->configureSavePolicy(saveBackups.get());
            bindings.connect(saveBackups.get(),&SaveBackupService::operationFailed,&session,&SessionState::cancelPendingExit);
        }
        const auto updateServiceActivity = [&] {
            session.setServiceActive(shell.pcGames()->busy() || shell.moduleServiceBusy() || folders.writing() || shell.settings()->storage()->busy() || deviceService.busy() || shell.network()->busy() || (saveBackups && saveBackups->busy()));
        };
        bindings.connect(&folders, &BatoceraLibrary::writingChanged, &session, updateServiceActivity);
        bindings.connect(&deviceService, &DeviceService::changed, &session, updateServiceActivity);
        bindings.connect(shell.network(), &NetworkController::changed, &session, updateServiceActivity);
        bindings.connect(shell.settings()->storage(), &LibraryStorageController::changed, &session, updateServiceActivity);
        if (saveBackups) bindings.connect(saveBackups.get(), &SaveBackupService::busyChanged, &session, updateServiceActivity);
        bindings.connect(&shell,&ShellController::changed,&session,updateServiceActivity);
        ProcessService adventureProcess;
        quint64 experienceSession=0;
        bindings.connect(&adventureProcess,&ProcessService::started,&shell,[&]{++experienceSession;});
        const auto liveExperienceSession=[&]{return adventureProcess.active()?QString::number(adventureProcess.processId())+":"+QString::number(experienceSession):QString();};
        AdventureLaunchController adventureLaunch(adventureProcess);
        adventureLaunch.configureGame=[&](ProcessCommand command,const QString& game){return shell.gamePerformance()->prepare(std::move(command),game);};
        bindings.connect(&adventureLaunch,&AdventureLaunchController::changed,&shell,[&]{
            shell.gamePerformance()->setLiveAvailable(adventureLaunch.state()=="running" && adventureProcess.supportsRuntimeSettings());
        });
        shell.gamePerformance()->applyLive=[&](const QString& game,const QJsonObject& settings,std::function<void(QJsonObject)> done){
            if(adventureLaunch.state()!="running" || adventureLaunch.adventureId()!=game)return false;
            return adventureProcess.updateSettings(settings,shell.gamePerformance(),std::move(done));
        };
        std::unique_ptr<PlatformBinding> platformSdk;
        // Publish discovery only when the GUI event loop can answer clients,
        // after synchronous QML/startup work has finished.
        if (!smoke) QTimer::singleShot(0, &shell, [&] {
            platformSdk = std::make_unique<PlatformBinding>(shell, adventureLaunch, PlatformHost::defaultEndpoint());
        });
        const auto storageApply=shell.settings()->storage()->apply;
        if(storageApply)shell.settings()->storage()->apply=[&,storageApply](const QString& root){
            return adventureLaunch.active()?QString("Exit the running game before changing game storage."):storageApply(root);
        };
        AdventureExitPresentation exitPresentation(adventureLaunch.exitController());
        InvitationOverlayService appsOverlay(platform.dedicatedSession(),"/var/opt/traineros/integrations/invitation-overlay.py");
        bindings.connect(shell.apps(),&AppsController::changed,&appsOverlay,[&]{appsOverlay.setGame(shell.apps()->processId());});
        InvitationOverlayService invitationOverlay(platform.dedicatedSession(),"/var/opt/traineros/integrations/invitation-overlay.py");
        InvitationOverlayService radialOverlay(platform.dedicatedSession(),"/var/opt/traineros/integrations/invitation-overlay.py");
        bindings.connect(&adventureProcess,&ProcessService::started,&radialOverlay,[&]{radialOverlay.setGame(adventureProcess.processId());});
        bindings.connect(&adventureProcess,&ProcessService::finished,&radialOverlay,[&]{radialOverlay.setGame(0);shell.settings()->gameInput()->setRadial({});});
        InvitationOverlayService achievementOverlay(platform.dedicatedSession(),"/var/opt/traineros/integrations/invitation-overlay.py");
        AchievementCelebration achievementCelebration(store.get(),realAchievements?static_cast<AchievementProvider&>(*realAchievements):shellAchievements);
        bindings.connect(&achievementCelebration,&AchievementCelebration::notice,&shell,&ShellController::showNotice);
        bindings.connect(&adventureProcess,&ProcessService::runtimeOutput,&achievementCelebration,&AchievementCelebration::feed);
        bindings.connect(&adventureProcess,&ProcessService::started,&achievementCelebration,[&]{
            achievementOverlay.setGame(adventureProcess.processId());
            achievementCelebration.begin(adventureLaunch.adventureId(),adventureProcess.processId(),adventureProcess.runtimeControls()["achievements"].toMap());
        });
        bindings.connect(&adventureProcess,&ProcessService::finished,&achievementCelebration,[&]{achievementCelebration.finish();achievementOverlay.setGame(0);});
        if(store) {
            const auto updateMoments=[&]{shell.hall()->setMoments(store->achievementMoments());};
            bindings.connect(store.get(),&LocalStateStore::opened,&achievementCelebration,[updateMoments](bool){updateMoments();});
            bindings.connect(store.get(),&LocalStateStore::libraryChanged,&achievementCelebration,updateMoments);
        }
        bindings.connect(&adventureProcess,&ProcessService::started,&invitationOverlay,[&]{invitationOverlay.setGame(adventureProcess.processId());});
        bindings.connect(&adventureProcess,&ProcessService::finished,&invitationOverlay,[&]{invitationOverlay.setGame(0);});
        bindings.connect(&adventureProcess,&ProcessService::started,shell.social(),[&]{shell.social()->setGameProcess(adventureProcess.processId());});
        bindings.connect(&adventureProcess,&ProcessService::finished,shell.social(),[&]{shell.social()->setGameProcess(0);});
        std::unique_ptr<AdventureOverlayService> adventureOverlay;
        TextEntryController externalKeyboard;
        InvitationOverlayService keyboardOverlay(platform.dedicatedSession(),"/var/opt/traineros/integrations/invitation-overlay.py");
        int keyboardOwner=0; // 1: application service; 2: owned adventure helper.
        const auto keyboardSend=[&](QJsonObject packet){
            if(keyboardOwner==1)shell.apps()->keyboardCommand(packet);
            else if(keyboardOwner==2&&adventureOverlay)adventureOverlay->keyboardCommand(packet);
        };
        const auto keyboardEvent=[&](int owner,const QJsonObject& packet){
            const auto event=packet["event"].toString();
            if(event=="keyboard") {
                if(!keyboardOverlay.ready()){if(owner==1)shell.apps()->keyboardCommand({{"command","keyboard-close"}});else if(adventureOverlay)adventureOverlay->keyboardCommand({{"command","keyboard-close"}});return;}
                externalKeyboard.cancel();
                keyboardOverlay.setGame(owner==1?shell.apps()->processId():adventureProcess.processId());
                externalKeyboard.configure(stateDirectory,store?store->ownerId():QString());
                keyboardOwner=owner;externalKeyboard.beginLive(0,"Keyboard");
            } else if(owner==keyboardOwner&&event=="keyboard-closed")externalKeyboard.cancel();
            else if(owner==keyboardOwner&&event=="keyboard-action") {
                static const QMap<QString,Action> actions{{"confirm",Action::Confirm},{"back",Action::Back},{"up",Action::Up},{"down",Action::Down},{"left",Action::Left},{"right",Action::Right},{"select",Action::LocalAction},{"secondary",Action::Secondary},{"recent",Action::ToggleContinue},{"previousFace",Action::PreviousFace},{"nextFace",Action::NextFace}};
                const auto name=packet["action"].toString();if(actions.contains(name))externalKeyboard.dispatch(actions[name]);
            }
        };
        bindings.connect(shell.apps(),&AppsController::keyboardEvent,&externalKeyboard,[&](const QJsonObject& packet){keyboardEvent(1,packet);});
        bindings.connect(shell.apps(),&AppsController::changed,&externalKeyboard,[&]{if(keyboardOwner==1&&(!shell.apps()->ready()||shell.apps()->current().isEmpty()))externalKeyboard.cancel();});
        bindings.connect(&keyboardOverlay,&InvitationOverlayService::changed,&externalKeyboard,[&]{if(!keyboardOverlay.ready())externalKeyboard.cancel();});
        bindings.connect(&session,&SessionState::changed,&externalKeyboard,[&]{if(session.blocked())externalKeyboard.cancel();});
        bindings.connect(&externalKeyboard,&TextEntryController::changed,&shell,[&]{if(!externalKeyboard.isOpen()&&keyboardOwner){keyboardSend({{"command","keyboard-close"}});keyboardOwner=0;}});
        bindings.connect(&externalKeyboard,&TextEntryController::textCommitted,&shell,[&](QString text){keyboardSend({{"command","keyboard-input"},{"text",text}});});
        bindings.connect(&externalKeyboard,&TextEntryController::keyRequested,&shell,[&](QString action){keyboardSend({{"command","keyboard-input"},{"action",action}});});
        bindings.connect(&externalKeyboard,&TextEntryController::keyStroke,&shell,[&](int key,int modifiers,QString text){keyboardSend({{"command","keyboard-input"},{"key",key},{"modifiers",modifiers},{"text",text}});});
        PlayerOverlayController playerOverlay(*shell.social(),*shell.keyboard(),exitPresentation);
        // Incoming calls reuse the game's captured-frame/input lease. They never
        // create a second input owner or change the game's save/exit lifecycle.
        bool callOwnsGameMenu=false,callUsesContentInput=false,previousCallContentInput=false;
        QString gameCallAttempt;
        const auto syncIncomingCall=[&]{
            const auto call=shell.social()->incomingCall();
            const bool show=!call.isEmpty()&&!session.blocked()&&adventureLaunch.active()&&!adventureLaunch.minimized();
            if(show) {
                if(!exitPresentation.menuOpen()&&!exitPresentation.capturing()
                   &&adventureOverlay&&adventureLaunch.exitController().available()
                   &&adventureLaunch.exitController().phase()==AdventureExitController::Phase::Idle
                   &&gameCallAttempt!=call["id"].toString()) {
                    gameCallAttempt=call["id"].toString();
                    callOwnsGameMenu=true;
                    // The helper must first acquire the input lease and raise
                    // the overlay; requesting only a preview bypasses that step.
                    adventureOverlay->openOptions();
                }
                if(exitPresentation.menuOpen()&&!callUsesContentInput) {
                    previousCallContentInput=exitPresentation.contentInput();callUsesContentInput=true;
                    exitPresentation.setContentInput(true);
                }
            } else {
                gameCallAttempt.clear();
                if(callUsesContentInput){callUsesContentInput=false;exitPresentation.setContentInput(previousCallContentInput);}
                if(callOwnsGameMenu){callOwnsGameMenu=false;exitPresentation.dismissMenu();}
            }
        };
        bindings.connect(shell.social(),&SocialController::changed,&exitPresentation,syncIncomingCall,Qt::QueuedConnection);
        bindings.connect(&exitPresentation,&AdventureExitPresentation::changed,&exitPresentation,syncIncomingCall,Qt::QueuedConnection);
        bindings.connect(&adventureProcess,&ProcessService::started,&playerOverlay,[&]{
            // Replies bind to every owned game; a gathering lobby must not
            // expose its roster over an unrelated solo game.
            playerOverlay.setGame(adventureProcess.processId(),shell.social()->gameParty()["running"].toBool());
        });
        bindings.connect(&adventureProcess,&ProcessService::finished,&playerOverlay,[&]{playerOverlay.setGame(0);});
        bindings.connect(&playerOverlay,&PlayerOverlayController::optionsRequested,&shell,[&]{if(adventureOverlay)adventureOverlay->openOptions();});
        bindings.connect(shell.social(),&SocialController::lobbyRequested,&shell,[&]{
            if(adventureLaunch.active()&&!adventureLaunch.minimized()) {
                if(!adventureOverlay||!adventureOverlay->requestShell())shell.showNotice("Open Home to enter the lobby safely.");
            }
        });
        bindings.connect(shell.social(),&SocialController::lobbyDeferred,&shell,[&]{
            if(adventureLaunch.active()&&adventureOverlay)adventureOverlay->returnToGame();
            else shell.goToPage(0);
        });
        QString pendingNotification, pendingSystemAction;
        shell.powerControl()->prepareSleep=[&](std::function<void(QString)> done){
            if(adventureLaunch.active()){done("Save and close the game before sleeping. Live-game sleep is not validated yet.");return;}
              for(const auto& task:shell.files()->service()->tasks())if(!task.toMap()["terminal"].toBool()){
                  done("Finish the current file operation before sleeping.");return;
              }
            if(shell.pcGames()->busy()||session.blocked()||shell.moduleServiceBusy()||folders.writing()||shell.settings()->storage()->busy()
               ||(saveBackups&&saveBackups->busy())){done("Finish the current save or file operation before sleeping.");return;}
            session.flushForSleep();
            auto* wait=new QTimer(&session);wait->setInterval(50);
            auto ticks=std::make_shared<int>(0);
            bindings.connect(wait,&QTimer::timeout,&session,[&,wait,ticks,done]{
                  bool guarded=shell.pcGames()->busy()||adventureLaunch.active()||session.blocked()||shell.moduleServiceBusy()||folders.writing()
                    ||shell.settings()->storage()->busy()||(saveBackups&&saveBackups->busy());
                  for(const auto& task:shell.files()->service()->tasks())guarded|=!task.toMap()["terminal"].toBool();
                if(!guarded&&!session.writesPending()){wait->stop();wait->deleteLater();done({});}
                else if(++*ticks>=80){wait->stop();wait->deleteLater();done("A write is still pending. Try Sleep again after it finishes.");}
            });wait->start();
        };
        bindings.connect(shell.powerControl(),&PowerControl::menuRequested,&shell,[&]{
            if(adventureLaunch.active()&&!adventureLaunch.minimized()){
                pendingSystemAction="power";
                if(adventureOverlay)adventureOverlay->requestShell();
              }else if(shell.apps()->processId()>0){
                  pendingSystemAction="power";shell.apps()->control(1);
              }else shell.systemAction("power");
        });
        bindings.connect(&adventureProcess,&ProcessService::finished,&shell,[&]{pendingSystemAction.clear();});
        bindings.connect(&shell,&ShellController::gameControlsClosed,&exitPresentation,[&](bool resume){
            exitPresentation.setContentInput(false);if(resume)exitPresentation.dismissMenu();
        });
        bindings.connect(&exitPresentation,&AdventureExitPresentation::menuDismissed,&shell,[&]{shell.setGameControlsOpen(false);});
        bindings.connect(&shell,&ShellController::gameSystemRequested,&exitPresentation,[&](QString id){
            pendingSystemAction=id;
            exitPresentation.setContentInput(false);
            if(adventureOverlay)adventureOverlay->minimize();
        });
        if(saveBackups)saveBackups->operationGuard=[&]{return adventureLaunch.active()?QString("Exit the running game before changing saves."):QString();};
        RuntimeMultiplayer multiplayer(activeLibrary,retroarch,ppsspp,dolphin,*shell.social(),adventureProcess,adventureLaunch,exitPresentation);
        LaunchPresentation launchPresentation(activeLibrary,shell.launchPreparation(),adventureLaunch,multiplayer,*shell.social(),exitPresentation);
        LobbyImportController lobbyImport(activeLibrary,*selectedAdapter,&files,*shell.social(),multiplayer);
        bindings.connect(&lobbyImport,&LobbyImportController::imported,&shell,[&]{shell.refreshLibrary();});
        RemotePlayRuntime remotePlay(activeLibrary,retroarch,*shell.social(),adventureProcess,adventureLaunch,multiplayer.gamePreparation());
        remotePlay.allowed=[&]{return personalLibrary&&!smoke&&!session.blocked()&&!(saveBackups&&saveBackups->busy())&&!pokemonModule(shell).party()->activities()->link()->active();};
        remotePlay.hostSupported=[&](const QJsonObject& profile){return multiplayer.supportsLive(profile);};
        if(personalLibrary&&!smoke&&remotePlay.configure("/var/opt/traineros/integrations/remote-play")) {
            shell.social()->lobbyRuntime()->registerService("remoteplay.v1",&remotePlay);
            multiplayer.additionalLobbyGames=[&](const QMap<QString,QJsonObject>& profiles){return remotePlay.choices(profiles);};
        }
        bindings.connect(&remotePlay,&RemotePlayRuntime::notice,&shell,&ShellController::showNotice);
        if(personalLibrary&&!smoke) {
            multiplayer.saveBackupRoot=QDir(stateDirectory).filePath("backups");
            multiplayer.resolveSaveTarget=[saveRuntimes](const AdventureRegistration& record) {
                const auto snapshot=saveRuntimes->load();
                return record.adventure.adapterId=="melonds"?resolveMelonDsSave(record,snapshot->melonds):resolveRetroArchSave(record,snapshot->retroarch);
            };
        }
        bindings.connect(&multiplayer,&RuntimeMultiplayer::invitationSurfaceRequested,&shell,[&]{shell.openGameHubFace("lobby");});
        shell.libraryTools()->multiplayerAvailable=[&](const QString& id){return multiplayer.multiplayerGames().contains(id);};
        shell.libraryTools()->contentPackage=azahar::package;
        shell.libraryTools()->installPackage=azahar::installContent;
        shell.libraryTools()->installGuard=[&]{
            if(adventureLaunch.active()||session.blocked()||shell.moduleServiceBusy())return QString("Finish the current game or operation before installing content.");
            return shell.libraryTools()->editGuard?shell.libraryTools()->editGuard():QString();
        };
        bindings.connect(&shell,&ShellController::togetherRequested,&multiplayer,[&](const QString& game){shell.openGameHubFace("lobby");multiplayer.beginTogether(game);});
        bindings.connect(shell.social(),&SocialController::runtimeAction,&multiplayer,[&](const QString& action){multiplayer.contextAction(action);});
        bindings.connect(&multiplayer,&RuntimeMultiplayer::notice,&shell,[&](QString text){
            shell.showNotice(text);
        });
        // Supported profiles must be usable in normal sessions, including owner testing.
        // Availability still follows exact runtime/content and current-session guards.
        // A focus move is not a library/runtime change. Keep permission changes
        // immediate, but do not walk every installation and rebuild all lobby
        // choices on each broad shell presentation notification.
        auto multiplayerState=[&]{return QVariantMap{
            {"trainer",shell.trainer()->profile()["name"]},
            {"owner",shell.trainer()->profile()["id"]},
            {"allowed",personalLibrary&&!smoke&&!session.blocked()&&!shell.libraryTools()->busy()&&!(saveBackups&&saveBackups->busy())&&!pokemonModule(shell).party()->activities()->link()->active()},
            {"account",shell.social()->account()["state"]},
            {"accountGeneration",qulonglong(shell.social()->accountGeneration())},
            {"runtime",adventureProcess.runtimeControls()}};};
        QVariantMap lastMultiplayerState;std::optional<quint64> lastMultiplayerRevision;
        auto refreshMultiplayer=[&]{
            lastMultiplayerState=multiplayerState();lastMultiplayerRevision=activeLibrary.libraryRevision();
            multiplayer.refresh(retroarchInstallation,lastMultiplayerState["trainer"].toString(),lastMultiplayerState["allowed"].toBool());
        };
        auto refreshMultiplayerState=[&]{
            if(!smoke&&!parser.isSet("ephemeral"))
                shell.social()->configureLocalLobby(stateDirectory,shell.trainer()->profile()["name"].toString());
            const auto revision=activeLibrary.libraryRevision();
            if(!revision || revision!=lastMultiplayerRevision || multiplayerState()!=lastMultiplayerState)refreshMultiplayer();
        };
        bindings.connect(&folders,&BatoceraLibrary::scanFinished,&multiplayer,refreshMultiplayer);
        bindings.connect(&session,&SessionState::changed,&multiplayer,refreshMultiplayerState);
        bindings.connect(shell.trainer(),&TrainerController::changed,&multiplayer,refreshMultiplayerState);
        bindings.connect(&shell,&ShellController::changed,&multiplayer,refreshMultiplayerState);
        if(store)bindings.connect(store.get(),&LocalStateStore::libraryChanged,&multiplayer,refreshMultiplayer);
        refreshMultiplayer();
        auto gameNotifications=[&]{
            QVariantList rows;
            for(const auto& value:shell.notifications()) {
                const auto row=value.toMap();
                rows.append(QVariantMap{{"id","notice:"+row["id"].toString()},
                    {"label",row["name"]},{"detail",row["detail"]},{"removable",!row["ringing"].toBool()&&!row["pending"].toBool()},{"readOnly",!(adventureOverlay && adventureOverlay->canMinimize())}});
            }
            const bool empty=rows.isEmpty();
            if(!empty)rows.append(QVariantMap{{"id","dismiss-notifications"},{"label","Clear updates"}});
            rows.append(QVariantMap{{"id","back"},{"label","Back"}});
            exitPresentation.setPanel("notifications",empty?"You're all caught up!":"Your updates",rows);
        };
        auto gameAchievementRows=[&] {
            QVariantList rows;
            auto& provider=realAchievements?static_cast<AchievementProvider&>(*realAchievements):static_cast<AchievementProvider&>(shellAchievements);
            for(const auto& set:provider.sets()) {
                if(set.adventureId!=adventureLaunch.adventureId() || !set.supported)continue;
                const auto snapshot=provider.snapshot(set.id);
                if(snapshot.context!=provider.context() || snapshot.setId!=set.id)continue;
                for(const auto& definition:snapshot.definitions) {
                    std::optional<bool> earned;
                    for(const auto& unlock:snapshot.unlocks)if(unlock.achievementId==definition.id)earned=unlock.unlocked;
                    rows.append(QVariantMap{{"id","achievement:"+set.id+":"+definition.id},{"label",definition.title},
                        {"detail",definition.description},{"readOnly",true},{"earned",earned.value_or(false)},
                        {"status",!earned.has_value()?"Not recorded":*earned?"Earned":""},
                        {"icon",definition.badgeName.isEmpty()?QString():"https://media.retroachievements.org/Badge/"+definition.badgeName+(earned==false?"_lock.png":".png")}});
                }
            }
            return rows;
        };
        auto gameMenuActions=[&]{
            QVariantList actions{QVariantMap{{"id","minimize"},{"label","Minimize"},
                {"readOnly",!(adventureOverlay && adventureOverlay->canMinimize())}}};
            if(adventureProcess.runtimeControls()["kind"]=="retroarch")actions.append(QVariantMap{{"id","display"},{"label","Game settings"},{"column","game"}});
            if(const auto r=activeLibrary.registration(adventureLaunch.adventureId());r)actions.append(QVariantMap{{"id","pc-settings"},{"label","Performance & compatibility"},{"column","game"},{"readOnly",!(adventureOverlay&&adventureOverlay->canMinimize())}});
            const auto achievements=gameAchievementRows();
            if(!achievements.isEmpty()) {
                int earned=0;for(const auto& row:achievements)if(row.toMap()["earned"].toBool())++earned;
                actions.append(QVariantMap{{"id","achievements"},{"label",QString("Achievements  ·  %1 / %2").arg(earned).arg(achievements.size())},{"column","game"}});
            }
            auto experienceActions=shell.liveExperienceActions(adventureLaunch.adventureId(),liveExperienceSession());
            for(auto& v:experienceActions){auto row=v.toMap();row["column"]="game";v=row;}actions.append(experienceActions);
            const auto lobby=shell.social()->homeLobby();
            if(!lobby["id"].toString().isEmpty())actions.append(QVariantMap{{"id","current-lobby"},{"label","Current lobby"},{"detail",lobby["title"]},{"readOnly",!(adventureOverlay && adventureOverlay->canMinimize())}});
            if(!playerOverlay.players().isEmpty())actions.append(QVariantMap{{"id","players"},{"label","Players & chat"}});
            const auto voice=shell.social()->account()["voice"].toMap();
            if(!voice["channel"].toString().isEmpty())actions.append(QVariantMap{{"id","call"},{"label","Voice call"},{"detail",voice["name"]}});
            actions.append(shell.social()->incomingCallActions());
            for(const auto& value:shell.notifications()) {
                const auto row=value.toMap();
                actions.append(QVariantMap{{"id","notice:"+row["id"].toString()},{"label",row["name"]},{"detail",row["detail"]},{"avatar",row["avatar"]},{"person",!row["name"].toString().isEmpty()},{"removable",!row["ringing"].toBool()&&!row["pending"].toBool()},
                    {"readOnly",!(adventureOverlay && adventureOverlay->canMinimize())}});
            }
            if(!shell.notifications().isEmpty())actions.append(QVariantMap{{"id","dismiss-notifications"},{"label","Clear updates"},{"detail","Keep calls and invitations"}});
            actions.append(QVariantMap{{"id","controls"},{"label","Controls"},{"detail","Start · volume, brightness and more"}});
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
        bindings.connect(shell.social(),&SocialController::changed,&exitPresentation,gameMenuActions);
        bindings.connect(&shell,&ShellController::changed,&exitPresentation,gameMenuActions);
        bindings.connect(&multiplayer,&RuntimeMultiplayer::changed,&exitPresentation,gameMenuActions);
        bindings.connect(&adventureLaunch,&AdventureLaunchController::adventureStarted,&exitPresentation,gameMenuActions);
        bindings.connect(&adventureProcess,&ProcessService::started,&exitPresentation,gameMenuActions);
        bindings.connect(&exitPresentation,&AdventureExitPresentation::changed,&exitPresentation,[&]{if(exitPresentation.menuOpen()&&exitPresentation.panel().isEmpty())gameMenuActions();});
        bindings.connect(&exitPresentation,&AdventureExitPresentation::changed,&multiplayer,&RuntimeMultiplayer::resumeLiveInvocation);
        // Other notifications retain the inbox route. The invitation badge is
        // hidden during both preview and explicit-exit capture barriers.
        bindings.connect(&exitPresentation,&AdventureExitPresentation::menuActionRequested,&exitPresentation,[&](const QString& action){
            if(action=="achievements"){auto rows=gameAchievementRows();rows.append(QVariantMap{{"id","back"},{"label","Back"}});exitPresentation.setPanel("achievements","Achievements",rows);return;}
            if(action=="controls"){exitPresentation.setContentInput(true);shell.setGameControlsOpen(true);return;}
            if(action=="pc-settings"){pendingSystemAction="pc-settings";if(adventureOverlay)adventureOverlay->minimize();return;}
            if(playerOverlay.menuAction(action))return;
            if(shell.invokeLiveExperienceAction(action,adventureLaunch.adventureId(),liveExperienceSession()))return;
            if(action=="current-lobby"){pendingSystemAction="current-lobby";if(adventureOverlay)adventureOverlay->minimize();return;}
            if(action=="minimize"){if(adventureOverlay)adventureOverlay->minimize();return;}
            if(action.startsWith("notice:")) {
                if(playerOverlay.openNotification(action.mid(7)))return;
                pendingNotification=action.mid(7);if(adventureOverlay)adventureOverlay->minimize();return;
            }
            const auto back=QVariantMap{{"id","back"},{"label","Back"}};
            const auto runtime=adventureProcess.runtimeControls();
            if(multiplayer.action(action))return;
            if(action.startsWith("answer-call:")||action.startsWith("decline-call:")) {
                shell.social()->answerCall(action.section(':',1),action.startsWith("answer-call:"));return;
            }
            if(action=="notifications") {
                gameNotifications();return;
            }
            if(action=="dismiss-notifications"){shell.dismissNotifications();gameNotifications();return;}
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
            if(action=="choose-shader"||action=="choose-bezel"||action=="display-frames") {
                const auto family=action=="choose-shader"?QString("shader"):QString("bezel");
                const auto parent=family=="shader"?QString("display-shaders"):QString("display");
                auto rows=retroarch::appearanceChoices(game,family,runtime);
                rows.append(QVariantMap{{"id",parent},{"label","Back"}});
                exitPresentation.setPanel("display-choice",family=="shader"?"Choose shader":"Choose frame",rows,parent);return;
            }
            QString category;
            if(action=="display-shaders"||action=="shader"||action=="next-shader"||action=="previous-shader"||action.startsWith("shader:"))category="shaders";
            else if(action=="display-frames"||action.startsWith("bezel:"))category="frames";
            QString caption=category=="shaders"?"Shaders":category=="frames"?"Frames":"Game settings";
            if(action=="display"&&!runtime["appearanceNotice"].toString().isEmpty())caption=runtime["appearanceNotice"].toString();
            if(action=="shader"||action=="next-shader"||action=="previous-shader") {
                if(!adventureProcess.runtimeCommand(action))caption="Couldn't change the shader";
            } else if(action.startsWith("shader:")||action.startsWith("bezel:")) {
                if(!retroarch::chooseAppearance(game,action,runtime))caption="Couldn't save display settings";
            } else if(action!="display"&&action!="display-shaders"&&action!="display-frames") {
                if(!retroarch::changeAppearance(game,action))caption="Couldn't save display settings";
            }
            if(category=="frames") {
                auto rows=retroarch::appearanceChoices(game,"bezel",runtime);
                rows.append(QVariantMap{{"id","display"},{"label","Back"}});
                exitPresentation.setPanel("display-choice",caption,rows,"display");
            } else exitPresentation.setPanel(category.isEmpty()?"display":"display-"+category,caption,
                retroarch::appearanceMenu(game,category),category.isEmpty()?QString():QString("display"));
        });
        bindings.connect(&adventureLaunch, &AdventureLaunchController::adventureStarted, &exitPresentation,
            [&](const QString& id) {
                QString title = "Adventure";
                for (const auto& adventure : activeLibrary.adventures())
                    if (adventure.id == id) { title = adventure.title; break; }
                if(adventureProcess.runtimeControls()["kind"]=="remoteplay")title=adventureProcess.runtimeControls()["title"].toString()+" · Remote Play";
                exitPresentation.setGameTitle(title);
            });
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
            if(adventureOverlay) {
                adventureOverlay->inputProfile=[&](const QString& id){return shell.pcGames()->steam.active()?QJsonObject{}:shell.settings()->gameInput()->snapshot(id);};
                bindings.connect(adventureOverlay.get(),&AdventureOverlayService::radialChanged,shell.settings()->gameInput(),&GameInputProfiles::setRadial);
                bindings.connect(shell.settings()->gameInput(),&GameInputProfiles::radialSlotRequested,adventureOverlay.get(),&AdventureOverlayService::selectRadialSlot);
                bindings.connect(adventureOverlay.get(),&AdventureOverlayService::inputMappingFailed,&shell,&ShellController::showNotice);
            }
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
            auto* practice=pokemonModule(shell).party()->activities()->practice();
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
                auto* link=pokemonModule(shell).party()->activities()->link();
                bindings.connect(link,&LinkController::connectionFailed,&shell,&ShellController::showNotice);
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
                bindings.connect(&adventureLaunch,&AdventureLaunchController::changed,link,nearbyGate);
                bindings.connect(&session,&SessionState::changed,link,nearbyGate);
                bindings.connect(&shell,&ShellController::changed,link,nearbyGate);
                bindings.connect(link,&LinkController::changed,link,nearbyGate);
                nearbyGate();
                bindings.connect(link,&LinkController::saveChanged,gameProgress.get(),[&,provider=gameProgress.get()]{
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
            bindings.connect(&shell, &ShellController::changed, gameProgress.get(), [&, refreshProgress] {
                const bool homeVisible = shell.page() == 0 || shell.page() == 2;
                const bool enteredHome = homeVisible && !progressHomeVisible;
                progressHomeVisible = homeVisible;
                refreshProgress(enteredHome);
            });
            bindings.connect(store.get(), &LocalStateStore::opened, gameProgress.get(), [refreshProgress](bool success) {
                if (success) refreshProgress(true);
            });
            bindings.connect(store.get(), &LocalStateStore::libraryChanged, gameProgress.get(), [&,refreshProgress] { if(!folders.busy())refreshProgress(true); });
            bindings.connect(&folders, &BatoceraLibrary::scanFinished, gameProgress.get(), [refreshProgress] { refreshProgress(true); });
            bindings.connect(&adventureLaunch, &AdventureLaunchController::changed, gameProgress.get(), [&, refreshProgress] {
                if (adventureLaunch.active()) { progressSelection.clear(); gameProgress->invalidate(); }
                else refreshProgress(true);
            });
            if (saveBackups) bindings.connect(saveBackups.get(), &SaveBackupService::busyChanged, gameProgress.get(), [&, refreshProgress] {
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
                changed |= azahar.updateInstallation(standalone("azahar"));
                changed |= vita3k.updateInstallation(standalone("vita3k"));
                changed |= dolphin.updateInstallation(standalone("dolphin"));
                changed |= ppsspp.updateInstallation(standalone("ppsspp"));
                changed |= armsx2.updateInstallation(standalone("armsx2"));
                changed |= cemu.updateInstallation(standalone("cemu"));
                changed |= duckstation.updateInstallation(standalone("duckstation"));
                changed |= rpcs3.updateInstallation(standalone("rpcs3"));
                changed |= eden.updateInstallation(standalone("eden"));
                changed |= xemu.updateInstallation(standalone("xemu"));
                if (changed) {
                    retroarchInstallation = next;
                    saveRuntimes->store(std::make_shared<const SaveRuntimes>(SaveRuntimes{next, ds}));
                    progressSelection.clear(); gameProgress->invalidate();
                    qInfo() << "Emulator readiness refreshed at idle";
                    shell.refreshLibrary();
                    refreshMultiplayer();
                }
                if (scanLibrary || changed) folders.refreshContentAvailability();
            };
            const auto refreshEmulators = [&] { emulatorRefresh->request(folders.root(), true); };
            bindings.connect(shell.settings(), &SettingsController::libraryRefreshRequested, emulatorRefresh.get(), refreshEmulators);
            bindings.connect(&app, &QGuiApplication::applicationStateChanged, emulatorRefresh.get(), [&](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive) emulatorRefresh->request(folders.root());
            });
            auto previousPage = std::make_shared<int>(shell.page());
            bindings.connect(&shell, &ShellController::changed, emulatorRefresh.get(), [&, previousPage, refreshEmulators] {
                const auto previous = *previousPage; *previousPage = shell.page();
                if (shell.page() == 1 && previous != 1) refreshEmulators();
            });
            emulatorPoll.setInterval(60000);
            bindings.connect(&emulatorPoll, &QTimer::timeout, emulatorRefresh.get(), [&] { emulatorRefresh->request(folders.root()); });
            bindings.connect(&emulatorPoll, &QTimer::timeout, &multiplayer, refreshMultiplayer);
            emulatorPoll.start();
        }
        shell.scraper()->canStart = [&] {
            return personalLibrary && !smoke && !adventureLaunch.active() && !session.blocked()
                && !folders.busy() && store && !store->pending() && !store->opening()
                && !shell.libraryManager()->saving() && !shell.libraryTools()->busy()
                && !shell.settings()->storage()->busy() && !(saveBackups && saveBackups->busy());
        };
        auto* softwareUpdates=shell.settings()->updates();
        if(!smoke && personalLibrary && qEnvironmentVariable("TRAINEROS_SOFTWARE_MANAGED")=="1") {
            softwareUpdates->maintenanceGuard=[&]() -> QString {
                if(adventureLaunch.active() || session.blocked() || shell.launchPreparation().busy()
                    || shell.moduleServiceBusy() || folders.busy() || folders.writing() || !store || store->pending() || store->opening()
                    || session.writesPending() || shell.scraper()->busy() || shell.libraryManager()->saving() || shell.libraryTools()->busy()
                    || shell.settings()->storage()->busy() || deviceService.busy() || shell.network()->busy()
                    || (saveBackups && saveBackups->busy()))
                    return "Finish the game and current save, file or download operation before restarting TrainerOS.";
                return {};
            };
            softwareUpdates->attachInstallation(qEnvironmentVariable("TRAINEROS_SOFTWARE_ROOT"),stateDirectory,QCoreApplication::applicationFilePath());
        }
        ControllerInput input(nullptr, preferred);
        shell.settings()->setController(&input);
        remotePlay.ownerDevice=[&]{return input.kernelDeviceName();};
        const auto updateTextInput=[&]{input.setTextEntryEnabled(shell.social()->incomingCall().isEmpty()&&shell.keyboard()->isOpen()&&(playerOverlay.chatOpen()?exitPresentation.ready():!shell.menuOpen()&&shell.notice().isEmpty()&&!shell.homeMenuOpen()));};
        bindings.connect(&shell,&ShellController::changed,&input,updateTextInput);
        bindings.connect(shell.keyboard(),&TextEntryController::changed,&input,updateTextInput);
        bindings.connect(&exitPresentation,&AdventureExitPresentation::changed,&input,updateTextInput);
        QString inputCall;
        bindings.connect(shell.social(),&SocialController::changed,&input,[&]{
            const auto id=shell.social()->incomingCall()["id"].toString();
            if(id!=inputCall){inputCall=id;input.requireNeutral();updateTextInput();}
        });
        bindings.connect(&input,&ControllerInput::textEntered,shell.keyboard(),&TextEntryController::insertText);
        bindings.connect(&input,&ControllerInput::textBackspace,shell.keyboard(),&TextEntryController::backspace);
        bindings.connect(&input,&ControllerInput::textCommand,&shell,[&](const QString& command){
            auto* keyboard=shell.keyboard();
            if(command=="left"||command=="right")keyboard->moveCursor(command=="left"?-1:1);
            else keyboard->shortcut(command);
        });
        bindings.connect(&input,&ControllerInput::textSubmitted,&shell,[&]{shell.keyboard()->dispatch(Action::LocalAction);});
        bool homeMenuWasOpen = false;
        bindings.connect(&shell, &ShellController::changed, &input, [&] {
            if (homeMenuWasOpen == shell.homeMenuOpen()) return;
            homeMenuWasOpen = shell.homeMenuOpen(); input.requireNeutral();
        });
        bindings.connect(&shell,&ShellController::changed,&input,[&]{input.setHoldConfirmEnabled(shell.canHoldConfirm());});
        input.setHoldConfirmEnabled(shell.canHoldConfirm());
        const auto reportBase = parser.isSet("data-dir") ? QDir(parser.value("data-dir")).absolutePath()
            : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        ClassicArt classicArt(parser.isSet("art-dir") ? parser.value("art-dir") : smoke || parser.isSet("ephemeral")
            ? QString() : pokemonModule(shell).manifest().assetRoot(reportBase,"art",QDir(reportBase).filePath("artwork/bootstrap")));
        pokemonModule(shell).pokedex()->configureArtwork(&classicArt);
        shell.hall()->configureArtwork(&classicArt);
        pokemonModule(shell).persona()->picker()->setArtwork(&classicArt);
        DiagnosticsService deviceReports(diagnosticsSmoke ? QDir(parser.value("screenshot-dir")).absoluteFilePath("reports")
                                                         : QDir(reportBase).filePath("diagnostics"));
        shell.diagnostics()->configure(&input, &deviceReports);
        bindings.connect(&input, &ControllerInput::action, &session, [&](Action action) {
            // An exit prompt must never navigate the hidden shell. The platform
            // input provider feeds its own isolated snapshots to the presenter.
            if (adventureLaunch.exitController().phase() != AdventureExitController::Phase::Idle) return;
            if(!session.blocked()&&(!adventureLaunch.active()||adventureLaunch.minimized())&&shell.social()->dispatchIncomingCall(action))return;
            if(launchPresentation.visible()){if(action==Action::Back)launchPresentation.cancel();return;}
            if(lobbyImport.open()){lobbyImport.dispatch(action);return;}
            if(playerOverlay.chatOpen()){if(action==Action::ToggleKeyboard)shell.dispatch(action);else playerOverlay.dispatch(action);return;}
            if (adventureLaunch.active()) {
                if (adventureLaunch.preparing() && action == Action::Back) adventureLaunch.cancel();
                return;
            }
            // Invitations remain in Notifications until deliberately opened.
            session.dispatch(action);
        });
        QString pendingMode;
        bool transitionStarted = false;
        bool softwareRestart=false;
        if(!smoke)bindings.connect(softwareUpdates,&SoftwareUpdateController::restartRequested,&session,[&]{softwareRestart=true;session.requestExit();});
        bindings.connect(&shell, &ShellController::modeRequested, &session, [&](const QString& mode) {
            if (adventureLaunch.active()) return;
            pendingMode = mode; session.requestExit();
        });
        bindings.connect(&session, &SessionState::changed, &session, [&] {
            if (!session.blocked() && !transitionStarted) pendingMode.clear();
            if(!session.blocked() && softwareRestart){softwareRestart=false;softwareUpdates->cancelRestart();}
        });
        bindings.connect(&session, &SessionState::exitReady, &app, [&] {
            if(softwareRestart){app.exit(75);return;}
            if (pendingMode.isEmpty()) { app.exit(); return; }
            if (transitionStarted) return;
            transitionStarted = true;
            platform.switchMode(pendingMode);
        });
        bindings.connect(&platform, &ArmadaPlatformService::transitionFinished, &session, [&](bool success) {
            if (success) { app.exit(); return; }
            transitionStarted = false; pendingMode.clear(); session.cancelPendingExit();
            shell.showNotice("The mode could not be changed. Your journal is saved; TrainerOS remains open.");
        });
        app.installEventFilter(&input);
        if (!smoke) {
            input.setEnabled(app.applicationState() == Qt::ApplicationActive);
            bindings.connect(&app, &QGuiApplication::applicationStateChanged, &input,
                             [&input, &adventureLaunch](Qt::ApplicationState state) {
                input.setEnabled(state == Qt::ApplicationActive && (!adventureLaunch.active() || adventureLaunch.preparing()));
            });
        }
        SpriteArt sprites(parser.isSet("sprite-dir") ? parser.value("sprite-dir") : smoke || parser.isSet("ephemeral")
            ? QString() : pokemonModule(shell).manifest().assetRoot(reportBase,"sprites",QDir(reportBase).filePath("artwork/sprites")));
        QQmlApplicationEngine engine;
        // Built-in adapters import the shared module from compiled resources.
        // The installed executable has no build-directory qmldir beside it.
        engine.addImportPath("qrc:/");
        engine.addImageProvider("sprite-detail", new SpriteImages(sprites));
        pokemonModule(shell).pokedex()->configureSprites(&sprites);
        pokemonModule(shell).party()->configureArtwork(&classicArt, &sprites);
        engine.addImageProvider("exit-frame", new ExitFrameImages(exitPresentation));
        engine.addImageProvider("exit-media", new SavedExitImages(store.get()));
        engine.addImageProvider("screens",new SavedScreenImages(store.get()));
        int qmlWarnings = 0;
        QStringList diagnostics;
        bindings.connect(&engine, &QQmlEngine::warnings, &engine,
                         [&](const QList<QQmlError>& errors) {
            qmlWarnings += errors.size();
            for (const auto& error : errors) diagnostics.append(error.toString());
        });
        engine.rootContext()->setContextProperty("shellController", &shell);
        engine.rootContext()->setContextProperty("controllerInput", &input);
        engine.rootContext()->setContextProperty("sessionState", &session);
        engine.rootContext()->setContextProperty("adventureLaunch", &adventureLaunch);
        engine.rootContext()->setContextProperty("adventureExitPresentation", &exitPresentation);
        engine.rootContext()->setContextProperty("launchPresentation", &launchPresentation);
        engine.rootContext()->setContextProperty("lobbyImport", &lobbyImport);
        engine.rootContext()->setContextProperty("runtimeMultiplayer", &multiplayer);
        engine.addImageProvider("desktop-app",new DesktopAppImages);
        engine.rootContext()->setContextProperty("appsOverlay", &appsOverlay);
        engine.rootContext()->setContextProperty("externalKeyboard", &externalKeyboard);
        engine.rootContext()->setContextProperty("keyboardOverlay", &keyboardOverlay);
        engine.rootContext()->setContextProperty("invitationOverlay", &invitationOverlay);
        engine.rootContext()->setContextProperty("achievementOverlay", &achievementOverlay);
        engine.rootContext()->setContextProperty("radialOverlay", &radialOverlay);
        engine.rootContext()->setContextProperty("achievementCelebration", &achievementCelebration);
        engine.rootContext()->setContextProperty("playerOverlay", &playerOverlay);
        engine.rootContext()->setContextProperty("powerStatus", &powerStatus);
        engine.load(QUrl("qrc:/TrainerOS/Main.qml"));
        if (engine.rootObjects().isEmpty()) {qCritical().noquote()<<"Shell UI did not load:"<<diagnostics.join('\n');result = 2;}
        else {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            if (!window) {qCritical()<<"Shell root is not a window:"<<engine.rootObjects().first()->metaObject()->className();return 2;}
            auto accountBrowser=std::make_shared<bool>(false);
            bindings.connect(shell.backendAccount(),&BackendAccount::returnRequested,window,[&,window,accountBrowser]{
                if(adventureLaunch.active())return;
                if(*accountBrowser&&!shell.apps()->current().isEmpty()){shell.apps()->control(1);return;}
                *accountBrowser=false;
                if(parser.isSet("windowed"))window->show();else window->showFullScreen();window->requestActivate();
            });
            shell.apps()->launchGuard=[&]{
                if(adventureLaunch.active())return QString("Return to your game and close it before opening an application.");
                if(session.blocked()||shell.runtimeChangeBlocked())return QString("Finish the current operation before opening an application.");
                return QString();
            };
            shell.pcGames()->installationGuard=[&]{
                if(adventureLaunch.active()||session.blocked()||shell.runtimeChangeBlocked())return QString("Finish the current game or operation before installing a game.");
                if(!shell.apps()->ready())return QString("Application window controls are unavailable in this session.");
                return QString();
            };
            shell.pcGames()->settingsGuard=[&]{
                return adventureLaunch.active()||session.blocked()||shell.runtimeChangeBlocked()?QString("Close the game and finish the current operation before saving launch settings."):QString();
            };
            bindings.connect(shell.apps(),&AppsController::foregroundRequested,window,[window]{window->hide();});
            shell.backendAccount()->loginGuard=shell.apps()->launchGuard;
            bindings.connect(shell.apps(),&AppsController::shellRequested,window,[&,window,accountBrowser]{
                if(adventureLaunch.active())return;
                if(!*accountBrowser)shell.goToSection("apps");*accountBrowser=false;
                if(parser.isSet("windowed"))window->show();else window->showFullScreen();window->requestActivate();
                  if(pendingSystemAction=="power"){pendingSystemAction.clear();shell.systemAction("power");}
            });
            if(personalLibrary&&!smoke&&platform.dedicatedSession()) {
                const QString helper="/var/opt/traineros/integrations/apps-service.py";
                if(QFileInfo::exists(helper))shell.apps()->start("/usr/bin/python3",helper);
                shell.backendAccount()->setBrowserOpener([&,accountBrowser](QUrl url){
                    *accountBrowser=shell.apps()->openWebUrl(url);return *accountBrowser;
                });
                bindings.connect(shell.apps(),&AppsController::webOpenFailed,window,[&,accountBrowser](QString message){
                    *accountBrowser=false;shell.backendAccount()->browserFailed(message);
                });
            }
            auto* pointerVisibility = new PointerVisibility(*window, platform.dedicatedSession());
            if (auto* exitWindow = window->findChild<QQuickWindow*>("adventure-exit-window"))
                new PointerVisibility(*exitWindow, platform.dedicatedSession());
            bindings.connect(&input, &ControllerInput::observedAction, pointerVisibility,
                             [pointerVisibility](Action, bool fromController) {
                if (fromController) pointerVisibility->hide();
            });
#ifdef Q_OS_LINUX
            bool readyDescriptorValid = false;
            const int readyDescriptor = qEnvironmentVariableIntValue("TRAINEROS_READY_FD", &readyDescriptorValid);
            if (!sessionReadyNotified && (platform.dedicatedSession() || qEnvironmentVariable("TRAINEROS_SOFTWARE_MANAGED")=="1") && readyDescriptorValid && readyDescriptor >= 3) {
                auto notified = std::make_shared<bool>(false);
                bindings.connect(window, &QQuickWindow::frameSwapped, window, [readyDescriptor, notified, &sessionReadyNotified] {
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
                bindings.connect(timer,&QTimer::timeout,window,[beat]{
                    PerformanceTrace::sample("eventLoop.delay",qMax(0.0,beat->nsecsElapsed()/1e6-100));beat->restart();
                });timer->start();
                auto frame=std::make_shared<QElapsedTimer>();
                bindings.connect(window,&QQuickWindow::frameSwapped,window,[frame]{
                    if(frame->isValid())PerformanceTrace::sample("frame.interval",frame->nsecsElapsed()/1e6);
                    frame->restart();
                },Qt::DirectConnection);
                auto* report=new QTimer(window);report->setInterval(1000);
                bindings.connect(report,&QTimer::timeout,window,[&shell]{PerformanceTrace::flush(shell.page());});report->start();
            }
            deviceReports.setWindow(window);
            bindings.connect(shell.powerControl(),&PowerControl::woke,window,[&,window]{
                // Re-arm the existing input filter's neutral gate after resume.
                input.setEnabled(false);
                QTimer::singleShot(250,window,[&,window]{
                    window->requestUpdate();
                    input.setEnabled(app.applicationState()==Qt::ApplicationActive&&!adventureLaunch.active());
                });
            });
            if(!smoke&&platform.dedicatedSession())shell.powerControl()->start();
            session.start();
            if (!parser.isSet("windowed") && !smoke) window->showFullScreen();
            if (personalLibrary && !smoke) {
                auto returnFullscreen = std::make_shared<bool>(false);
                auto returnedAdventure=std::make_shared<QString>();
                const auto requestAdventure = [&, window, returnFullscreen, returnedAdventure](const ProcessCommand& command, const QString& id) -> AdventureResult {
                    if(shell.pcGames()->installing())return {false,"Finish or cancel the game installer before starting a game."};
                    if(adventureLaunch.minimized()) {
                        if(id==adventureLaunch.adventureId() && adventureOverlay){adventureOverlay->returnToGame();return {true,{},true};}
                        return {false,"A game is still running. Return to it or exit it from Game Options before starting another."};
                    }
                    if (pendingLinkSave(QDir(stateDirectory).filePath("backups")))
                        return {false, "Reconnect with your friend to finish the exchange before playing."};
                    if (pokemonModule(shell).party()->activities()->link()->active())
                        return {false, "Leave Together before starting a game."};
                    if (session.blocked() || adventureLaunch.active() || shell.libraryTools()->busy() || softwareUpdates->transitionPending())
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
                    useRetroArchAchievementAccount(launch, realAchievements->launchAccount(),achievementOverlay.ready());
                    return requestAdventure(launch, id);
                };
                melonDs.requestLaunch = requestAdventure;
                azahar.requestLaunch = requestAdventure;
                vita3k.requestLaunch = requestAdventure;
                dolphin.requestLaunch = requestAdventure;
                ppsspp.requestLaunch = requestAdventure;
                armsx2.requestLaunch = requestAdventure;
                cemu.requestLaunch = requestAdventure;
                duckstation.requestLaunch = requestAdventure;
                rpcs3.requestLaunch = requestAdventure;
                eden.requestLaunch = requestAdventure;
                xemu.requestLaunch = requestAdventure;
                cratesRuntime.requestLaunch = requestAdventure;
                pcRuntime.library=&activeLibrary;
                pcRuntime.requestLaunch=requestAdventure;
                pcRuntime.steamEnvironment=[&]{return shell.pcGames()->steam.active()?shell.pcGames()->steam.environment():QJsonObject{};};
                shell.pcGames()->steam.launch=[&](const QString& id)->QString {
                    if(session.blocked()||shell.runtimeChangeBlocked()||adventureLaunch.active())return "Finish the current game or operation first.";
                    const auto r=activeLibrary.registration(id);
                    if(!r||r->removed||r->adventure.adapterId!="pc-local")return "This PC game is not available in the current Trainer profile.";
                    const auto error=pcRuntime.verifyInstallation(*r);if(!error.isEmpty())return error;
                    const auto result=pcRuntime.launch(r->adventure);return result.success?QString():result.message;
                };
                auto prizesAfterGame=std::make_shared<bool>(false);
                bindings.connect(&adventureLaunch,&AdventureLaunchController::adventureFinished,&shell,[&,prizesAfterGame](bool failed){*prizesAfterGame=true;shell.pcGames()->steam.finished(failed);});
                cratesRuntime.ownerId=[&]{return store->ownerId();};
                auto* packages=shell.crates()->installer();
                packages->replacementGuard=[&]{return adventureLaunch.active()||session.blocked()?QString("Waiting for your game or save operation to finish"):QString();};
                auto* crateLibrary=new CratesLibrary(activeLibrary,cratesRuntime.root,&shell);
                bindings.connect(crateLibrary,&CratesLibrary::changed,&shell,[&]{shell.refreshLibrary();shell.apps()->refresh();});
                bindings.connect(crateLibrary,&CratesLibrary::error,&shell,&ShellController::showNotice);
                const auto syncCrateGames=[&,crateLibrary]{
                    if(store->load()&&!adventureLaunch.active())crateLibrary->update(shell.crates()->installer()->installed());
                };
                bindings.connect(&adventureLaunch,&AdventureLaunchController::changed,&shell,[syncCrateGames]{syncCrateGames();});
                bindings.connect(packages,&CratesInstaller::inventoryChanged,&shell,syncCrateGames);
                bindings.connect(store.get(),&LocalStateStore::opened,&shell,[syncCrateGames](bool ready){if(ready)syncCrateGames();});
                packages->configure("/var/opt/traineros/crates/installer.py","/var/opt/traineros/crates/packages.json",cratesRuntime.root);
                shell.crates()->configureCatalogue("/var/opt/traineros/crates/catalogue.py",cratesRuntime.root+"/catalogue");
                bindings.connect(shell.crates(),&CratesController::launchRequested,&shell,[&](QString id,QString kind){
                    if(kind=="game")shell.launchPreparation().launch("crates:"+id);
                    else shell.apps()->openDesktopId("org.traineros.crates."+id+".desktop");
                });
                multiplayer.requestRuntimeLaunch = requestAdventure;
                remotePlay.requestGuestLaunch = requestAdventure;
                bindings.connect(&adventureLaunch, &AdventureLaunchController::changed, &session, [&] {
                    session.setAdventureActive(adventureLaunch.active());
                    session.setAdventureMinimized(adventureLaunch.minimized());
                    QVariantMap live;
                    if(adventureLaunch.active() && !adventureLaunch.preparing())live={{"id",adventureLaunch.adventureId()},{"title",exitPresentation.gameTitle()}};
                    shell.setLiveGame(live);
                    input.setEnabled(app.applicationState() == Qt::ApplicationActive && (!adventureLaunch.active() || adventureLaunch.preparing()));
                });
                bindings.connect(&adventureLaunch, &AdventureLaunchController::checkpointRequested, store.get(),
                                 [&](quint64 token, const QJsonObject& state) {
                    store->saveNavigation(state, &adventureLaunch, [&, token](const QString& error) {
                        adventureLaunch.checkpointCompleted(token, error);
                    });
                });
                bindings.connect(&adventureLaunch, &AdventureLaunchController::suspendRequested, window, [window] { window->hide(); });
                bindings.connect(&shell, &ShellController::changed, &adventureLaunch, [&] {
                    if(adventureLaunch.minimized())adventureLaunch.updateReturnContext(shell.navigationState());
                });
                if(adventureOverlay) {
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::keyboardEvent,&externalKeyboard,[&](const QJsonObject& packet){keyboardEvent(2,packet);});
                    bindings.connect(&multiplayer,&RuntimeMultiplayer::invitationOptionsRequested,adventureOverlay.get(),&AdventureOverlayService::openOptions);
                    bindings.connect(&multiplayer,&RuntimeMultiplayer::liveOptionsRequested,adventureOverlay.get(),[&]{
                        if(shell.runtimeChangeBlocked() || !adventureOverlay->canMinimize()){multiplayer.cancelInvocation();shell.showNotice("Game controls are temporarily unavailable.");return;}
                        adventureOverlay->returnToGame(true);
                    });
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::handoffFailed,&multiplayer,[&](const QString&){multiplayer.cancelInvocation();});
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::shellRequested,window,[&,window,returnFullscreen]{
                        if(*returnFullscreen)window->showFullScreen();else window->show();
                        window->requestActivate();
                        if(!pendingNotification.isEmpty()) {
                            const auto id=std::exchange(pendingNotification,{});
                            shell.dispatch(Action::Home);shell.openNotificationById(id);
                        }
                    });
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::gameRequested,window,[&,window]{
                        adventureLaunch.updateReturnContext(shell.navigationState());window->hide();
                    });
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::contentAction,&shell,[&](Action action){if(shell.social()->dispatchIncomingCall(action))return;if(shell.gameControlsOpen())shell.dispatchGameControls(action);else playerOverlay.dispatch(action);});
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::shellRequested,&shell,[&]{if(!pendingSystemAction.isEmpty()){const auto id=std::exchange(pendingSystemAction,QString());if(id=="current-lobby")shell.openGameHubFace("lobby");else if(id=="pc-settings")shell.gamePerformance()->begin(adventureLaunch.adventureId());else shell.systemAction(id);}});
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::shellAction,&session,[&](Action action){
                        if(adventureLaunch.minimized())session.dispatch(action);
                    });
                    bindings.connect(adventureOverlay.get(),&AdventureOverlayService::handoffFailed,&shell,[&](QString message){pendingNotification.clear();pendingSystemAction.clear();shell.showNotice(message);});
                    bindings.connect(&shell,&ShellController::liveGameRequested,adventureOverlay.get(),[&](bool options){
                        if(shell.runtimeChangeBlocked())return;
                        adventureOverlay->returnToGame(options);
                    });
                }
                bindings.connect(&adventureLaunch, &AdventureLaunchController::restoreRequested, window,
                                 [&, window, returnFullscreen, returnedAdventure,prizesAfterGame](const QJsonObject& state) {
                    shell.restoreNavigation(state);
                    if (!adventureLaunch.error().isEmpty()) shell.showNotice(adventureLaunch.error());
                    else if (playHistory && !playHistory->error().isEmpty()) shell.showNotice(playHistory->error());
                    if (*returnFullscreen) window->showFullScreen(); else window->show();
                    window->requestActivate();
                    input.setEnabled(app.applicationState() == Qt::ApplicationActive);
                    realAchievements->refreshAdventure(*returnedAdventure,true);
                    if(std::exchange(*prizesAfterGame,false))shell.presentPostGamePrizes();
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
                bindings.connect(playHistory.get(), &PlayHistoryController::writeFailed, &shell, [&](const QString& error) {
                    if (!adventureLaunch.active()) shell.showNotice(error);
                });
            }
            if (smoke) {
                const QString screenshotDir = parser.value("screenshot-dir");
                if (!screenshotDir.isEmpty() && !QDir().mkpath(screenshotDir)) return 2;
#ifdef TRAINEROS_UI_TESTS
                if(firstRunSmoke) {
                    startFirstRunSmoke(window,shell,session,*store,input,joystick,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(updatesSmoke) {
                    startSoftwareUpdateSmoke(window,shell,input,joystick,setupTestData->path(),screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(avatarSmoke) {
                    startAvatarSmoke(window,shell,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(platformSmoke) {
                    startPlatformSmoke(window,shell,adventureLaunch,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(playerOverlaySmoke) {
                    PlayerOverlaySmokeScenario::start(window,shell,exitPresentation,adventureLaunch.exitController(),playerOverlay,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(onlineSmoke) {
                    startOnlineSmoke(window,shell,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(lobbySmoke) {
                    LobbySmokeScenario::start(window,shell,input,joystick,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(experienceSmoke) {
                    startExperienceSmoke(window,shell,input,joystick,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(cratesSmoke) {
                    startCratesSmoke(window,shell,input,joystick,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if(appsSmoke) {
                    startAppsSmoke(window,shell,input,joystick,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if (exitSmoke) {
                    startExitSmoke(window, shell, adventureLaunch, exitPresentation, input, joystick,
                                   screenshotDir, smokeCompleted, qmlWarnings, diagnostics);
                } else if(persistencePhase=="library-center") {
                    startCenterSmoke(window,shell,session,*store,input,joystick,stateDirectory,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                } else if (settingsSmoke) {
                    startSettingsSmoke(window,shell,input,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
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
                    if(parser.isSet("achievement-gallery-smoke-test")) startAchievementGallerySmoke(window,shell,input,shellAchievements,joystick,screenshotDir,smokeCompleted,qmlWarnings,diagnostics);
                    else startHallSmoke(window, shell, input, shellArchive, shellAchievements, joystick, screenshotDir,
                                   smokeCompleted, qmlWarnings, diagnostics);
                } else if (artSmoke) {
                    startArtworkSmoke(window, shell, input, joystick, screenshotDir, smokeCompleted, qmlWarnings, diagnostics);
                } else if (pokedexSmoke) {
                    startPokedexSmoke(window, shell, input, dex, joystick, screenshotDir,
                                      smokeCompleted, qmlWarnings, diagnostics);
                } else if (downloadsSmoke) {
                    startDownloadsSmoke(window, shell, input, joystick, screenshotDir, smokeCompleted, qmlWarnings, diagnostics);
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
                bindings.connect(timer, &QTimer::timeout, &app, [&, window, step, failed, savedId, drawerWait, timer, screenshotDir] {
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
                        check(shell.page() == 1 && shell.collectionsRoot() && !shell.drawerOpen() && focusIs("series-card-pokemon"), "R1 opens collections and closes transient layers");
                        press(a);
                        press(SDL_CONTROLLER_BUTTON_DPAD_DOWN); press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT); break;
                    case 7:
                        check(shell.focusIndex() == 1 && focusIs("multiverse-game-1"), "collection game focus");
                        capture("worlds"); press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
                    case 8:
                        check(shell.page() == 2 && focusIs("dex-entry-bulbasaur"), "Pokedex list focus");
                        capture("pokedex"); press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); break;
                    case 9:
                        check(shell.page() == 1 && shell.collectionsRoot() && focusIs("series-card-pokemon"), "collection focus restored");
                        press(a);check(shell.focusIndex()==1,"Game focus restored within the collection");
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); break;
                    case 10:
                        check(shell.page() == 3 && focusIs("trainer-open"), "empty Trainer focus");
                        capture("trainer-empty"); press(a); break;
                    case 11:
                        check(pokemonModule(shell).persona()->editing() && focusIs("trainer-field-0"), "create Trainer starts at Name");
                        capture("trainer-create");
                        taps(down, 3); press(a);
                        check(!pokemonModule(shell).persona()->error().isEmpty() && focusIs("trainer-field-0"), "empty name restores editable field");
                        capture("trainer-validation"); press(a); break;
                    case 12:
                        check(shell.keyboard()->isOpen() && focusIs("key-Q"), "keyboard starts at Q");
                        capture("keyboard-empty");
                        // Enter ERI 2 using physical-style button events only.
                        taps(right, 2); press(a); // E
                        press(right); press(a); // R
                        taps(right, 4); press(a); // I
                        taps(down, 3); taps(left, 2); press(a); // Space
                        taps(right, 4); taps(up, 3); press(a); // 2
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
                        check(pokemonModule(shell).persona()->draftName().isEmpty(), "keyboard buffer must not modify the form before Apply");
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
                        check(pokemonModule(shell).persona()->draftName() == "ERI 2" && !profiles.load(), "Apply changes draft only");
                        press(down); press(a); // leaf
                        press(down); press(a); // Open favorite picker.
                        press(right); taps(down, 2); press(a); // Treecko in the reference list.
                        capture("trainer-edit");
                        press(down); press(a); // Save
                        break;
                    case 18:
                        check(pokemonModule(shell).persona()->exists() && focusIs("trainer-open"), "Save creates Trainer and restores opener");
                        check(pokemonModule(shell).persona()->profile()["name"] == "ERI 2", "saved name");
                        check(pokemonModule(shell).persona()->profile()["emblem"] == "leaf", "saved emblem");
                        check(pokemonModule(shell).persona()->profile()["favorite"] == "Treecko", "saved favorite");
                        *savedId = pokemonModule(shell).persona()->profile()["id"].toString();
                        check(!savedId->isEmpty() && shell.home()["trainer"] == "ERI 2", "stable identity and Home projection");
                        capture("trainer-profile"); press(a); break;
                    case 19:
                        press(a); press(a); // Edit Name, append Q.
                        check(shell.keyboard()->text() == "ERI 2Q", "editing starts from saved name");
                        press(b);
                        check(pokemonModule(shell).persona()->draftName() == "ERI 2" && focusIs("trainer-field-0"), "B discards only keyboard buffer");
                        press(down); press(a); // Unsaved emblem.
                        press(b);
                        check(pokemonModule(shell).persona()->profile()["emblem"] == "leaf", "B discards form changes");
                        press(a); press(a); // Reopen name for numeric tour.
                        break;
                    case 20:
                        // Reach and enter all ten numeric keys through the separate block.
                        taps(right, 10); press(a); press(right); press(a); press(right); press(a);
                        press(down); press(a); press(left); press(a); press(left); press(a);
                        press(down); press(a); press(right); press(a); press(right); press(a);
                        press(down); press(a);
                        check(shell.keyboard()->text() == "ERI 21236547890" && focusIs("key-0"), "all numeric keys reachable");
                        taps(left, 3); press(a); // Delete
                        check(shell.keyboard()->text() == "ERI 2123654789", "Delete removes last character");
                        press(right); press(a); // Clear
                        check(shell.keyboard()->text().isEmpty(), "Clear empties buffer");
                        taps(left, 2); press(a); // Space
                        check(shell.keyboard()->text() == " ", "Space reachable from numeric block");
                        press(b); press(b); // Discard buffer and form.
                        check(pokemonModule(shell).persona()->profile()["name"] == "ERI 2", "cancel numeric tour preserves profile");
                        press(a); press(a); press(a);
                        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); // Global page switch cancels both drafts.
                        break;
                    case 21:
                        check(shell.page() == 4 && focusIs("social-empty"), "R1 escapes keyboard to Social");
                        check(!shell.keyboard()->isOpen() && !pokemonModule(shell).persona()->editing(), "page switch closes both drafts");
                        check(pokemonModule(shell).persona()->profile()["id"] == *savedId && pokemonModule(shell).persona()->profile()["name"] == "ERI 2", "switch preserves saved identity and name");
                        press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); break;
                    case 22:
                        check(focusIs("trainer-open"), "L1 restores Trainer opener");
                        // Save another edit and ensure identity survives.
                        press(a); press(down); press(a); taps(down, 2); press(a);
                        check(pokemonModule(shell).persona()->profile()["id"] == *savedId && pokemonModule(shell).persona()->profile()["emblem"] == "spark", "editing preserves identity");
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
                        check(shell.socialFace()=="communities", "Physical R2 cycles Social peers");
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
                        check(shell.keyboard()->text() == "qQ!", "Controller case and symbol entry");
                        break;
                    case 37:
                        check(focusIs("key-Q"), "Changing key layout preserves visible focus");
                        if (auto* field = window->findChild<QObject*>("keyboard-text"))
                            check(field->property("text").toString() == QString(3, QChar(0x2022)) + QChar(0x2502), "Rendered password contains only bullets and cursor");
                        else check(false, "Password display exists");
                        capture("keyboard-secret-symbols");
                        press(SDL_CONTROLLER_BUTTON_X); break;
                    case 38:
                        capture("keyboard-symbols-second-page");
                        taps(right, 6); press(a);
                        check(shell.keyboard()->text() == "qQ!1", "Numeric block remains reachable past hidden symbol slots");
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
            // Runtime-service destruction can publish Lobby/Home changes. End
            // shell bindings before the stack data captured by them is released.
            bindings.clear();
            shell.prepareShutdown();
            if (smoke && !smokeCompleted) result = 2; // Early window/app exit is not a passing smoke test.
        }
        bindings.clear();
        shell.prepareShutdown();
    } while (restartTrainer);
    if (joystick) SDL_JoystickClose(joystick);
    if (virtualIndex >= 0) SDL_JoystickDetachVirtual(virtualIndex);
    if (smoke) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    return result;
}
