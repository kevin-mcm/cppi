# Instruments the build for code coverage when CPPI_ENABLE_COVERAGE is on.
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-08

if(CPPI_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
    message(STATUS "cppi: coverage instrumentation enabled")
  else()
    message(WARNING "cppi: coverage is only supported with GCC or Clang")
  endif()
endif()
