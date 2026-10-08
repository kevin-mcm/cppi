#pragma once

/// @file StackFrame.hpp
/// One active function call, as a debugger shows it.

#include <cppi/SourceRange.hpp>
#include <cppi/Variable.hpp>

#include <string>
#include <vector>

namespace cppi {

struct StackFrame {
    std::string function;          ///< "<script>" for the top-level statements
    SourceRange location;          ///< where it is executing (or will continue)
    std::vector<Variable> locals;  ///< parameters and local variables in scope
};

}  // namespace cppi
