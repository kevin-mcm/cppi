#pragma once

// cppi-run as a whole: parse arguments, load the program, compile it, run it
// on the farm and report. main() only forwards to Application::run().

#include "CommandLineOptions.hpp"
#include "Messages.hpp"

#include <span>
#include <string_view>

namespace cppi_run {

class Application {
public:
    /// Returns the process exit code (see ExitCode.hpp).
    [[nodiscard]] int run(std::span<const std::string_view> args) const;

private:
    [[nodiscard]] static int execute(const CommandLineOptions& options, const Messages& messages);
};

}  // namespace cppi_run
