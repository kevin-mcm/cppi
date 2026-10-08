#pragma once

/// @file Variable.hpp
/// @brief A variable as a debugger shows it.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <string>
#include <vector>

namespace cppi {

/// A variable as a debugger shows it: name, type and value already rendered
/// as text, plus its elements or fields.
struct Variable {
    std::string name;                ///< "hp", "[2]", "x" (field)
    std::string type;                ///< "int", "Point", "int[3]"
    std::string value;               ///< "42", "true", "East", "nullptr", "&grid[1]"; "{...}" for arrays and records
    bool initialized = true;         ///< false: reading it would be undefined behavior
    std::vector<Variable> children;  ///< array elements and fields
};

}  // namespace cppi
