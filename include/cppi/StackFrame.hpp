#pragma once

/// @file StackFrame.hpp
/// @brief One active function call, as a debugger shows it.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/SourceRange.hpp>
#include <cppi/Variable.hpp>

#include <string>
#include <vector>

namespace cppi {

/// One entry of Execution::call_stack().
struct StackFrame {
    std::string function;          ///< "<script>" for the top-level statements
    SourceRange location;          ///< where it is executing (or will continue)
    std::vector<Variable> locals;  ///< parameters and local variables in scope
};

}  // namespace cppi
