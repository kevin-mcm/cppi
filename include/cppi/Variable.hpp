#pragma once

/// @file Variable.hpp
/// A variable as a debugger shows it.

#include <string>
#include <vector>

namespace cppi {

struct Variable {
    std::string name;                ///< "hp", "[2]", "x" (field)
    std::string type;                ///< "int", "Point", "int[3]"
    std::string value;               ///< "42", "true", "East", "nullptr", "&grid[1]"; "{...}" for arrays and records
    bool initialized = true;         ///< false: reading it would be undefined behavior
    std::vector<Variable> children;  ///< array elements and fields
};

}  // namespace cppi
