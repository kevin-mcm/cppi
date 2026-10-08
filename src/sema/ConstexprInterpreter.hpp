#pragma once

/// @file ConstexprInterpreter.hpp
/// @brief Runs constexpr functions at compile time, so their results can size
/// arrays, label cases or check static_asserts: `int grid[square(3)];`.
///
/// It walks the bound tree of the function with its own frame of cells.
/// Anything it cannot do at compile time (pointers, objects, calls to non-
/// constexpr functions, I/O) makes the call "not a constant".
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisResult.hpp"
#include "sema/BoundTree.hpp"
#include "sema/ConstEvaluator.hpp"
#include "sema/FunctionInfo.hpp"
#include "sema/TypeTable.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace cppi::sema {

/// A small tree-walking interpreter over bound statements, for constexpr
/// functions. Bounded in steps and depth, so a compile-time call can never
/// hang or crash the compiler.
class ConstexprInterpreter {
public:
    /// @param evaluator Evaluates expressions inside the functions.
    /// @param types     The type table.
    /// @param functions Every function of the program.
    /// @param program   The bound program (function bodies).
    ConstexprInterpreter(const ConstEvaluator& evaluator, const TypeTable& types,
                         const std::deque<FunctionInfo>& functions, const BoundProgram& program) noexcept
        : evaluator_(evaluator), types_(types), functions_(functions), program_(program) {}

    /// The result of calling constexpr function `function` with scalar
    /// arguments (as cell bits), or nullopt if it is not a constant.
    [[nodiscard]] std::optional<std::int64_t> call(std::uint32_t function, const std::vector<std::int64_t>& args);

    /// Reads a variable of the frame (`lvalue` is a local, or an element of a local array).
    [[nodiscard]] std::optional<std::int64_t> load(const BExpr& lvalue, ConstEvaluator::Frame& frame) const;
    /// Assignments, increments and commas inside a constexpr function; yields the value.
    [[nodiscard]] std::optional<std::int64_t> effect(const BExpr& expr, ConstEvaluator::Frame& frame);

    /// Statements and effects one top-level call may execute.
    static constexpr std::uint32_t kMaxSteps = 1'000'000;
    /// Nested constexpr calls before giving up (the call is then not constant, E0216).
    /// Each one takes several native frames: in a browser (and Node.js) the
    /// machine stack is about 1 MB, and 512 overflowed it.
    static constexpr std::uint32_t kMaxDepth = 256;

private:
    /// How a statement ended.
    enum class Flow : std::uint8_t { Normal, Break, Continue, Return, Fail };

    /// Executes one statement; a `return` stores its value in `result`.
    [[nodiscard]] Flow run(const BStmt& stmt, ConstEvaluator::Frame& frame, std::optional<std::int64_t>& result);
    /// Executes statements in order until one does not end normally.
    [[nodiscard]] Flow run_all(const std::vector<BStmt>& list, ConstEvaluator::Frame& frame,
                               std::optional<std::int64_t>& result);
    /// The frame cell an lvalue denotes.
    [[nodiscard]] std::optional<std::uint32_t> cell(const BExpr& lvalue, ConstEvaluator::Frame& frame) const;
    /// The value of `expr` as cell bits.
    [[nodiscard]] std::optional<std::int64_t> value(const BExpr& expr, ConstEvaluator::Frame& frame) const;
    /// Cell bits of a scalar of `type` as a Value (nullopt for pointers).
    [[nodiscard]] std::optional<ConstEvaluator::Value> as_value(std::int64_t bits, TypeRef type) const;
    /// A Value as cell bits.
    [[nodiscard]] static std::int64_t to_bits(const ConstEvaluator::Value& v);
    /// Counts one step; false once kMaxSteps is exceeded.
    [[nodiscard]] bool tick() noexcept { return ++steps_ <= kMaxSteps; }

    /// Evaluates expressions.
    const ConstEvaluator& evaluator_;
    /// The type table.
    const TypeTable& types_;
    /// Every function.
    const std::deque<FunctionInfo>& functions_;
    /// Bound bodies.
    const BoundProgram& program_;
    /// Steps taken by the current top-level call.
    std::uint32_t steps_ = 0;
    /// Current call nesting.
    std::uint32_t depth_ = 0;
};

}  // namespace cppi::sema
