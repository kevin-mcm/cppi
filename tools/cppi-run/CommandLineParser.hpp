#pragma once

/// @file CommandLineParser.hpp
/// @brief Parses cppi-run's arguments.
///
/// Prints --help, --version and usage errors itself, and says which exit code
/// the process should end with.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "CommandLineOptions.hpp"

#include <optional>
#include <ostream>
#include <span>
#include <string_view>

namespace cppi_run {

/// Parses cppi-run's command line, printing help, the version or errors.
class CommandLineParser {
public:
    /// The parsed options, or the exit code to stop with.
    struct Outcome {
        std::optional<CommandLineOptions> options;  ///< nullopt: exit with `exit_code`
        /// Exit code when `options` is empty.
        int exit_code = 0;
    };

    /// @param out Where `--help` and `--version` print.
    /// @param err Where errors print.
    CommandLineParser(std::ostream& out, std::ostream& err) noexcept : out_(out), err_(err) {}

    /// Parses `args` (without the program name).
    [[nodiscard]] Outcome parse(std::span<const std::string_view> args) const;

    /// The `--help` text.
    static const std::string_view kUsage;

private:
    /// Prints `message` and the usage to `err`; exits with kUsage.
    [[nodiscard]] Outcome fail(std::string_view message) const;

    /// Normal output.
    std::ostream& out_;
    /// Error output.
    std::ostream& err_;
};

}  // namespace cppi_run
