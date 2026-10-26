# SPDX-License-Identifier: GPL-3.0-or-later
# Optional upstream CMAKE_PROJECT_INCLUDE for focused native network checks.
# Not installed in the runtime; does not change normal Azahar targets/options.
if(PROJECT_NAME STREQUAL "citra" AND NOT TARGET traineros_room_probe)
    add_executable(traineros_room_probe "${CMAKE_CURRENT_LIST_DIR}/native-room-probe.cpp")
    target_compile_features(traineros_room_probe PRIVATE cxx_std_20)
    target_include_directories(traineros_room_probe PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(traineros_room_probe PRIVATE network citra_common json-headers)
endif()
