#pragma once

/// What the analyzer knows about the function whose body it is binding.

#include "sema/FrameAllocator.hpp"
#include "sema/TypeInfo.hpp"
#include "sema/TypeTable.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

struct FunctionContext {
    std::uint32_t function = 0;  ///< id in the function table (0 = the script)
    TypeRef return_type = TypeTable::kVoid;
    FrameAllocator frame;
    std::uint32_t loop_depth = 0;         ///< `continue` is valid inside
    std::uint32_t breakable_depth = 0;    ///< `break` is valid inside (loops and switches)
    std::optional<std::uint32_t> record;  ///< member functions: `this` is the cell at offset 0
    bool const_this = false;
    bool has_this = true;                      ///< false in static member functions
    std::optional<std::uint32_t> result_slot;  ///< records returned through a hidden pointer
    bool deduce_return = false;                ///< lambdas without `-> T`: the first return decides

    /// Temporaries with destructors created by the expression being bound;
    /// the statement destroys them at the end of the full-expression.
    struct Temporary {
        std::uint32_t offset = 0;
        TypeRef type = 0;
    };
    std::vector<Temporary> temporaries;
};

}  // namespace cppi::sema
