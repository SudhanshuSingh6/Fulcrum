cmake_minimum_required(VERSION 3.20)
project(fulcrum VERSION 0.0.1 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "Fulcrum is Linux-only. Use scripts/dev.sh to build in the dev container.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FULCRUM_WERROR "Treat warnings as errors" ON)

# Warnings apply to our targets only, not to fetched dependencies.
add_library(fulcrum_warnings INTERFACE)
target_compile_options(fulcrum_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  $<$<BOOL:${FULCRUM_WERROR}>:-Werror>)

# ---- Dependencies -------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest spdlog)

# ---- Fulcrum ------------------------------------------------------------------
configure_file(src/core/version_config.h.in ${CMAKE_BINARY_DIR}/generated/version_config.h)

add_library(fulcrum_core STATIC
  src/control/args.cpp
  src/core/fd.cpp
  src/core/listener.cpp
  src/core/socket_util.cpp
  src/core/version.cpp
  src/proxy/tcp_proxy.cpp)
target_include_directories(fulcrum_core PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(fulcrum_core PUBLIC spdlog::spdlog PRIVATE fulcrum_warnings)

add_executable(fulcrum src/main.cpp)
target_link_libraries(fulcrum PRIVATE fulcrum_core fulcrum_warnings)

# ---- Go tools -----------------------------------------------------------------
find_program(GO_EXECUTABLE go REQUIRED)
set(FAKE_BACKEND_BIN ${CMAKE_BINARY_DIR}/tools/fake_backend)
file(GLOB FAKE_BACKEND_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tools/fake_backend/*.go)
add_custom_command(
  OUTPUT ${FAKE_BACKEND_BIN}
  COMMAND ${GO_EXECUTABLE} build -o ${FAKE_BACKEND_BIN} ./tools/fake_backend
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS ${FAKE_BACKEND_SRC} ${CMAKE_SOURCE_DIR}/go.mod
  COMMENT "go build fake_backend")
add_custom_target(fake_backend ALL DEPENDS ${FAKE_BACKEND_BIN})

# ---- Tests --------------------------------------------------------------------
enable_testing()
include(GoogleTest)

add_executable(unit_tests
  tests/unit/args_test.cpp
  tests/unit/fd_test.cpp
  tests/unit/listener_test.cpp
  tests/unit/version_test.cpp)
target_link_libraries(unit_tests PRIVATE fulcrum_core fulcrum_warnings GTest::gtest_main)
gtest_discover_tests(unit_tests)

add_executable(integration_tests
  tests/integration/process.cpp
  tests/integration/http_client.cpp
  tests/integration/proxy_fixture.cpp
  tests/integration/fake_backend_test.cpp
  tests/integration/m1_listen_test.cpp)
target_include_directories(integration_tests PRIVATE tests/integration)
target_compile_definitions(integration_tests PRIVATE
  FULCRUM_BIN="$<TARGET_FILE:fulcrum>"
  FAKE_BACKEND_BIN="${FAKE_BACKEND_BIN}")
target_link_libraries(integration_tests PRIVATE fulcrum_warnings GTest::gtest_main)
add_dependencies(integration_tests fulcrum fake_backend)
gtest_discover_tests(integration_tests PROPERTIES LABELS integration)
