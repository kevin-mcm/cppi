find_package(GTest REQUIRED CONFIG)
include(GoogleTest)

add_executable(
  cppi_tests
  unit/FeaturesTest.cpp
  unit/HostRegistryTest.cpp
  unit/ParserTest.cpp
  unit/CompileTest.cpp
  unit/VmTest.cpp
  unit/LanguageTest.cpp
  unit/DebuggerTest.cpp
  unit/ModernTest.cpp
  unit/CompletenessTest.cpp
  unit/ProgramTest.cpp
  unit/DiagnosticsTest.cpp
  unit/NumberFormatTest.cpp)

# Tests may reach into internal modules (parser, analyzer) through src/.
target_include_directories(cppi_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cppi_tests PRIVATE cppi::cppi GTest::gtest_main)
cppi_set_warnings(cppi_tests)
if(EMSCRIPTEN)
  target_link_options(cppi_tests PRIVATE ${CPPI_EMSCRIPTEN_EXE_FLAGS})
endif()

# Test discovery runs the binary. When cross-compiling without an emulator
# (Android, iOS) defer it to ctest time so the build itself succeeds.
if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
  set(cppi_discovery_mode PRE_TEST)
else()
  set(cppi_discovery_mode POST_BUILD)
endif()
# Suites named *Threads start threads; they get the `threads` label, so
# `ctest -LE threads` skips them where threads are not available.
gtest_discover_tests(cppi_tests DISCOVERY_MODE ${cppi_discovery_mode} TEST_FILTER "-*Threads.*"
                     PROPERTIES LABELS unit)
gtest_discover_tests(cppi_tests DISCOVERY_MODE ${cppi_discovery_mode} TEST_FILTER "*Threads.*"
                     PROPERTIES LABELS threads)

# --- Golden tests: run cppi-run and compare stdout/stderr/exit code ---------
if(TARGET cppi-run)
  # When cross-compiling (qemu, node for WebAssembly...) run cppi-run through
  # the emulator. Lists can't cross -D boundaries, so encode ';' as '|'.
  string(REPLACE ";" "|" cppi_emulator "${CMAKE_CROSSCOMPILING_EMULATOR}")
  file(GLOB golden_cases CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/golden/*.args)
  foreach(case_file IN LISTS golden_cases)
    get_filename_component(case_name ${case_file} NAME_WE)
    add_test(
      NAME golden.${case_name}
      COMMAND
        ${CMAKE_COMMAND} -DCPPI_RUN=$<TARGET_FILE:cppi-run> -DCASE=${CMAKE_CURRENT_SOURCE_DIR}/golden/${case_name}
        -DWORKDIR=${PROJECT_SOURCE_DIR} -DUPDATE=$ENV{CPPI_UPDATE_GOLDEN} -DEMULATOR=${cppi_emulator} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/golden/run_golden.cmake)
    set_tests_properties(golden.${case_name} PROPERTIES LABELS golden)
  endforeach()
endif()
