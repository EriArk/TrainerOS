cmake_minimum_required(VERSION 3.21)
project(TrainerGameAdapter LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
add_library(trainer_game_adapter STATIC
    src/integrations/progress/Gen3Progress.cpp
    src/integrations/progress/EmeraldParty.cpp
    src/integrations/progress/EmeraldShops.cpp)
target_include_directories(trainer_game_adapter PUBLIC src)
target_link_libraries(trainer_game_adapter PUBLIC Qt6::Core)
qt_add_resources(trainer_game_adapter emerald_reference PREFIX /progress BASE data
    FILES data/emerald-reference.json)
qt_add_resources(trainer_game_adapter emerald_shops PREFIX /progress BASE data
    FILES data/emerald-shops.json)
add_executable(inspect_game examples/inspect.cpp)
target_link_libraries(inspect_game PRIVATE trainer_game_adapter)
