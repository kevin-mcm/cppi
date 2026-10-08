#pragma once

// Parses cppi-run's arguments. Prints --help, --version and usage errors
// itself, and says which exit code the process should end with.

#include "CommandLineOptions.hpp"

#include <optional>
#include <ostream>
#include <span>
#include <string_view>

namespace cppi_run {

class CommandLineParser {
public:
    struct Outcome {
        std::optional<CommandLineOptions> options;  ///< nullopt: exit with `exit_code`
        int exit_code = 0;
    };

    CommandLineParser(std::ostream& out, std::ostream& err) noexcept : out_(out), err_(err) {}

    [[nodiscard]] Outcome parse(std::span<const std::string_view> args) const;

    static const std::string_view kUsage;

private:
    [[nodiscard]] Outcome fail(std::string_view message) const;

    std::ostream& out_;
    std::ostream& err_;
};

}  // namespace cppi_run
