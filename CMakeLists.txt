cmake_minimum_required(VERSION 3.24)
project(TrainerOS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
include(CTest)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Sql Network Xml Multimedia)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
find_package(SDL2 2.0.14 REQUIRED CONFIG)

add_library(trainer_core STATIC
    src/integrations/scraper/ScreenScraper.cpp
    src/integrations/scraper/BatoceraScrapeWriter.cpp
    src/core/storage/SqliteLibraryEdits.cpp
    src/core/storage/LibraryFileMove.cpp
    src/features/library/LibraryToolsController.cpp
    src/core/repository/BatoceraLibrary.cpp
    src/core/repository/BatoceraLibrary.h
    src/core/storage/TrainerPin.cpp
    src/features/trainer/TrainerAccessController.cpp
    src/features/pokedex/SpriteArt.h
    src/features/pokedex/SpriteArt.cpp
    src/features/pokedex/ClassicArt.h
    src/features/pokedex/ClassicArt.cpp
    src/core/model/ExitMedia.h
    src/features/home/ExitImage.h
    src/features/home/ExitImage.cpp
    src/core/storage/SqliteExitMedia.h
    src/core/storage/SqliteOwnership.h
    src/core/storage/SqliteOwnership.cpp
    src/integrations/achievements/TrainerAchievementProvider.h
    src/core/storage/SqliteExitMedia.cpp
    src/platform/input/AdventureOverlayService.h
    src/platform/input/AdventureOverlayService.cpp
    src/features/adventure/AdventureExitPresentation.h
    src/features/adventure/AdventureExitPresentation.cpp
    src/features/home/BadgeAssets.h
    src/features/home/BadgeAssets.cpp
    src/core/model/GameProgress.h
    src/core/model/GameProgressProvider.h
    src/integrations/progress/Gen3Progress.h
    src/integrations/progress/Gen3Progress.cpp
    src/integrations/progress/EmeraldShops.cpp
    src/features/center/SaveCenterShops.cpp
    src/integrations/progress/EmeraldParty.h
    src/integrations/progress/EmeraldParty.cpp
    src/integrations/progress/GameProgressService.h
    src/integrations/progress/GameProgressService.cpp
    src/features/halloffame/AchievementAccountController.h
    src/features/halloffame/AchievementAccountController.cpp
    src/integrations/achievements/RetroAchievementsApi.h
    src/integrations/achievements/RetroAchievementsApi.cpp
    src/integrations/achievements/RetroAchievementsProvider.h
    src/integrations/achievements/RetroAchievementsProvider.cpp
    src/integrations/adventure/standalone/MelonDsSave.h
    src/integrations/adventure/standalone/MelonDsSave.cpp
    src/integrations/adventure/AdapterRouter.h
    src/integrations/adventure/standalone/StandaloneAdapter.h
    src/integrations/adventure/standalone/StandaloneAdapter.cpp
    src/platform/device/DeviceService.h
    src/platform/device/DeviceService.cpp
    src/platform/device/VolumeKeys.cpp
    src/features/device/DeviceController.h
    src/features/device/DeviceController.cpp
    src/platform/ArmadaPlatformService.h
    src/platform/ArmadaPlatformService.cpp
    src/platform/power/PowerStatus.h
    src/platform/power/PowerStatus.cpp
    src/core/model/SaveBackup.h
    src/platform/storage/SaveBackupStorage.h
    src/platform/storage/SaveBackupStorage.cpp
    src/features/center/SaveCenterController.h
    src/features/center/SaveCenterController.cpp
    src/core/repository/ResumeLibraryRepository.h
    src/integrations/adventure/retroarch/RetroArchConfiguration.h
    src/integrations/adventure/retroarch/RetroArchConfiguration.cpp
    src/integrations/adventure/retroarch/RetroArchDisc.h
    src/integrations/adventure/retroarch/RetroArchDisc.cpp
    src/integrations/adventure/retroarch/RetroArchSave.h
    src/integrations/adventure/retroarch/RetroArchSave.cpp
    src/core/repository/PlayHistoryRepository.h
    src/core/storage/SqliteHallOfFame.cpp
    src/core/storage/SqliteHallOfFame.h
    src/core/storage/SqlitePokedexJournal.cpp
    src/core/storage/SqlitePokedexJournal.h
    src/core/repository/HallOfFameRepository.cpp
    src/core/storage/SqlitePlayHistory.cpp
    src/core/storage/SqlitePlayHistory.h
    src/features/home/PlayHistoryController.cpp
    src/features/home/PlayHistoryController.h
    src/core/navigation/ResumePresentation.h
    src/core/repository/CollectionRepository.cpp
    src/core/repository/CollectionRepository.h
    src/integrations/adventure/retroarch/RetroArchAdapter.cpp
    src/integrations/adventure/retroarch/RetroArchAdapter.h
    src/platform/diagnostics/DiagnosticsService.cpp
    src/platform/diagnostics/DiagnosticsService.h
    src/features/diagnostics/DiagnosticsController.cpp
    src/features/diagnostics/DiagnosticsController.h
    src/platform/process/ProcessService.cpp
    src/platform/process/ProcessService.h
    src/core/navigation/AdventureLaunchController.cpp
    src/core/navigation/AdventureLaunchController.h
    src/core/navigation/AdventureExitController.cpp
    src/core/navigation/AdventureExitController.h
    src/platform/storage/FileCatalog.cpp
    src/platform/storage/FileCatalog.h
    src/features/library/FilePickerController.cpp
    src/features/library/FilePickerController.h
    src/features/library/LibraryManagementController.cpp
    src/features/library/LibraryManagementController.h
    src/features/settings/SettingsController.cpp
    src/features/settings/SettingsController.h
    src/core/storage/SqliteLibrary.cpp
    src/core/storage/SqliteLibrary.h
    src/core/repository/PreferencesRepository.h
    src/core/storage/LocalStateStore.cpp
    src/core/storage/LocalStateStore.h
    src/core/storage/SessionState.cpp
    src/core/storage/SessionState.h
    src/core/input/ControllerInput.cpp
    src/core/input/ControllerInput.h
    src/core/input/PointerVisibility.cpp
    src/core/input/PointerVisibility.h
    src/core/input/Action.h
    src/core/input/TextEntryController.cpp
    src/core/input/TextEntryController.h
    src/core/navigation/ShellController.cpp
    src/core/navigation/ShellController.h
    src/core/model/Models.h
    src/core/repository/HallOfFameRepository.h
    src/core/repository/MockHallOfFameRepository.cpp
    src/integrations/achievements/AchievementProvider.h
    src/integrations/achievements/MockAchievementProvider.cpp
    src/features/halloffame/HallOfFameController.h
    src/features/halloffame/ArchiveEditor.h
    src/features/halloffame/ArchiveEditor.cpp
    src/features/halloffame/HallOfFameController.cpp
    src/core/repository/MockLibraryRepository.cpp
    src/core/repository/LibraryRepository.h
    src/core/repository/TrainerRepository.h
    src/core/repository/MockTrainerRepository.cpp
    src/core/repository/PokedexRepository.h
    src/core/repository/PokedexRepository.cpp
    src/core/repository/OfflinePokedex.h
    src/core/repository/OfflinePokedex.cpp
    src/core/repository/MockPokedexRepository.cpp
    src/features/pokedex/PokedexController.h
    src/features/pokedex/PokedexController.cpp
    src/features/pokedex/SpeciesPicker.h
    src/features/pokedex/SpeciesPicker.cpp
    src/features/trainer/TrainerOverview.h
    src/features/trainer/TrainerOverview.cpp
    src/features/trainer/TrainerSetupPresentation.cpp
    src/features/trainer/TrainerController.cpp
    src/features/trainer/TrainerController.h
    src/features/worlds/WorldsController.cpp
    src/features/worlds/MultiversePresentation.cpp
    src/features/center/PartyPresentation.cpp
    src/features/center/CenterActivities.cpp
    src/features/worlds/WorldsController.h
    src/integrations/adventure/AdventureAdapter.h
    src/integrations/adventure/mock/MockAdventureAdapter.cpp
    src/integrations/adventure/mock/MockAdventureAdapter.h
    src/platform/PlatformService.h
)
target_include_directories(trainer_core PUBLIC src)
file(GLOB badge_images CONFIGURE_DEPENDS assets/badges/*.png)
qt_add_resources(trainer_core badge_assets PREFIX "/badges" BASE assets/badges
    FILES assets/badges/manifest.json ${badge_images})
qt_add_resources(trainer_core collection PREFIX "/collection" BASE data FILES data/catalogue.json)
qt_add_resources(trainer_core pokedex_reference PREFIX "/pokedex" BASE data FILES data/pokedex.json)
qt_add_resources(trainer_core emerald_reference PREFIX "/progress" BASE data FILES data/emerald-reference.json)
qt_add_resources(trainer_core emerald_shops PREFIX "/progress" BASE data FILES data/emerald-shops.json)
target_link_libraries(trainer_core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql Qt6::Network Qt6::Xml SDL2::SDL2 OpenSSL::Crypto)
target_compile_definitions(trainer_core PUBLIC SDL_MAIN_HANDLED)
if(MSVC)
    target_compile_options(trainer_core PRIVATE /W4 PUBLIC /utf-8)
else()
    target_compile_options(trainer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(traineros src/app/Main.cpp)
qt_add_resources(traineros display_fonts PREFIX "/fonts" BASE assets/fonts
    FILES assets/fonts/Fredoka.ttf assets/fonts/OFL.txt
          assets/fonts/ChakraPetch-Bold.ttf assets/fonts/ChakraPetch-OFL.txt
          assets/fonts/Bungee-Regular.ttf assets/fonts/Bungee-OFL.txt)
target_link_libraries(traineros PRIVATE trainer_core Qt6::Quick Qt6::Multimedia)
set_source_files_properties(src/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
file(GLOB trainer_qml_sources CONFIGURE_DEPENDS src/qml/*.qml src/qml/*.js)
foreach(qml_file IN LISTS trainer_qml_sources)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
qt_add_qml_module(traineros
    URI TrainerOS
    VERSION 1.0
    RESOURCE_PREFIX /
    QML_FILES
        src/qml/AdventureArtwork.qml
        src/qml/LibraryWheel.qml
        src/qml/GameInformation.qml
        src/qml/GamePreview.qml
        src/qml/GameVideoPreview.qml
        src/qml/TrainerAccessPanel.qml
        src/qml/Main.qml
        src/qml/AdventureExitWindow.qml
        src/qml/Theme.qml
        src/qml/Panel.qml
        src/qml/ChassisFrame.qml
        src/qml/ChassisTopRim.qml
        src/qml/PageHeader.qml
        src/qml/ShellBackgroundPattern.qml
        src/qml/MountedPanel.qml
        src/qml/CapButton.qml
        src/qml/PlatformBadge.qml
        src/qml/Hint.qml
        src/qml/HomePage.qml
        src/qml/LeagueBadge.qml
        src/qml/BadgeTray.qml
        src/qml/AdventureButton.qml
        src/qml/HallOfFamePage.qml
        src/qml/JourneyPanel.qml
        src/qml/ArchiveEditorPanel.qml
        src/qml/AchievementAccountPanel.qml
        src/qml/ControllerList.qml
        src/qml/ContinueDrawer.qml
        src/qml/SystemPanel.qml
        src/qml/StoragePanel.qml
        src/qml/LaunchPanel.qml
        src/qml/LibraryToolsPanel.qml
        src/qml/LibraryPanel.qml
        src/qml/TrainerSettingsPanel.qml
        src/qml/TrainerSetupPanel.qml
        src/qml/SettingControl.qml
        src/qml/SettingsPanel.qml
        src/qml/DevicePanel.qml
        src/qml/DiagnosticsPanel.qml
        src/qml/KeyboardPanel.qml
        src/qml/TrainerPage.qml
        src/qml/TrainerEmblem.qml
        src/qml/WorldsPage.qml
        src/qml/WorldCard.qml
        src/qml/WorldThemes.js
        src/qml/PlatformCard.qml
        src/qml/PlatformThemes.js
        src/qml/MultiversePage.qml
        src/qml/MultiverseHome.qml
        src/qml/SystemGlyph.qml
        src/qml/PokedexPage.qml
        src/qml/SpeciesPickerPanel.qml
        src/qml/ClassicIllustration.qml
        src/qml/ArtworkPanel.qml
        src/qml/SpritePreview.qml
        src/qml/SpriteActor.qml
        src/qml/SaveCenterPanel.qml
        src/qml/PartyPanel.qml
        src/qml/PokemonClinic.qml
        src/qml/PokemonShop.qml
        src/qml/CenterActivitiesPanel.qml
        src/qml/HomeParty.qml
        src/qml/PartyPlayroom.qml
        src/qml/PlayroomMeadow.qml
        src/qml/PlayroomActor.qml
        src/qml/BatteryGauge.qml
)

if(BUILD_TESTING)
    qt_add_executable(trainer_video_tests tests/VideoPreviewTests.cpp)
    target_compile_definitions(trainer_video_tests PRIVATE TRAINER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(trainer_video_tests PRIVATE Qt6::Quick Qt6::Multimedia Qt6::Test)
    add_test(NAME video_preview COMMAND trainer_video_tests)
    set_tests_properties(video_preview PROPERTIES TIMEOUT 45)
    qt_add_executable(trainer_scraper_tests tests/ScreenScraperTests.cpp)
    target_link_libraries(trainer_scraper_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME screenscraper COMMAND trainer_scraper_tests)
    qt_add_executable(trainer_pointer_tests tests/PointerVisibilityTests.cpp)
    target_link_libraries(trainer_pointer_tests PRIVATE trainer_core Qt6::Quick Qt6::Test)
    add_test(NAME pointer_visibility COMMAND trainer_pointer_tests)
    qt_add_executable(trainer_progress_tests tests/GameProgressTests.cpp)
    target_link_libraries(trainer_progress_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME game_progress COMMAND trainer_progress_tests)
    qt_add_executable(trainer_achievement_tests tests/RetroAchievementsTests.cpp)
    target_link_libraries(trainer_achievement_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME achievements COMMAND trainer_achievement_tests)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME session_supervisor COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/SessionSupervisorTests.py)
        add_test(NAME overlay_helper COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/OverlayHelperTests.py)
        add_test(NAME legacy_retirement COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/LegacyRetirementTests.py)
        set_tests_properties(overlay_helper PROPERTIES TIMEOUT 15)
        set_tests_properties(session_supervisor PROPERTIES TIMEOUT 40)
    endif()
    qt_add_executable(trainer_power_tests tests/PowerStatusTests.cpp)
    qt_add_executable(trainer_device_tests tests/DeviceTests.cpp)
    qt_add_executable(trainer_standalone_tests tests/StandaloneTests.cpp)
    target_link_libraries(trainer_standalone_tests PRIVATE trainer_core Qt6::Test)
    add_dependencies(trainer_standalone_tests trainer_process_probe)
    add_test(NAME standalone COMMAND trainer_standalone_tests)
    target_link_libraries(trainer_device_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME device COMMAND trainer_device_tests)
    target_link_libraries(trainer_power_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME power COMMAND trainer_power_tests)
    qt_add_executable(trainer_save_backup_tests tests/SaveBackupTests.cpp)
    target_link_libraries(trainer_save_backup_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME save_backups COMMAND trainer_save_backup_tests)
    qt_add_executable(trainer_overview_tests tests/TrainerOverviewTests.cpp)
    target_link_libraries(trainer_overview_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME trainer_overview COMMAND trainer_overview_tests)
    qt_add_executable(trainer_offline_pokedex_tests tests/OfflinePokedexTests.cpp)
    target_link_libraries(trainer_offline_pokedex_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME offline_pokedex COMMAND trainer_offline_pokedex_tests)
    qt_add_executable(trainer_sprite_art_tests tests/SpriteArtTests.cpp)
    target_link_libraries(trainer_sprite_art_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME sprite_art COMMAND trainer_sprite_art_tests)
    qt_add_executable(trainer_classic_art_tests tests/ClassicArtTests.cpp)
    target_link_libraries(trainer_classic_art_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME classic_art COMMAND trainer_classic_art_tests)
    qt_add_executable(trainer_archive_tests tests/ArchiveTests.cpp)
    target_link_libraries(trainer_archive_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME archive COMMAND trainer_archive_tests)
    qt_add_executable(trainer_retroarch_resume_tests tests/RetroArchResumeTests.cpp)
    target_sources(trainer_retroarch_resume_tests PRIVATE src/integrations/adventure/retroarch/RetroArchResume.cpp src/integrations/adventure/retroarch/RetroArchResume.h)
    target_link_libraries(trainer_retroarch_resume_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME retroarch_resume COMMAND trainer_retroarch_resume_tests)
    qt_add_executable(trainer_process_probe tests/ProcessProbe.cpp)
    target_link_libraries(trainer_process_probe PRIVATE Qt6::Core)
    add_dependencies(trainer_retroarch_resume_tests trainer_process_probe)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(trainer_resume_tests tests/ResumeTests.cpp)
    target_link_libraries(trainer_resume_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME resume COMMAND trainer_resume_tests)
    qt_add_executable(trainer_history_tests tests/PlayHistoryTests.cpp)
    target_link_libraries(trainer_history_tests PRIVATE trainer_core Qt6::Test)
    add_dependencies(trainer_history_tests trainer_process_probe)
    add_test(NAME history COMMAND trainer_history_tests)
    qt_add_executable(trainer_batocera_tests tests/BatoceraTests.cpp)
    target_link_libraries(trainer_batocera_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME batocera COMMAND trainer_batocera_tests)
    qt_add_executable(trainer_batocera_import tools/ImportBatocera.cpp)
    target_link_libraries(trainer_batocera_import PRIVATE trainer_core)
    qt_add_executable(trainer_collection_tests tests/CollectionTests.cpp)
    target_link_libraries(trainer_collection_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME collection COMMAND trainer_collection_tests)
    qt_add_executable(trainer_retroarch_tests tests/RetroArchTests.cpp)
    target_link_libraries(trainer_retroarch_tests PRIVATE trainer_core Qt6::Test)
    add_dependencies(trainer_retroarch_tests trainer_process_probe)
    add_test(NAME retroarch COMMAND trainer_retroarch_tests)
    target_sources(traineros PRIVATE tests/ui/WorldsSmokeScenario.cpp tests/ui/WorldsSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/PokedexSmokeScenario.cpp tests/ui/PokedexSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/ArtworkSmokeScenario.cpp tests/ui/ArtworkSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/HallSmokeScenario.cpp tests/ui/HallSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/PersistenceSmokeScenario.cpp tests/ui/PersistenceSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/LibrarySmokeScenario.cpp tests/ui/LibrarySmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/LaunchSmokeScenario.cpp tests/ui/LaunchSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/HomeSmokeScenario.cpp tests/ui/HomeSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/DiagnosticsSmokeScenario.cpp tests/ui/DiagnosticsSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/CenterSmokeScenario.cpp tests/ui/CenterSmokeScenario.h)
    target_sources(traineros PRIVATE tests/ui/ExitSmokeScenario.cpp tests/ui/ExitSmokeScenario.h)
    add_dependencies(traineros trainer_process_probe)
    target_include_directories(traineros PRIVATE tests/ui)
    # Only the entry point branches on this macro; keep test entry flags scoped
    # to that file instead of defining them for every generated QML unit.
    set_source_files_properties(src/app/Main.cpp PROPERTIES COMPILE_DEFINITIONS TRAINEROS_UI_TESTS)
    qt_add_executable(trainer_core_tests tests/CoreTests.cpp)
    target_link_libraries(trainer_core_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME core COMMAND trainer_core_tests)
    qt_add_executable(trainer_interaction_tests tests/InteractionTests.cpp)
    target_link_libraries(trainer_interaction_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME interactions COMMAND trainer_interaction_tests)
    qt_add_executable(trainer_worlds_tests tests/WorldsTests.cpp)
    target_link_libraries(trainer_worlds_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME worlds COMMAND trainer_worlds_tests)
    qt_add_executable(trainer_pokedex_tests tests/PokedexTests.cpp)
    target_link_libraries(trainer_pokedex_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME pokedex COMMAND trainer_pokedex_tests)
    qt_add_executable(trainer_hall_tests tests/HallOfFameTests.cpp)
    target_link_libraries(trainer_hall_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME hall COMMAND trainer_hall_tests)
    qt_add_executable(trainer_storage_tests tests/StorageTests.cpp)
    target_link_libraries(trainer_storage_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME storage COMMAND trainer_storage_tests)
    qt_add_executable(trainer_ownership_tests tests/OwnershipTests.cpp)
    target_link_libraries(trainer_ownership_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME ownership COMMAND trainer_ownership_tests)
    qt_add_executable(trainer_access_tests tests/TrainerAccessTests.cpp)
    target_link_libraries(trainer_access_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME trainer_access COMMAND trainer_access_tests)
    qt_add_executable(trainer_profiles_tests tests/TrainerProfilesTests.cpp)
    target_link_libraries(trainer_profiles_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME trainer_profiles COMMAND trainer_profiles_tests)
    qt_add_executable(trainer_store_migration_probe tests/StoreMigrationProbe.cpp)
    target_link_libraries(trainer_store_migration_probe PRIVATE trainer_core)
    qt_add_executable(trainer_library_import tools/ImportMultiverse.cpp)
    target_link_libraries(trainer_library_import PRIVATE trainer_core)
    qt_add_executable(trainer_library_tests tests/LibraryTests.cpp)
    add_dependencies(trainer_library_tests trainer_library_import)
    target_link_libraries(trainer_library_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME library COMMAND trainer_library_tests)
    qt_add_executable(trainer_process_tests tests/ProcessTests.cpp)
    target_link_libraries(trainer_process_tests PRIVATE trainer_core Qt6::Test)
    add_dependencies(trainer_process_tests trainer_process_probe)
    add_test(NAME process COMMAND trainer_process_tests)
    qt_add_executable(trainer_exit_tests tests/AdventureExitTests.cpp)
    target_link_libraries(trainer_exit_tests PRIVATE trainer_core Qt6::Test)
    add_dependencies(trainer_exit_tests trainer_process_probe)
    add_test(NAME adventure_exit COMMAND trainer_exit_tests)
    qt_add_executable(trainer_exit_presentation_tests tests/AdventureExitPresentationTests.cpp)
    target_link_libraries(trainer_exit_presentation_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME adventure_exit_presentation COMMAND trainer_exit_presentation_tests)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        qt_add_executable(trainer_overlay_tests tests/AdventureOverlayServiceTests.cpp)
        target_link_libraries(trainer_overlay_tests PRIVATE trainer_core Qt6::Test)
        add_dependencies(trainer_overlay_tests trainer_process_probe)
        add_test(NAME adventure_overlay_transport COMMAND trainer_overlay_tests)
        set_tests_properties(adventure_overlay_transport PROPERTIES TIMEOUT 20)
    endif()
    set_tests_properties(adventure_exit PROPERTIES TIMEOUT 30)
    qt_add_executable(trainer_diagnostics_tests tests/DiagnosticsTests.cpp)
    target_link_libraries(trainer_diagnostics_tests PRIVATE trainer_core Qt6::Test)
    add_test(NAME diagnostics COMMAND trainer_diagnostics_tests)
    qt_add_executable(trainer_persistence_process_tests tests/PersistenceProcessTests.cpp)
    target_link_libraries(trainer_persistence_process_tests PRIVATE Qt6::Test)
    add_dependencies(trainer_persistence_process_tests traineros)
    add_test(NAME persistence_process COMMAND trainer_persistence_process_tests)
    add_test(NAME qml_smoke COMMAND traineros --smoke-test --screenshot-dir "${CMAKE_CURRENT_BINARY_DIR}/screenshots")
    add_test(NAME worlds_qml_smoke COMMAND traineros --worlds-smoke-test --screenshot-dir "${CMAKE_CURRENT_BINARY_DIR}/screenshots/worlds")
    add_test(NAME pokedex_qml_smoke COMMAND traineros --pokedex-smoke-test --screenshot-dir "${CMAKE_CURRENT_BINARY_DIR}/screenshots/pokedex")
    add_test(NAME artwork_qml_smoke COMMAND traineros --art-smoke-test --screenshot-dir "${CMAKE_CURRENT_BINARY_DIR}/screenshots/artwork")
    add_test(NAME hall_qml_smoke COMMAND traineros --hall-smoke-test --screenshot-dir "${CMAKE_CURRENT_BINARY_DIR}/screenshots/hall")
    add_test(NAME diagnostics_qml_smoke COMMAND traineros --diagnostics-smoke-test --screenshot-dir "${CMAKE_CURRENT_BINARY_DIR}/screenshots/diagnostics")
    add_test(NAME exit_qml_smoke COMMAND traineros --exit-smoke-test --screenshot-dir "${CMAKE_CURRENT_BINARY_DIR}/screenshots/exit")
    set_tests_properties(exit_qml_smoke pointer_visibility retroarch core interactions worlds pokedex hall storage library process diagnostics persistence_process qml_smoke worlds_qml_smoke pokedex_qml_smoke hall_qml_smoke diagnostics_qml_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 30
    )
    # This aggregates multiple complete application launches, including the
    # paired Center/controller/restore scenario. Individual child deadlines stay.
    set_tests_properties(persistence_process PROPERTIES TIMEOUT 60)
    # 61 controller stages already spend 28 seconds in deliberate settling waits,
    # before image captures and software rendering; retain bounded headroom.
    set_tests_properties(worlds_qml_smoke PROPERTIES TIMEOUT 60)
    set_tests_properties(artwork_qml_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 60)
endif()

include(GNUInstallDirs)
install(TARGETS traineros RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/badges/README.md assets/badges/manifest.json DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/traineros/badges)
install(FILES data/licenses/PokeAPI.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/traineros/licenses)
if(UNIX AND NOT APPLE)
    install(PROGRAMS packaging/integrations/controller-bridge.py DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/traineros)
    configure_file(packaging/linux/org.traineros.TrainerOS.desktop.in
                   org.traineros.TrainerOS.desktop @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.traineros.TrainerOS.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
# Session installation intentionally waits for validation on the ArmadaOS device.
