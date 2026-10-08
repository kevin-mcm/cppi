#pragma once

/// @file FunctionContext.hpp
/// @brief What the analyzer knows about the function whose body it is binding.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/FrameAllocator.hpp"
#include "sema/TypeInfo.hpp"
#include "sema/TypeTable.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

/// The state of the function whose body is being bound.
struct FunctionContext {
    std::uint32_t function = 0;  ///< id in the function table (0 = the script)
    /// Declared (or deduced) return type.
    TypeRef return_type = TypeTable::kVoid;
    /// Its frame cells.
    FrameAllocator frame;
    std::uint32_t loop_depth = 0;         ///< `continue` is valid inside
    std::uint32_t breakable_depth = 0;    ///< `break` is valid inside (loops and switches)
    std::optional<std::uint32_t> record;  ///< member functions: `this` is the cell at offset 0
    /// A const member function: `*this` is const.
    bool const_this = false;
    bool has_this = true;                      ///< false in static member functions
    std::optional<std::uint32_t> result_slot;  ///< records returned through a hidden pointer
    bool deduce_return = false;                ///< lambdas without `-> T`: the first return decides

    /// Temporaries with destructors created by the expression being bound;
    /// the statement destroys them at the end of the full-expression.
    struct Temporary {
        /// Its first frame cell.
        std::uint32_t offset = 0;
        /// Its type.
        TypeRef type = 0;
    };
    /// Pending temporaries of the current full-expression.
    std::vector<Temporary> temporaries;
};

}  // namespace cppi::sema
