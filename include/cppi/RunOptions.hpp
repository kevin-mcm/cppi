#pragma once

/// @file RunOptions.hpp
/// @brief Per-run settings: budget, cost model and an optional observer.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/CostModel.hpp>

#include <cstdint>

namespace cppi {

class ExecutionObserver;

/// Settings for one run of a program.
struct RunOptions {
    /// Maximum operations this run may consume.
    std::uint64_t budget = 1'000'000;
    /// How operations are priced.
    CostModel cost;
    /// Deepest chain of nested calls before a stack overflow (recursion).
    std::uint32_t max_call_depth = 1000;
    /// Optional, not owned. Must outlive the run.
    ExecutionObserver* observer = nullptr;
};

}  // namespace cppi
