#pragma once

/// The conversions C++ applies on its own: arithmetic promotions and
/// conversions, boolean conversions, null pointers, derived-to-base pointers
/// and objects. Ranks follow overload resolution: exact match beats
/// promotion beats conversion.

#include "sema/BoundTree.hpp"
#include "sema/ClassHierarchy.hpp"
#include "sema/TypeTable.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

/// Exact match beats promotion beats a standard conversion beats a
/// user-defined conversion (a converting constructor).
enum class ConversionRank : std::uint8_t { Exact, Promotion, Conversion, UserDefined, None };

class ImplicitConversions {
public:
    ImplicitConversions(TypeTable& types, const ClassHierarchy& hierarchy) noexcept
        : types_(types), hierarchy_(hierarchy) {}

    /// How well the rvalue `from` converts to `to`.
    [[nodiscard]] ConversionRank rank(const BExpr& from, TypeRef to) const;

    /// Converts the rvalue `expr` to `to`. Requires rank(expr, to) != None.
    /// Constants are folded.
    [[nodiscard]] BExprPtr convert(BExprPtr expr, TypeRef to) const;

    /// Explicit conversions (casts): everything implicit, plus int -> enum,
    /// enum class -> int, and between pointers to related classes.
    [[nodiscard]] bool can_cast(const BExpr& from, TypeRef to) const;
    [[nodiscard]] BExprPtr cast(BExprPtr expr, TypeRef to) const;

    /// bool, char and unscoped enums promote to int.
    [[nodiscard]] TypeRef promoted(TypeRef type) const;
    /// The type both operands of an arithmetic operator convert to.
    [[nodiscard]] TypeRef common_arithmetic(TypeRef a, TypeRef b) const;

    /// The VM conversions that take an arithmetic value of type `from` to `to`.
    [[nodiscard]] std::vector<Conv> steps(TypeRef from, TypeRef to) const;

    [[nodiscard]] static bool is_null_pointer_constant(const BExpr& expr, const TypeTable& types);
    [[nodiscard]] static ScalarKind scalar_kind(const TypeTable& types, TypeRef type);

private:
    [[nodiscard]] BExprPtr arithmetic(BExprPtr expr, TypeRef to) const;
    [[nodiscard]] BExprPtr runtime_arithmetic(BExprPtr expr, TypeRef to) const;
    [[nodiscard]] static BExprPtr wrap(BExprPtr expr, Conv conv, TypeRef to);

    TypeTable& types_;
    const ClassHierarchy& hierarchy_;
};

}  // namespace cppi::sema
