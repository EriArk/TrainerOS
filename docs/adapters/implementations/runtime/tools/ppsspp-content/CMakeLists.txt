cmake_minimum_required(VERSION 3.24)
project(TrainerPspContent LANGUAGES CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The headless reader recipe requires Linux ELF section garbage collection")
endif()
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(ZLIB REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PPSSPP_SOURCE "" CACHE PATH "PPSSPP v1.20.4 checkout with libchdr headers")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare-source.py"
    "${PPSSPP_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/BlockDevices.cpp" COMMAND_ERROR_IS_FATAL ANY)
add_executable(psp-content-probe main.cpp headless.cpp "${CMAKE_CURRENT_BINARY_DIR}/BlockDevices.cpp"
    "${PPSSPP_SOURCE}/Core/ELF/PBPReader.cpp" "${PPSSPP_SOURCE}/Common/StringUtils.cpp"
    "${PPSSPP_SOURCE}/Common/Data/Text/Parsers.cpp")
target_include_directories(psp-content-probe PRIVATE "${PPSSPP_SOURCE}" "${PPSSPP_SOURCE}/Common" "${PPSSPP_SOURCE}/ext/libchdr/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
target_compile_options(psp-content-probe PRIVATE -ffunction-sections -fdata-sections)
target_link_options(psp-content-probe PRIVATE -Wl,--gc-sections)
target_link_libraries(psp-content-probe PRIVATE Qt6::Core ZLIB::ZLIB)
