cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FRITZMONITOR_VERSION
     LIMIT_COUNT 1)
string(STRIP "${FRITZMONITOR_VERSION}" FRITZMONITOR_VERSION)
if(NOT FRITZMONITOR_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
  message(FATAL_ERROR "VERSION must contain a SemVer MAJOR.MINOR.PATCH value")
endif()
project(fritzmonitor VERSION ${FRITZMONITOR_VERSION} LANGUAGES CXX)

set(FRITZMONITOR_SERVICE_EXECUTABLE "%h/.local/bin/fritzmonitor"
    CACHE STRING "Executable path used by the user service")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)

set(FRITZMONITOR_DESKTOP_LIBS OFF)
if(PkgConfig_FOUND)
  pkg_check_modules(GLIB QUIET IMPORTED_TARGET glib-2.0 gio-2.0 gtk+-3.0)
  pkg_check_modules(LIBNOTIFY QUIET IMPORTED_TARGET libnotify)
  pkg_check_modules(APPINDICATOR QUIET IMPORTED_TARGET ayatana-appindicator3-0.1)
  if(NOT APPINDICATOR_FOUND)
    pkg_check_modules(APPINDICATOR QUIET IMPORTED_TARGET appindicator-0.1)
  endif()
  if(GLIB_FOUND AND LIBNOTIFY_FOUND AND APPINDICATOR_FOUND)
    set(FRITZMONITOR_DESKTOP_LIBS ON)
  endif()
endif()

add_library(fritzmonitor_core
  src/call_event.cpp
  src/callmonitor_parser.cpp
  src/config.cpp
  src/callmonitor_client.cpp
  src/phonebook.cpp
  src/secret_store.cpp
)
target_include_directories(fritzmonitor_core PUBLIC include)
target_compile_features(fritzmonitor_core PUBLIC cxx_std_20)
target_compile_options(fritzmonitor_core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(fritzmonitor_core PRIVATE PkgConfig::LIBSECRET)

find_package(CURL QUIET)
find_package(LibXml2 QUIET)
if(CURL_FOUND AND LIBXML2_FOUND)
  target_compile_definitions(fritzmonitor_core PRIVATE FRITZMONITOR_HAVE_PHONEBOOK=1)
  target_link_libraries(fritzmonitor_core PRIVATE CURL::libcurl LibXml2::LibXml2)
else()
  message(STATUS "Phonebook libraries not found; TR-064 address book support disabled")
endif()

add_executable(fritzmonitor src/main.cpp src/desktop.cpp)
target_link_libraries(fritzmonitor PRIVATE fritzmonitor_core)
target_compile_options(fritzmonitor PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(fritzmonitor PRIVATE
  FRITZMONITOR_VERSION="${PROJECT_VERSION}")

if(FRITZMONITOR_DESKTOP_LIBS)
  target_compile_definitions(fritzmonitor PRIVATE FRITZMONITOR_HAVE_DESKTOP=1)
  target_link_libraries(fritzmonitor PRIVATE PkgConfig::GLIB PkgConfig::LIBNOTIFY PkgConfig::APPINDICATOR)
else()
  message(STATUS "Desktop libraries not found; building console/headless mode")
endif()

if(BUILD_TESTING)
  add_executable(callmonitor_parser_test tests/callmonitor_parser_test.cpp)
  target_link_libraries(callmonitor_parser_test PRIVATE fritzmonitor_core)
  target_compile_options(callmonitor_parser_test PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  add_test(NAME callmonitor_parser_test COMMAND callmonitor_parser_test)

  add_executable(config_test tests/config_test.cpp)
  target_link_libraries(config_test PRIVATE fritzmonitor_core)
  target_compile_options(config_test PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  add_test(NAME config_test COMMAND config_test)

  add_executable(secret_store_test tests/secret_store_test.cpp)
  target_link_libraries(secret_store_test PRIVATE fritzmonitor_core)
  target_compile_options(secret_store_test PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  add_test(NAME secret_store_test COMMAND secret_store_test)
endif()

install(TARGETS fritzmonitor RUNTIME DESTINATION bin)
configure_file(
  systemd/fritzmonitor.service.in
  ${CMAKE_CURRENT_BINARY_DIR}/fritzmonitor.service
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fritzmonitor.service
        DESTINATION share/systemd/user)
install(FILES
  assets/fritzmonitor-phone-green.svg
  assets/fritzmonitor-phone-red.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
install(FILES
  LICENSE
  COPYRIGHT.md
  README.md
  FRITZMONITOR.md
  QUICK-SETUP.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

set(CPACK_PACKAGE_NAME "fritzmonitor")
set(CPACK_PACKAGE_VENDOR "Thomas Tuul")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FRITZ!Box call monitor for the Linux system tray")
set(CPACK_PACKAGE_DESCRIPTION
    "Native Linux system-tray monitor for incoming FRITZ!Box calls.")
set(CPACK_PACKAGE_CONTACT "Thomas Tuul")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-only")
set(CPACK_RPM_PACKAGE_REQUIRES
    "glib2, gtk3, libnotify, libayatana-appindicator-gtk3, libcurl, libsecret, libxml2")
set(CPACK_GENERATOR "DEB;RPM")
include(CPack)

set(FRITZMONITOR_PACKAGE_DEPENDENCIES fritzmonitor)
if(BUILD_TESTING)
  list(APPEND FRITZMONITOR_PACKAGE_DEPENDENCIES
       callmonitor_parser_test config_test secret_store_test)
endif()

add_custom_target(package-deb
  DEPENDS ${FRITZMONITOR_PACKAGE_DEPENDENCIES}
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMAND ${CMAKE_CPACK_COMMAND} -G DEB --config ${CMAKE_BINARY_DIR}/CPackConfig.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Building Debian package"
)

add_custom_target(package-rpm
  DEPENDS ${FRITZMONITOR_PACKAGE_DEPENDENCIES}
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMAND ${CMAKE_CPACK_COMMAND} -G RPM --config ${CMAKE_BINARY_DIR}/CPackConfig.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Building Fedora/RPM package"
)
