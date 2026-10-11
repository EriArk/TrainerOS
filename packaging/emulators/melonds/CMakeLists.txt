cmake_minimum_required(VERSION 3.16)
project(TrainerOSDetMPChecks LANGUAGES C CXX)

# Build the patched core, focused checks and an optional local player frontend.
# This is not an Internet-capable runtime bundle or GameParty admission profile.
if(NOT EXISTS "${MELONDS_SOURCE}/src/net/DetMP.cpp")
    message(FATAL_ERROR "MELONDS_SOURCE must point to the pinned, patched tree")
endif()
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO_RELEASE OFF CACHE BOOL "" FORCE)
add_subdirectory("${MELONDS_SOURCE}" melonDS)

find_package(Threads REQUIRED)
add_executable(traineros_detmp_checks detmp_checks.cpp
    "${MELONDS_SOURCE}/src/net/DetMP.cpp")
target_compile_features(traineros_detmp_checks PRIVATE cxx_std_17)
target_include_directories(traineros_detmp_checks PRIVATE
    "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
target_link_libraries(traineros_detmp_checks PRIVATE Threads::Threads)
enable_testing()
add_test(NAME detmp_lifecycle COMMAND traineros_detmp_checks)
set_tests_properties(detmp_lifecycle PROPERTIES TIMEOUT 20)

add_executable(traineros_ds_input_checks input_trace_checks.cpp)
target_compile_features(traineros_ds_input_checks PRIVATE cxx_std_17)
target_include_directories(traineros_ds_input_checks PRIVATE
    "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
add_test(NAME ds_input_trace COMMAND traineros_ds_input_checks)

add_executable(traineros_ds_spi_checks spi_state_checks.cpp ProbePlatform.cpp
    "${MELONDS_SOURCE}/src/net/DetMP.cpp")
target_compile_features(traineros_ds_spi_checks PRIVATE cxx_std_17)
target_include_directories(traineros_ds_spi_checks PRIVATE
    "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
target_link_libraries(traineros_ds_spi_checks PRIVATE core Threads::Threads)
add_test(NAME ds_spi_reset COMMAND traineros_ds_spi_checks)

add_executable(traineros_ds_gpu_checks gpu_state_checks.cpp ProbePlatform.cpp
    "${MELONDS_SOURCE}/src/net/DetMP.cpp")
target_compile_features(traineros_ds_gpu_checks PRIVATE cxx_std_17)
target_include_directories(traineros_ds_gpu_checks PRIVATE
    "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
target_link_libraries(traineros_ds_gpu_checks PRIVATE core Threads::Threads)
add_test(NAME ds_gpu_state COMMAND traineros_ds_gpu_checks)

foreach(kind IN ITEMS cartridge_state_checks owned_state_probe)
    add_executable(traineros_ds_${kind} ${kind}.cpp NativeSession.cpp ProbePlatform.cpp
        "${MELONDS_SOURCE}/src/net/DetMP.cpp")
    target_compile_features(traineros_ds_${kind} PRIVATE cxx_std_17)
    target_include_directories(traineros_ds_${kind} PRIVATE
        "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
    target_link_libraries(traineros_ds_${kind} PRIVATE core Threads::Threads)
endforeach()
add_test(NAME ds_cartridge_state COMMAND traineros_ds_cartridge_state_checks)

add_executable(traineros_ds_pair_probe native_pair_probe.cpp NativeSession.cpp
    ProbePlatform.cpp "${MELONDS_SOURCE}/src/net/DetMP.cpp")
target_compile_features(traineros_ds_pair_probe PRIVATE cxx_std_17)
target_include_directories(traineros_ds_pair_probe PRIVATE
    "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
target_link_libraries(traineros_ds_pair_probe PRIVATE core Threads::Threads)

option(TRAINEROS_DS_LOCAL_FRONTEND "Build the local player frontend (no network admission)" ON)
if(TRAINEROS_DS_LOCAL_FRONTEND)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    find_package(SDL2 REQUIRED)
    add_executable(traineros_ds_prepared_checks prepared_session_checks.cpp)
    target_compile_features(traineros_ds_prepared_checks PRIVATE cxx_std_17)
    target_include_directories(traineros_ds_prepared_checks PRIVATE "${MELONDS_SOURCE}/src")
    target_link_libraries(traineros_ds_prepared_checks PRIVATE Qt6::Core)
    add_test(NAME ds_prepared_session COMMAND traineros_ds_prepared_checks)
    add_executable(traineros_ds_local LocalFrontend.cpp NativeSession.cpp
        ProbePlatform.cpp "${MELONDS_SOURCE}/src/net/DetMP.cpp")
    target_compile_features(traineros_ds_local PRIVATE cxx_std_17)
    target_include_directories(traineros_ds_local PRIVATE
        "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
    target_link_libraries(traineros_ds_local PRIVATE core Threads::Threads Qt6::Widgets SDL2::SDL2)
endif()
