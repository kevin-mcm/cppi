#pragma once

/// Lowers the bound program to bytecode. It cannot fail: every check already
/// happened during semantic analysis.
///
/// Expressions are generated in one of three modes: their value (an rvalue
/// on the stack), their address (lvalues), or for their effects only.
/// Destructors of block-scoped objects run on every way out of the block:
/// falling off the end, break, continue and return.

#include "codegen/ConstantPool.hpp"
#include "codegen/OpCode.hpp"
#include "codegen/ProgramData.hpp"
#include "sema/AnalysisResult.hpp"
#include "sema/BoundTree.hpp"
#include "sema/ConstEvaluator.hpp"

#include <cppi/HostRegistry.hpp>
#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppi::detail {

class CodeGenerator {
public:
    [[nodiscard]] static std::shared_ptr<const ProgramData> generate(const sema::BoundProgram& program,
                                                                     std::shared_ptr<const HostRegistry> host);

private:
    enum class Mode : std::uint8_t { Discard, Value, Address };

    struct Jump {
        std::uint32_t label = 0;
    };
    struct BreakTarget {
        std::uint32_t break_label = 0;
        std::optional<std::uint32_t> continue_label;
        std::size_t scope_depth = 0;
    };

    CodeGenerator(ProgramData& out, const sema::BoundProgram& program) noexcept
        : out_(out), program_(program), types_(*program.types), constants_(out.constants), evaluator_(types_) {}

    void function(std::uint32_t id);
    void build_metadata();

    // Statements
    void statement(const sema::BStmt& s);
    void statements(const std::vector<sema::BStmt>& list);
    void block(const sema::BBlock& b);
    void loop(const sema::BLoop& l, SourceRange range);
    void switch_statement(const sema::BSwitch& s, SourceRange range);
    void return_statement(const sema::BReturn& r, SourceRange range);
    void try_statement(const sema::BTry& t, SourceRange range);
    void emit_cleanups(std::size_t down_to);

    // Expressions
    void value(const sema::BExpr& e);
    /// Evaluates the condition `e` and jumps to `label` if it is `when`, else
    /// falls through. `&&`, `||` and `!` become jumps instead of a bool.
    void branch(const sema::BExpr& e, bool when, std::uint32_t label, SourceRange range);
    /// A scalar expression whose value is known now (constants, `const`
    /// variables with constant initializers, and operations on them that
    /// cannot fail), written on one line: pushes it and returns true.
    bool fold(const sema::BExpr& e);
    [[nodiscard]] std::optional<std::int64_t> folded(const sema::BExpr& e) const;
    /// Made only of constants and operations on them (memoized per function,
    /// so folding a long expression stays linear).
    [[nodiscard]] bool constant_shape(const sema::BExpr& e) const;
    void load_variable(const sema::BVar& v, SourceRange range);
    void store_variable(const sema::BVar& v, SourceRange range);
    void address(const sema::BExpr& e);
    void discard(const sema::BExpr& e);
    void assign(const sema::BAssign& a, const sema::BExpr& e, Mode mode);
    void inc_dec(const sema::BIncDec& i, const sema::BExpr& e, Mode mode);
    void call(const sema::BCall& c, const sema::BExpr& e);
    void new_object(const sema::BNew& n, const sema::BExpr& e);
    void delete_object(const sema::BDelete& d, const sema::BExpr& e);
    void conversions(const std::vector<sema::Conv>& list, SourceRange range);
    void conversion(sema::Conv conv, SourceRange range);

    // Emission
    std::uint32_t emit(OpCode op, SourceRange range, std::uint32_t operand = 0, std::uint16_t argc = 0);
    std::uint32_t emit_wide(OpCode op, SourceRange range, std::uint32_t operand, std::uint32_t wide);
    void push_constant(std::int64_t bits, sema::TypeRef type, SourceRange range);
    [[nodiscard]] std::string constant_text(std::int64_t bits, sema::TypeRef type) const;
    [[nodiscard]] std::uint16_t kind_of(sema::ScalarKind kind) const;
    void adjust(long delta);
    std::uint32_t new_label();
    void bind_label(std::uint32_t label);
    void jump(OpCode op, std::uint32_t label, SourceRange range);
    void patch();
    /// The code emitted since `start` is a statement or a condition: its first
    /// instruction is charged by CostModel::Unit::Statement (player code only).
    void count_from(std::size_t start);
    /// Peephole pass over the current function: jumps to jumps go straight
    /// to the final target, and a jump to a return returns.
    void thread_jumps(std::uint32_t entry);

    ProgramData& out_;
    const sema::BoundProgram& program_;
    const sema::TypeTable& types_;
    ConstantPool constants_;
    sema::ConstEvaluator evaluator_;
    mutable std::unordered_map<const sema::BExpr*, bool> shapes_;  ///< constant_shape(), this function
    long depth_ = 0;

    // Per function
    std::uint32_t current_ = 0;
    std::vector<std::int64_t> labels_;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> fixups_;  ///< (instruction, label)
    /// Scopes being generated whose exit runs code: their cleanup statements,
    /// and whether they registered an exception handler (popped on exit).
    struct Scope {
        const std::vector<sema::BStmt>* cleanup = nullptr;
        bool handler = false;
    };
    std::vector<Scope> cleanups_;
    bool exceptions_ = false;  ///< some reachable code throws: blocks register cleanup pads
    std::vector<BreakTarget> targets_;
    std::uint32_t end_label_ = 0;
};

}  // namespace cppi::detail
