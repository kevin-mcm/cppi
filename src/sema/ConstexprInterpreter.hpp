#pragma once

/// Runs constexpr functions at compile time, so their results can size
/// arrays, label cases or check static_asserts: `int grid[square(3)];`.
/// It walks the bound tree of the function with its own frame of cells.
/// Anything it cannot do at compile time (pointers, objects, calls to
/// non-constexpr functions, I/O) makes the call "not a constant".

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

class ConstexprInterpreter {
public:
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

    static constexpr std::uint32_t kMaxSteps = 1'000'000;
    /// Nested constexpr calls before giving up (the call is then not constant, E0216).
    /// Each one takes several native frames: in a browser (and Node.js) the
    /// machine stack is about 1 MB, and 512 overflowed it.
    static constexpr std::uint32_t kMaxDepth = 256;

private:
    enum class Flow : std::uint8_t { Normal, Break, Continue, Return, Fail };

    [[nodiscard]] Flow run(const BStmt& stmt, ConstEvaluator::Frame& frame, std::optional<std::int64_t>& result);
    [[nodiscard]] Flow run_all(const std::vector<BStmt>& list, ConstEvaluator::Frame& frame,
                               std::optional<std::int64_t>& result);
    /// The frame cell an lvalue denotes.
    [[nodiscard]] std::optional<std::uint32_t> cell(const BExpr& lvalue, ConstEvaluator::Frame& frame) const;
    [[nodiscard]] std::optional<std::int64_t> value(const BExpr& expr, ConstEvaluator::Frame& frame) const;
    [[nodiscard]] std::optional<ConstEvaluator::Value> as_value(std::int64_t bits, TypeRef type) const;
    [[nodiscard]] static std::int64_t to_bits(const ConstEvaluator::Value& v);
    [[nodiscard]] bool tick() noexcept { return ++steps_ <= kMaxSteps; }

    const ConstEvaluator& evaluator_;
    const TypeTable& types_;
    const std::deque<FunctionInfo>& functions_;
    const BoundProgram& program_;
    std::uint32_t steps_ = 0;
    std::uint32_t depth_ = 0;
};

}  // namespace cppi::sema
