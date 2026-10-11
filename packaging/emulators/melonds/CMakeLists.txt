cmake_minimum_required(VERSION 3.16)
project(TrainerOSDetMPChecks LANGUAGES C CXX)

# This builds the real, patched emulator core and a focused DetMP lifecycle
# executable. It is not a frontend or an Internet-capable runtime bundle.
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

add_executable(traineros_ds_pair_probe native_pair_probe.cpp NativeSession.cpp
    ProbePlatform.cpp "${MELONDS_SOURCE}/src/net/DetMP.cpp")
target_compile_features(traineros_ds_pair_probe PRIVATE cxx_std_17)
target_include_directories(traineros_ds_pair_probe PRIVATE
    "${MELONDS_SOURCE}/src" "${MELONDS_SOURCE}/src/net")
target_link_libraries(traineros_ds_pair_probe PRIVATE core Threads::Threads)
