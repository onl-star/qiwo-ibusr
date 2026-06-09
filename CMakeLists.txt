cmake_minimum_required(VERSION 3.10)
project(qiwo-ibus)

set(qiwo_ibus_version 2.0.0)

option(BUILD_STATIC "Build Rime using static libraries" OFF)
option(BUILD_TESTING "Build qiwo-ibus tests" ON)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(${CMAKE_ROOT}/Modules/FindPkgConfig.cmake)
pkg_check_modules(IBus REQUIRED ibus-1.0)
if(IBus_FOUND)
  include_directories(${IBus_INCLUDE_DIRS})
  link_directories(${IBus_LIBRARY_DIRS})
endif(IBus_FOUND)

pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
if (LIBNOTIFY_FOUND)
  include_directories(${LIBNOTIFY_INCLUDE_DIRS})
  link_directories(${LIBNOTIFY_LIBRARY_DIRS})
endif(LIBNOTIFY_FOUND)

pkg_check_modules(GTK REQUIRED gtk+-3.0)
if(GTK_FOUND)
  include_directories(${GTK_INCLUDE_DIRS})
  link_directories(${GTK_LIBRARY_DIRS})
endif(GTK_FOUND)

pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
if(LIBSECRET_FOUND)
  include_directories(${LIBSECRET_INCLUDE_DIRS})
  link_directories(${LIBSECRET_LIBRARY_DIRS})
  add_definitions(-DQIWO_WITH_LIBSECRET)
endif(LIBSECRET_FOUND)

find_package(Rime REQUIRED)
if(Rime_FOUND)
  include_directories(${Rime_INCLUDE_DIR})
endif(Rime_FOUND)

if(NOT DEFINED RIME_DATA_DIR)
  find_package(RimeData REQUIRED)
endif(NOT DEFINED RIME_DATA_DIR)
message(STATUS "Precompiler macro RIME_DATA_DIR is set to \"${RIME_DATA_DIR}\"")
add_definitions(-DRIME_DATA_DIR="${RIME_DATA_DIR}")

set(qiwo_sync_dir "${CMAKE_INSTALL_FULL_DATADIR}/qiwo")
add_definitions(-DQIWO_SYNC_DIR="${qiwo_sync_dir}")

set(ibus_rime_icons_dir "${CMAKE_INSTALL_FULL_DATADIR}/qiwo/icons")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/rime_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/rime_config.h"
  @ONLY)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

if(BUILD_STATIC)
  find_package(ICU REQUIRED uc)
  if(ICU_FOUND)
    set(RIME_DEPS ${RIME_DEPS} ${ICU_LIBRARIES})
  endif()

  link_directories(${PROJECT_SOURCE_DIR}/lib)
  set(RIME_DEPS ${RIME_DEPS} m stdc++ pthread
      libboost_filesystem.a libboost_locale.a libboost_regex.a libboost_signals.a libboost_system.a libboost_thread.a
      libglog.a libleveldb.a libmarisa.a libopencc.a libyaml-cpp.a)
endif(BUILD_STATIC)

set(IBUS_RIME_SRC
  rime_engine.c
  rime_main.c
  rime_settings.c
  qiwo_webdav_config.c
  qiwo_sync_command.c)
add_executable(ibus-engine-rime ${IBUS_RIME_SRC})
target_link_libraries(ibus-engine-rime ${IBus_LIBRARIES} ${LIBNOTIFY_LIBRARIES} ${LIBSECRET_LIBRARIES} ${Rime_LIBRARIES} ${RIME_DEPS})

add_executable(qiwo-webdav-settings
  qiwo_webdav_settings.c
  qiwo_webdav_config.c
  qiwo_sync_command.c)
target_link_libraries(qiwo-webdav-settings ${GTK_LIBRARIES} ${LIBSECRET_LIBRARIES})

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/qiwo.xml.in"
  "${CMAKE_CURRENT_BINARY_DIR}/qiwo.xml"
  @ONLY)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/qiwo-webdav-settings.desktop.in"
  "${CMAKE_CURRENT_BINARY_DIR}/qiwo-webdav-settings.desktop"
  @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qiwo.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/ibus/component")
install(TARGETS ibus-engine-rime DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/qiwo")
install(TARGETS qiwo-webdav-settings DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qiwo-webdav-settings.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(DIRECTORY icons DESTINATION "${CMAKE_INSTALL_DATADIR}/qiwo/icons" FILES_MATCHING PATTERN "*.png")
install(PROGRAMS qiwo_sync.py DESTINATION "${CMAKE_INSTALL_DATADIR}/qiwo" RENAME qiwo-rime-sync)
install(FILES ibus_rime.yaml DESTINATION "${RIME_DATA_DIR}")

if(BUILD_TESTING)
  enable_testing()

  add_executable(test_qiwo_webdav_config
    tests/test_qiwo_webdav_config.c
    qiwo_webdav_config.c)
  target_include_directories(test_qiwo_webdav_config PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(test_qiwo_webdav_config ${LIBSECRET_LIBRARIES})
  add_test(NAME qiwo_webdav_config COMMAND test_qiwo_webdav_config)

  add_executable(test_qiwo_sync_command
    tests/test_qiwo_sync_command.c
    qiwo_sync_command.c
    qiwo_webdav_config.c)
  target_include_directories(test_qiwo_sync_command PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(test_qiwo_sync_command ${LIBSECRET_LIBRARIES})
  add_test(NAME qiwo_sync_command COMMAND test_qiwo_sync_command)

  add_executable(test_rime_settings
    tests/test_rime_settings.c
    rime_settings.c
    qiwo_webdav_config.c)
  target_include_directories(test_rime_settings BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/stubs")
  target_include_directories(test_rime_settings PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(test_rime_settings ${LIBSECRET_LIBRARIES})
  add_test(NAME rime_settings COMMAND test_rime_settings)
endif()
