cmake_minimum_required(VERSION 3.20)
project(fritzmonitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(PkgConfig QUIET)

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
)
target_include_directories(fritzmonitor_core PUBLIC include)
target_compile_features(fritzmonitor_core PUBLIC cxx_std_20)
target_compile_options(fritzmonitor_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(fritzmonitor src/main.cpp src/desktop.cpp)
target_link_libraries(fritzmonitor PRIVATE fritzmonitor_core)
target_compile_options(fritzmonitor PRIVATE -Wall -Wextra -Wpedantic)

if(FRITZMONITOR_DESKTOP_LIBS)
  target_compile_definitions(fritzmonitor PRIVATE FRITZMONITOR_HAVE_DESKTOP=1)
  target_link_libraries(fritzmonitor PRIVATE PkgConfig::GLIB PkgConfig::LIBNOTIFY PkgConfig::APPINDICATOR)
else()
  message(STATUS "Desktop libraries not found; building console/headless mode")
endif()

if(BUILD_TESTING)
  add_executable(callmonitor_parser_test tests/callmonitor_parser_test.cpp)
  target_link_libraries(callmonitor_parser_test PRIVATE fritzmonitor_core)
  target_compile_options(callmonitor_parser_test PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME callmonitor_parser_test COMMAND callmonitor_parser_test)

  add_executable(config_test tests/config_test.cpp)
  target_link_libraries(config_test PRIVATE fritzmonitor_core)
  target_compile_options(config_test PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME config_test COMMAND config_test)
endif()

install(TARGETS fritzmonitor RUNTIME DESTINATION bin)
install(FILES systemd/fritzmonitor.service DESTINATION share/systemd/user)
