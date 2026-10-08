#pragma once

/// @file LvalueCloner.hpp
/// @brief Copies simple lvalue expressions (variables, members, constant-index
/// elements, `*this`) so one target can be initialized in several steps.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/BoundTree.hpp"

namespace cppi::sema {

/// Duplicates side-effect-free lvalue expressions.
class LvalueCloner {
public:
    /// A deep copy of `expr`. Only side-effect-free expressions are supported:
    /// the result is null otherwise.
    [[nodiscard]] static BExprPtr clone(const BExpr& expr);
};

}  // namespace cppi::sema
