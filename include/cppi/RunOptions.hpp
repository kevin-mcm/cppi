#pragma once

/// @file RunOptions.hpp
/// Per-run settings: budget, cost model and an optional observer.

#include <cppi/CostModel.hpp>

#include <cstdint>

namespace cppi {

class ExecutionObserver;

struct RunOptions {
    /// Maximum operations this run may consume.
    std::uint64_t budget = 1'000'000;
    CostModel cost;
    /// Deepest chain of nested calls before a stack overflow (recursion).
    std::uint32_t max_call_depth = 1000;
    /// Optional, not owned. Must outlive the run.
    ExecutionObserver* observer = nullptr;
};

}  // namespace cppi
