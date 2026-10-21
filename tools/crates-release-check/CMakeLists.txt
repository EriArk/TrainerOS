cmake_minimum_required(VERSION 3.24)
project(CratesReleaseCheck LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Test Quick Qml)
get_filename_component(ROOT "../.." ABSOLUTE)
add_library(crates_check_host STATIC
    ${ROOT}/src/features/crates/CratesController.cpp
    ${ROOT}/src/features/crates/CratesCatalogue.cpp
    ${ROOT}/src/features/crates/CratesInstaller.cpp
    ${ROOT}/src/features/device/ControlsDashboard.cpp
    ${ROOT}/src/platform/sdk/PlatformHost.cpp
    ${ROOT}/src/platform/sdk/SdkNetwork.cpp
    ${ROOT}/src/features/social/LobbyCoordinator.cpp
    ${ROOT}/src/features/social/LobbyRoomCoordinator.cpp
    ${ROOT}/src/features/social/LobbySession.cpp
    ${ROOT}/src/features/social/LobbyRecovery.cpp
    ${ROOT}/src/features/social/LobbyRuntime.cpp
    ${ROOT}/src/features/social/LobbyInput.cpp)
target_include_directories(crates_check_host PUBLIC ${ROOT}/src)
target_link_libraries(crates_check_host PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
add_executable(crates_release_check main.cpp)
target_link_libraries(crates_release_check PRIVATE crates_check_host Qt6::Test)
add_executable(crates_controller_tests ${ROOT}/tests/CratesTests.cpp)
target_link_libraries(crates_controller_tests PRIVATE crates_check_host Qt6::Test)
foreach(target crates_release_check crates_controller_tests)
    qt_add_resources(${target} crates_data PREFIX /crates BASE ${ROOT}/data/crates
        FILES ${ROOT}/data/crates/showcase.json ${ROOT}/data/crates/catalogue.json)
endforeach()
add_executable(crates_preview preview.cpp)
target_link_libraries(crates_preview PRIVATE crates_check_host Qt6::Quick Qt6::Qml)
qt_add_resources(crates_preview preview_entry PREFIX /preview FILES Preview.qml)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/qmldir "singleton Theme 1.0 Theme.qml\n")
qt_add_resources(crates_preview preview_directory PREFIX /preview BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${CMAKE_CURRENT_BINARY_DIR}/qmldir)
file(GLOB qml ${ROOT}/src/qml/*.qml ${ROOT}/src/qml/*.js)
qt_add_resources(crates_preview shell_qml PREFIX /preview BASE ${ROOT}/src/qml FILES ${qml})
qt_add_resources(crates_preview crates_data PREFIX /crates BASE ${ROOT}/data/crates
    FILES ${ROOT}/data/crates/showcase.json ${ROOT}/data/crates/catalogue.json)
qt_add_resources(crates_preview fonts PREFIX /fonts BASE ${ROOT}/assets/fonts
    FILES ${ROOT}/assets/fonts/Fredoka.ttf ${ROOT}/assets/fonts/ChakraPetch-Bold.ttf ${ROOT}/assets/fonts/Bungee-Regular.ttf)
file(GLOB_RECURSE crate_art ${ROOT}/assets/crates/*.png ${ROOT}/assets/crates/*.svg)
qt_add_resources(crates_preview crate_art PREFIX /crates BASE ${ROOT}/assets/crates FILES ${crate_art})
file(GLOB icons ${ROOT}/assets/icons/phosphor/*.svg)
qt_add_resources(crates_preview icons PREFIX /social-icons BASE ${ROOT}/assets/icons/phosphor FILES ${icons})
