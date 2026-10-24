cmake_minimum_required(VERSION 3.24)
project(traineros_stream_probe LANGUAGES C CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This isolated probe requires Linux V4L2/GStreamer")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(FetchContent)
FetchContent_Declare(ml
  GIT_REPOSITORY https://github.com/moonlight-stream/moonlight-common-c.git
  GIT_TAG e2722dc83791302b724422ede0ae4192504ab2cd)
FetchContent_Declare(wolf
  GIT_REPOSITORY https://github.com/games-on-whales/wolf.git
  GIT_TAG 541c541983b5705332f9e66333f7c0a3ed090ce6
  GIT_SUBMODULES "" SOURCE_SUBDIR unused-no-runner)
FetchContent_Declare(wolf_rs
  GIT_REPOSITORY https://github.com/sleepybishop/nanors.git
  GIT_TAG 19f07b513e924e471cadd141943c1ec4adc8d0e0
  SOURCE_SUBDIR unused-no-project)
FetchContent_MakeAvailable(ml wolf wolf_rs)
FetchContent_Declare(inputtino
  GIT_REPOSITORY https://github.com/games-on-whales/inputtino.git
  GIT_TAG fd136cfe492b4375b4507718bcca1f044588fc6f)
set(stream_saved_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(inputtino)
set(BUILD_SHARED_LIBS ${stream_saved_shared})
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl2)
find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost CONFIG REQUIRED)
set(W ${CMAKE_CURRENT_BINARY_DIR}/wolf-parts)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/prepare.py
  ${wolf_SOURCE_DIR} ${W} COMMAND_ERROR_IS_FATAL ANY)
add_library(wolf_packets STATIC
  ${W}/src/moonlight-server/gst-plugin/gstrtpmoonlightpay_video.cpp
  ${W}/src/moonlight-server/gst-plugin/gstrtpmoonlightpay_audio.cpp
  ${W}/src/moonlight-protocol/crypto/src/crypto.cpp
  ${W}/src/moonlight-protocol/crypto/src/utils.cpp
  ${W}/src/moonlight-protocol/nanors/rswrapper.c)
target_include_directories(wolf_packets PUBLIC support
  ${W}/src/moonlight-server ${W}/src/moonlight-protocol
  ${W}/src/moonlight-protocol/crypto ${W}/src/moonlight-protocol/nanors
  ${wolf_rs_SOURCE_DIR} ${wolf_rs_SOURCE_DIR}/deps ${wolf_rs_SOURCE_DIR}/deps/obl
  ${Boost_INCLUDE_DIRS})
target_link_libraries(wolf_packets PUBLIC PkgConfig::GST OpenSSL::Crypto fmt::fmt)
add_executable(stream_host host.cpp ${ml_SOURCE_DIR}/src/RtspParser.c)
target_include_directories(stream_host PRIVATE ${ml_SOURCE_DIR}/src)
target_link_libraries(stream_host PRIVATE wolf_packets enet libinputtino)
add_executable(stream_guest guest.cpp)
set(TRAINEROS_INPUT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/input" CACHE PATH "TrainerOS shared controller identity headers")
target_include_directories(stream_guest PRIVATE ${TRAINEROS_INPUT_INCLUDE})
target_link_libraries(stream_guest PRIVATE moonlight-common-c PkgConfig::GST PkgConfig::SDL)
foreach(target stream_host stream_guest)
  target_link_options(${target} PRIVATE -Wl,--as-needed)
endforeach()
# Separate manifest bundle via package.py; no media-library link in the shell.
include(CTest)
if(BUILD_TESTING)
  add_executable(stream_port_fixture port_fixture.cpp)
  target_link_libraries(stream_port_fixture PRIVATE libinputtino)
  add_executable(config_test config_test.cpp)
  add_test(NAME stream_config COMMAND config_test)
  add_executable(crypto_test crypto_test.cpp)
  target_link_libraries(crypto_test PRIVATE wolf_packets)
  add_test(NAME stream_crypto COMMAND crypto_test)
  add_executable(pad_test pad_test.cpp)
  add_test(NAME stream_pad_lease COMMAND pad_test)
  add_executable(shared_source_test shared_source_test.cpp)
  target_link_libraries(shared_source_test PRIVATE PkgConfig::GST)
  add_test(NAME stream_shared_idr COMMAND shared_source_test)
  add_executable(stream_guest_fixture guest_fixture.cpp)
  target_include_directories(stream_guest_fixture PRIVATE ${TRAINEROS_INPUT_INCLUDE})
  target_link_libraries(stream_guest_fixture PRIVATE moonlight-common-c PkgConfig::GST PkgConfig::SDL)
endif()
