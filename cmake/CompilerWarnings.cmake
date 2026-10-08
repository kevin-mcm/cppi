# cppi_set_warnings(<target>)
# Strict warnings for project code only; third-party code is never affected.
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-08
function(cppi_set_warnings target)
  set(msvc_warnings /W4 /permissive- /w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14311 /w14826 /w14905
                    /w14906 /w14928)
  set(gcc_clang_warnings
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wconversion
      -Wsign-conversion
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough)
  set(gcc_only -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op)

  if(CPPI_WARNINGS_AS_ERRORS)
    list(APPEND msvc_warnings /WX)
    list(APPEND gcc_clang_warnings -Werror)
  endif()

  if(MSVC)
    target_compile_options(${target} PRIVATE ${msvc_warnings})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${target} PRIVATE ${gcc_clang_warnings} ${gcc_only})
  else()
    target_compile_options(${target} PRIVATE ${gcc_clang_warnings})
  endif()

  if(CPPI_CLANG_TIDY_COMMAND)
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CPPI_CLANG_TIDY_COMMAND}")
  endif()
endfunction()
