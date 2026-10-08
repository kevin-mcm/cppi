#pragma once

/// Evaluates constant expressions at compile time: array sizes, case
/// labels, enumerator values, static_assert conditions.

#include "sema/BoundTree.hpp"
#include "sema/TypeTable.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

class ConstexprInterpreter;

class ConstEvaluator {
public:
    explicit ConstEvaluator(const TypeTable& types) noexcept : types_(types) {}

    struct Value {
        bool is_double = false;
        std::int64_t i = 0;
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
    [[nodiscard]] std::optional<Value> compute(BinOp op, ScalarKind kind, Value lhs, Value rhs, TypeRef type) const;
    [[nodiscard]] static std::optional<Value> convert(Value v, Conv conv);

private:
    [[nodiscard]] std::optional<Value> binary(const BBinary& b, TypeRef type, Frame* frame) const;
    [[nodiscard]] bool fits(std::int64_t value, TypeRef type) const;
    [[nodiscard]] static std::optional<Value> unsigned_binary(BinOp op, ScalarKind kind, std::int64_t lhs,
                                                              std::int64_t rhs);

    const TypeTable& types_;
    ConstexprInterpreter* interpreter_ = nullptr;
};

}  // namespace cppi::sema
