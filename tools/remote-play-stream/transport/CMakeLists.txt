cmake_minimum_required(VERSION 3.24)
project(traineros_stream_transport LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
FetchContent_Declare(datachannel
  GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
  GIT_TAG 6b1e2e620f1e37f0eafeee702eaea0043cb305fd
  GIT_SUBMODULES deps/plog deps/usrsctp deps/libjuice)
FetchContent_MakeAvailable(datachannel)
add_executable(stream_transport bridge.cpp)
target_link_libraries(stream_transport PRIVATE Qt6::Core Qt6::Network datachannel OpenSSL::Crypto)
# Isolated admission/latency gate; not an installed product transport yet.
include(CTest)
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME stream_transport_boundary COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/check.py $<TARGET_FILE:stream_transport>)
  set_tests_properties(stream_transport_boundary PROPERTIES TIMEOUT 60)
endif()
