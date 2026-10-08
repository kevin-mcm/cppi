# Applies CPPI_SANITIZERS (e.g. "address;undefined") globally so that the
# library, the tests and the tools are all instrumented consistently.
if(CPPI_SANITIZERS)
  if(MSVC)
    if("address" IN_LIST CPPI_SANITIZERS)
      add_compile_options(/fsanitize=address /Zi)
    endif()
  else()
    list(JOIN CPPI_SANITIZERS "," _cppi_sanitizers)
    add_compile_options(-fsanitize=${_cppi_sanitizers} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=${_cppi_sanitizers})
  endif()
  message(STATUS "cppi: sanitizers enabled: ${CPPI_SANITIZERS}")
endif()
