#pragma once

/// @file RunResult.hpp
/// @brief Outcome of running a program to completion.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Diagnostic.hpp>
#include <cppi/LineOperations.hpp>
#include <cppi/RunStatus.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cppi {

/// What happened when a program ran.
struct RunResult {
    /// How the run ended.
    RunStatus status = RunStatus::Completed;
    /// Operations consumed.
    std::uint64_t operations = 0;
    /// Runtime errors and warnings (e.g. undefined behavior, memory leaks).
    std::vector<Diagnostic> diagnostics;
    /// Text the program printed (std::cout).
    std::string output;
    /// Where the operations went: one entry per line that spent any, by line
    /// number. They add up to `operations`.
    std::vector<LineOperations> line_operations;

    /// @return true if the program ran to completion.
    [[nodiscard]] bool ok() const noexcept { return status == RunStatus::Completed; }
};

}  // namespace cppi
