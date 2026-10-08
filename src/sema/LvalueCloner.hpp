#pragma once

/// Copies simple lvalue expressions (variables, members, constant-index
/// elements, `*this`) so one target can be initialized in several steps.

#include "sema/BoundTree.hpp"

namespace cppi::sema {

class LvalueCloner {
public:
    /// A deep copy of `expr`. Only side-effect-free expressions are supported:
    /// the result is null otherwise.
    [[nodiscard]] static BExprPtr clone(const BExpr& expr);
};

}  // namespace cppi::sema
