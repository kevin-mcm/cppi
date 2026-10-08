#pragma once

/// @file Application.hpp
/// @brief cppi-run as a whole: parse arguments, load the program, compile it,
/// run it on the farm and report.
///
/// main() only forwards to Application::run().
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "CommandLineOptions.hpp"
#include "Messages.hpp"

#include <span>
#include <string_view>

namespace cppi_run {

/// The cppi-run program.
class Application {
public:
    /// Returns the process exit code (see ExitCode.hpp).
    /// @param args Command-line arguments, without the program name.
    [[nodiscard]] int run(std::span<const std::string_view> args) const;

private:
    /// Reads, compiles and runs the program with parsed `options`; returns the
    /// exit code.
    [[nodiscard]] static int execute(const CommandLineOptions& options, const Messages& messages);
};

}  // namespace cppi_run
