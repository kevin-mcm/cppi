#pragma once

/// @file RunResult.hpp
/// Outcome of running a program to completion.

#include <cppi/Diagnostic.hpp>
#include <cppi/LineOperations.hpp>
#include <cppi/RunStatus.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cppi {

struct RunResult {
    RunStatus status = RunStatus::Completed;
    std::uint64_t operations = 0;
    std::vector<Diagnostic> diagnostics;
    /// Text the program printed (std::cout).
    std::string output;
    /// Where the operations went: one entry per line that spent any, by line
    /// number. They add up to `operations`.
    std::vector<LineOperations> line_operations;

    [[nodiscard]] bool ok() const noexcept { return status == RunStatus::Completed; }
};

}  // namespace cppi
