// cppi-run: runs a player program against a small simulated farm.
//
//   cppi-run [options] <file | ->
//
// Exit codes: 0 completed, 1 compile errors, 2 stopped at runtime,
//             64 bad usage, 66 cannot read input.

#include "Application.hpp"

#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    return cppi_run::Application().run(args);
}
