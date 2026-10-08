#pragma once

/// @file ConstEvaluator.hpp
/// @brief Evaluates constant expressions at compile time: array sizes, case
/// labels, enumerator values, static_assert conditions.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/BoundTree.hpp"
#include "sema/TypeTable.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

/// Forward declaration (see ConstexprInterpreter.hpp).
class ConstexprInterpreter;

/// Evaluates bound expressions whose value is known at compile time.
class ConstEvaluator {
public:
    /// @param types The type table (sizes and kinds of the operands).
    explicit ConstEvaluator(const TypeTable& types) noexcept : types_(types) {}

    /// A compile-time scalar: an integer (or bits) or a double.
    struct Value {
        /// `d` holds the value; otherwise `i` does.
        bool is_double = false;
        /// Integer value (also enumerators, bools and chars).
        std::int64_t i = 0;
        /// Floating-point value.
        double d = 0;
    };
    /// The cells of a constexpr function being evaluated (nullopt: not yet initialized).
    using Frame = std::vector<std::optional<std::int64_t>>;

    /// Calls to constexpr functions are evaluated by `interpreter`.
    void attach(ConstexprInterpreter* interpreter) noexcept { interpreter_ = interpreter; }

    /// The value as an integer (doubles are truncated), or nullopt if the
    /// expression is not a constant expression. Overflow makes it non-constant.
    [[nodiscard]] std::optional<std::int64_t> integer(const BExpr& expr) const;

    /// The value of `expr`; inside a constexpr function, `frame` holds its variables.
    [[nodiscard]] std::optional<Value> eval(const BExpr& expr, Frame* frame = nullptr) const;
    /// `lhs op rhs` computed in `kind` for a result of `type`; nullopt on
    /// overflow, division by zero or a bad shift (not a constant).
    [[nodiscard]] std::optional<Value> compute(BinOp op, ScalarKind kind, Value lhs, Value rhs, TypeRef type) const;
    /// `v` after conversion `conv`; nullopt if the result is not representable.
    [[nodiscard]] static std::optional<Value> convert(Value v, Conv conv);

private:
    /// Evaluates a binary expression.
    [[nodiscard]] std::optional<Value> binary(const BBinary& b, TypeRef type, Frame* frame) const;
    /// True if `value` is representable in the integer type `type`.
    [[nodiscard]] bool fits(std::int64_t value, TypeRef type) const;
    /// `lhs op rhs` on unsigned operands (wraps around).
    [[nodiscard]] static std::optional<Value> unsigned_binary(BinOp op, ScalarKind kind, std::int64_t lhs,
                                                              std::int64_t rhs);

    /// The type table.
    const TypeTable& types_;
    /// Evaluates constexpr calls; null until attach().
    ConstexprInterpreter* interpreter_ = nullptr;
};

}  // namespace cppi::sema
