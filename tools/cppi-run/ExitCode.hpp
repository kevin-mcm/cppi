#pragma once

/// @file ExitCode.hpp
/// @brief Process exit codes of cppi-run (sysexits.h values for usage errors).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

/// Exit codes.
namespace cppi_run::exit_code {

/// The program ran to completion.
inline constexpr int kOk = 0;
/// The program did not compile.
inline constexpr int kCompileError = 1;
/// The program stopped early (budget, runtime error, host error).
inline constexpr int kRuntimeStop = 2;
/// Bad command line (EX_USAGE).
inline constexpr int kUsage = 64;
/// The program file cannot be read (EX_NOINPUT).
inline constexpr int kNoInput = 66;

}  // namespace cppi_run::exit_code
