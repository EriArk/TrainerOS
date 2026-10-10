cmake_minimum_required(VERSION 3.21)
project(TrainerRuntimeProfiles LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network)
add_library(trainer_runtime_profiles STATIC
    src/integrations/adventure/standalone/PpssppNetplay.cpp
    src/integrations/adventure/retroarch/RetroArchNetplay.cpp
    src/integrations/adventure/retroarch/RetroArchHandheldLink.cpp
    src/integrations/adventure/retroarch/LinkedSavePreparation.cpp
    src/integrations/adventure/retroarch/RetroArchSaveTarget.cpp
    src/integrations/adventure/retroarch/RetroArchDisc.cpp
    src/integrations/adventure/retroarch/RetroArchConfiguration.cpp)
target_include_directories(trainer_runtime_profiles PUBLIC src)
target_link_libraries(trainer_runtime_profiles PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
add_executable(inspect_psp examples/inspect_psp.cpp)
target_link_libraries(inspect_psp PRIVATE trainer_runtime_profiles)
