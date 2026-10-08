#pragma once

/// @file RunStatus.hpp
/// @brief State of a running or finished program.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>

namespace cppi {

/// State of an execution.
enum class RunStatus : std::uint8_t {
    Running,          ///< only seen while stepping
    Completed,        ///< reached the end of the program
    BudgetExhausted,  ///< the next instruction would exceed the budget
    HostError,        ///< a host function called HostCall::fail()
    RuntimeError,     ///< undefined behavior (5xx) or a resource limit (stack, memory)
    Paused,           ///< stopped at a breakpoint; run() or step() continues
};

}  // namespace cppi
