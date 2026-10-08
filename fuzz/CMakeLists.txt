if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "CPPI_BUILD_FUZZERS requires Clang (libFuzzer)")
endif()

add_executable(fuzz_compile_run FuzzCompileRun.cpp)
target_link_libraries(fuzz_compile_run PRIVATE cppi::cppi)
target_compile_options(fuzz_compile_run PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_compile_run PRIVATE -fsanitize=fuzzer)
