#pragma once

// Process exit codes of cppi-run (sysexits.h values for usage errors).

namespace cppi_run::exit_code {

inline constexpr int kOk = 0;
inline constexpr int kCompileError = 1;
inline constexpr int kRuntimeStop = 2;
inline constexpr int kUsage = 64;
inline constexpr int kNoInput = 66;

}  // namespace cppi_run::exit_code
