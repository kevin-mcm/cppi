# Top-level CMake project of cppi: the library, its tests, tools,
# benchmarks and fuzzers.
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-08

cmake_minimum_required(VERSION 3.23)

project(
  cppi
  VERSION 0.2.0
  DESCRIPTION "Embeddable interpreter for a progressively unlockable subset of C++"
  HOMEPAGE_URL "https://github.com/kevin-mcm/cppi"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
include(CTest) # defines BUILD_TESTING (Conan turns it off with tools.build:skip_test)

option(CPPI_BUILD_TOOLS "Build the cppi-run console tool" ON)
option(CPPI_BUILD_BENCHMARKS "Build the Google Benchmark suite" OFF)
option(CPPI_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
option(CPPI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CPPI_ENABLE_COVERAGE "Instrument for code coverage (GCC/Clang)" OFF)
option(CPPI_ENABLE_CLANG_TIDY "Run clang-tidy while compiling" OFF)
set(CPPI_SANITIZERS "" CACHE STRING "Semicolon list of sanitizers, e.g. address;undefined")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

include(CompilerWarnings)
include(Sanitizers)
include(Coverage)

if(EMSCRIPTEN)
  # Exceptions are off by default in Emscripten; cppi needs them to contain
  # host-function failures. Executables (tests, tools) run under Node.js
  # with direct access to the host file system. Hosts that use wasm-native
  # exceptions (such as Godot web extensions, built with
  # -sSUPPORT_LONGJMP=wasm) pass -fwasm-exceptions in CMAKE_CXX_FLAGS, and
  # the two exception models cannot be mixed.
  if(NOT CMAKE_CXX_FLAGS MATCHES "-fwasm-exceptions")
    add_compile_options(-fexceptions)
    add_link_options(-fexceptions)
  endif()
  # The same stack as Godot's web export gives its main module (Emscripten's
  # default, 64 KiB, is too small for the compiler's recursive stages).
  set(CPPI_EMSCRIPTEN_EXE_FLAGS -sNODERAWFS=1 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=5MB)
endif()

if(CPPI_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set(CPPI_CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
endif()

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
add_subdirectory(src)

if(CPPI_BUILD_TOOLS)
  add_subdirectory(tools/cppi-run)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(CPPI_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

if(CPPI_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()

# ---------------------------------------------------------------------------
# Install (used by `conan create` and by non-Conan consumers)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  TARGETS cppi
  EXPORT cppiTargets
  FILE_SET public_headers
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(CPPI_BUILD_TOOLS)
  install(TARGETS cppi-run RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(
  EXPORT cppiTargets
  NAMESPACE cppi::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppi)

configure_package_config_file(cmake/cppiConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/cppiConfig.cmake"
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppi)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/cppiConfigVersion.cmake"
                                 COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cppiConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/cppiConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppi)
