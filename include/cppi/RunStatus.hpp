#pragma once

/// @file RunStatus.hpp
/// State of a running or finished program.

#include <cstdint>

namespace cppi {

enum class RunStatus : std::uint8_t {
    Running,          ///< only seen while stepping
    Completed,        ///< reached the end of the program
    BudgetExhausted,  ///< the next instruction would exceed the budget
    HostError,        ///< a host function called HostCall::fail()
    RuntimeError,     ///< undefined behavior (5xx) or a resource limit (stack, memory)
    Paused,           ///< stopped at a breakpoint; run() or step() continues
};

}  // namespace cppi
