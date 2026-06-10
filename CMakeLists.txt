cmake_minimum_required(VERSION 3.10)
project(qiwo-ibus)

set(qiwo_ibus_version 2.0.0)

option(BUILD_STATIC "Build Rime using static libraries" OFF)
option(BUILD_TESTING "Build qiwo-ibus tests" ON)
set(QIWO_RIME_SYNC_BIN "" CACHE FILEPATH "Path to a prebuilt shared qiwo-rime-sync executable")
set(QIWO_SYNC_CORE_DIR "" CACHE PATH "Path to qiwo-sync-core source tree")

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

set(QIWO_RIME_FROST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rime-frost")
if(NOT EXISTS "${QIWO_RIME_FROST_DIR}/rime_frost.schema.yaml")
  message(FATAL_ERROR
    "rime-frost submodule is not initialized. Run: git submodule update --init --recursive")
endif()

set(qiwo_sync_dir "${CMAKE_INSTALL_FULL_DATADIR}/qiwo")
add_definitions(-DQIWO_SYNC_DIR="${qiwo_sync_dir}")

if(NOT QIWO_SYNC_CORE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qiwo-sync-core/Cargo.toml")
  set(QIWO_SYNC_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qiwo-sync-core"
      CACHE PATH "Path to qiwo-sync-core source tree" FORCE)
elseif(NOT QIWO_SYNC_CORE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../qiwo-sync-core/Cargo.toml")
  set(QIWO_SYNC_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../qiwo-sync-core"
      CACHE PATH "Path to qiwo-sync-core source tree" FORCE)
endif()
if(QIWO_RIME_SYNC_BIN)
  get_filename_component(QIWO_RIME_SYNC_BIN "${QIWO_RIME_SYNC_BIN}" ABSOLUTE)
endif()
if(QIWO_SYNC_CORE_DIR)
  get_filename_component(QIWO_SYNC_CORE_DIR "${QIWO_SYNC_CORE_DIR}" ABSOLUTE)
endif()

set(QIWO_RIME_SYNC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/qiwo-rime-sync")
if(QIWO_RIME_SYNC_BIN)
  if(NOT EXISTS "${QIWO_RIME_SYNC_BIN}")
    message(FATAL_ERROR
      "QIWO_RIME_SYNC_BIN does not exist: ${QIWO_RIME_SYNC_BIN}. "
      "Pass a valid prebuilt qiwo-rime-sync executable or use QIWO_SYNC_CORE_DIR.")
  endif()
  add_custom_command(
    OUTPUT "${QIWO_RIME_SYNC_OUTPUT}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${QIWO_RIME_SYNC_BIN}" "${QIWO_RIME_SYNC_OUTPUT}"
    DEPENDS "${QIWO_RIME_SYNC_BIN}"
    COMMENT "Staging prebuilt shared qiwo-rime-sync"
    VERBATIM)
else()
  if(NOT QIWO_SYNC_CORE_DIR OR NOT EXISTS "${QIWO_SYNC_CORE_DIR}/Cargo.toml")
    message(FATAL_ERROR
      "qiwo-sync-core source tree was not found. "
      "Set QIWO_SYNC_CORE_DIR to a directory containing Cargo.toml, "
      "or set QIWO_RIME_SYNC_BIN to a prebuilt shared qiwo-rime-sync executable.")
  endif()
  find_program(CARGO_EXECUTABLE cargo)
  if(NOT CARGO_EXECUTABLE)
    message(FATAL_ERROR
      "cargo was not found. Install Rust/Cargo or set QIWO_RIME_SYNC_BIN "
      "to a prebuilt shared qiwo-rime-sync executable.")
  endif()
  add_custom_command(
    OUTPUT "${QIWO_RIME_SYNC_OUTPUT}"
    COMMAND "${CARGO_EXECUTABLE}" build --release
            --manifest-path "${QIWO_SYNC_CORE_DIR}/Cargo.toml"
            -p qiwo-rime-sync
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${QIWO_SYNC_CORE_DIR}/target/release/qiwo-rime-sync"
            "${QIWO_RIME_SYNC_OUTPUT}"
    WORKING_DIRECTORY "${QIWO_SYNC_CORE_DIR}"
    COMMENT "Building shared qiwo-rime-sync from qiwo-sync-core"
    VERBATIM)
endif()
add_custom_target(qiwo-rime-sync-tool ALL DEPENDS "${QIWO_RIME_SYNC_OUTPUT}")

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
  qiwo_rime_default_config.c
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
install(PROGRAMS "${QIWO_RIME_SYNC_OUTPUT}" DESTINATION "${CMAKE_INSTALL_DATADIR}/qiwo")
install(DIRECTORY "${QIWO_RIME_FROST_DIR}/" DESTINATION "${RIME_DATA_DIR}"
  FILES_MATCHING
  PATTERN "*.yaml"
  PATTERN "*.dict.yaml"
  PATTERN "*.txt"
  PATTERN "*.json"
  PATTERN "*.ocd2"
  PATTERN "*.gram"
  PATTERN "default.yaml" EXCLUDE
  PATTERN ".git*" EXCLUDE
  PATTERN ".github" EXCLUDE
  PATTERN "README.md" EXCLUDE
  PATTERN "LICENSE" EXCLUDE)
install(FILES ibus_rime.yaml DESTINATION "${RIME_DATA_DIR}")

if(BUILD_TESTING)
  enable_testing()

  add_test(NAME sync_core_packaging
           COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sync_core_packaging.sh")
  add_test(NAME install_linux_sync_core
           COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_install_linux_sync_core.sh")
  add_test(NAME sync_core_dependencies
           COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sync_core_dependencies.sh")
  add_test(NAME sync_core_file_selector
           COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sync_core_file_selector.sh")
  set_tests_properties(sync_core_file_selector PROPERTIES
    ENVIRONMENT "QIWO_SYNC_CORE_DIR=${QIWO_SYNC_CORE_DIR}")

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

  add_executable(test_qiwo_rime_default_config
    tests/test_qiwo_rime_default_config.c
    qiwo_rime_default_config.c)
  target_include_directories(test_qiwo_rime_default_config PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(test_qiwo_rime_default_config ${LIBSECRET_LIBRARIES})
  add_test(NAME qiwo_rime_default_config COMMAND test_qiwo_rime_default_config)
endif()
