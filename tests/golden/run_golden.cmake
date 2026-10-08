# Runs one golden case.
#   <case>.args  command-line arguments, one per line
#   <case>.out   expected stdout
#   <case>.err   expected stderr
#   <case>.code  expected exit code
# EMULATOR (optional, '|'-separated) runs cppi-run under qemu, node, etc.
# Set CPPI_UPDATE_GOLDEN=1 in the environment to (re)write the expectations.

file(STRINGS "${CASE}.args" args)
string(REPLACE "|" ";" emulator "${EMULATOR}")
execute_process(
  COMMAND ${emulator} "${CPPI_RUN}" ${args}
  WORKING_DIRECTORY "${WORKDIR}"
  OUTPUT_VARIABLE actual_out
  ERROR_VARIABLE actual_err
  RESULT_VARIABLE actual_code)

# Windows writes CRLF to stdout in text mode; compare normalized text.
string(REPLACE "\r\n" "\n" actual_out "${actual_out}")
string(REPLACE "\r\n" "\n" actual_err "${actual_err}")

if(UPDATE)
  file(WRITE "${CASE}.out" "${actual_out}")
  file(WRITE "${CASE}.err" "${actual_err}")
  file(WRITE "${CASE}.code" "${actual_code}\n")
  message(STATUS "updated ${CASE}")
  return()
endif()

file(READ "${CASE}.out" expected_out)
file(READ "${CASE}.err" expected_err)
file(STRINGS "${CASE}.code" expected_code)

set(failed FALSE)
if(NOT actual_code STREQUAL expected_code)
  message(SEND_ERROR "exit code: expected ${expected_code}, got ${actual_code}")
  set(failed TRUE)
endif()
if(NOT actual_out STREQUAL expected_out)
  message(SEND_ERROR "stdout differs.\n--- expected\n${expected_out}\n--- actual\n${actual_out}")
  set(failed TRUE)
endif()
if(NOT actual_err STREQUAL expected_err)
  message(SEND_ERROR "stderr differs.\n--- expected\n${expected_err}\n--- actual\n${actual_err}")
  set(failed TRUE)
endif()
if(failed)
  message(FATAL_ERROR "golden case failed: ${CASE}")
endif()
