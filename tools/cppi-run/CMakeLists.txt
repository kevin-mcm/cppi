# cppi-run, the command-line runner over a simulated farm.
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-08

add_executable(
  cppi-run
  Main.cpp
  Application.cpp
  CommandLineParser.cpp
  DiagnosticPrinter.cpp
  EnglishCatalog.cpp
  FarmHostBindings.cpp
  FarmWorld.cpp
  Language.cpp
  MessageCatalog.cpp
  Messages.cpp
  SourceReader.cpp
  SpanishCatalog.cpp
  TraceObserver.cpp
  ValueFormatter.cpp)
target_link_libraries(cppi-run PRIVATE cppi::cppi)
cppi_set_warnings(cppi-run)
# MSVC deprecates std::getenv (C4996); cppi-run reads LANG once, at startup.
target_compile_definitions(cppi-run PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
if(EMSCRIPTEN)
  target_link_options(cppi-run PRIVATE ${CPPI_EMSCRIPTEN_EXE_FLAGS})
endif()
