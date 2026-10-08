#pragma once

/// @file CodeGenerator.hpp
/// @brief Lowers the bound program to bytecode.
///
/// It cannot fail: every check already happened during semantic analysis.
///
/// Expressions are generated in one of three modes: their value (an rvalue
/// on the stack), their address (lvalues), or for their effects only.
/// Destructors of block-scoped objects run on every way out of the block:
/// falling off the end, break, continue and return.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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

/// Generates the bytecode and metadata of a bound program.
class CodeGenerator {
public:
    /// Generates code for every reachable function, then the metadata VMs and
    /// debuggers use.
    /// @param program The analyzed program (error-free).
    /// @param host    The host registry it was analyzed against.
    /// @return The compiled program data.
    [[nodiscard]] static std::shared_ptr<const ProgramData> generate(const sema::BoundProgram& program,
                                                                     std::shared_ptr<const HostRegistry> host);

private:
    /// What an expression is generated for: effects only, its value, or its address.
    enum class Mode : std::uint8_t { Discard, Value, Address };

    /// A jump to a label.
    struct Jump {
        /// The target label.
        std::uint32_t label = 0;
    };
    /// Where `break` and `continue` go inside the innermost loop or switch.
    struct BreakTarget {
        /// Label after the loop or switch.
        std::uint32_t break_label = 0;
        /// Label of the next iteration; empty for a switch.
        std::optional<std::uint32_t> continue_label;
        /// Number of cleanup scopes open outside it (run when jumping out).
        std::size_t scope_depth = 0;
    };

    /// Use generate().
    CodeGenerator(ProgramData& out, const sema::BoundProgram& program) noexcept
        : out_(out), program_(program), types_(*program.types), constants_(out.constants), evaluator_(types_) {}

    /// Generates the body of function `id`.
    void function(std::uint32_t id);
    /// Fills the type, record, function and variable tables of the output.
    void build_metadata();

    // Statements
    /// Generates one statement.
    void statement(const sema::BStmt& s);
    /// Generates a list of statements, in order.
    void statements(const std::vector<sema::BStmt>& list);
    /// Generates a block and the destructors of its objects.
    void block(const sema::BBlock& b);
    /// Generates any loop (`while`, `do`, `for`, range `for`).
    void loop(const sema::BLoop& l, SourceRange range);
    /// Generates a `switch`: comparisons and jumps to its labels.
    void switch_statement(const sema::BSwitch& s, SourceRange range);
    /// Generates a `return`, running pending cleanups first.
    void return_statement(const sema::BReturn& r, SourceRange range);
    /// Generates a `try` block, its handler and its catch table.
    void try_statement(const sema::BTry& t, SourceRange range);
    /// Runs the cleanups of the scopes above `down_to`, innermost first.
    void emit_cleanups(std::size_t down_to);

    // Expressions
    /// Generates `e` so that its value ends up on the stack.
    void value(const sema::BExpr& e);
    /// Evaluates the condition `e` and jumps to `label` if it is `when`, else
    /// falls through. `&&`, `||` and `!` become jumps instead of a bool.
    void branch(const sema::BExpr& e, bool when, std::uint32_t label, SourceRange range);
    /// A scalar expression whose value is known now (constants, `const`
    /// variables with constant initializers, and operations on them that
    /// cannot fail), written on one line: pushes it and returns true.
    bool fold(const sema::BExpr& e);
    /// The value of `e` if fold() applies to it.
    [[nodiscard]] std::optional<std::int64_t> folded(const sema::BExpr& e) const;
    /// Made only of constants and operations on them (memoized per function,
    /// so folding a long expression stays linear).
    [[nodiscard]] bool constant_shape(const sema::BExpr& e) const;
    /// Pushes the value of a variable.
    void load_variable(const sema::BVar& v, SourceRange range);
    /// Pops a value into a variable.
    void store_variable(const sema::BVar& v, SourceRange range);
    /// Generates `e` (an lvalue) so that its address ends up on the stack.
    void address(const sema::BExpr& e);
    /// Generates `e` for its side effects only.
    void discard(const sema::BExpr& e);
    /// Generates an assignment, leaving what `mode` asks for.
    void assign(const sema::BAssign& a, const sema::BExpr& e, Mode mode);
    /// Generates `++`/`--`, leaving what `mode` asks for.
    void inc_dec(const sema::BIncDec& i, const sema::BExpr& e, Mode mode);
    /// Generates a call to a player, library, virtual or host function.
    void call(const sema::BCall& c, const sema::BExpr& e);
    /// Generates a `new` expression and the constructor calls.
    void new_object(const sema::BNew& n, const sema::BExpr& e);
    /// Generates a `delete` expression and the destructor calls.
    void delete_object(const sema::BDelete& d, const sema::BExpr& e);
    /// Applies a sequence of implicit conversions to the value on the stack.
    void conversions(const std::vector<sema::Conv>& list, SourceRange range);
    /// Applies one conversion to the value on the stack.
    void conversion(sema::Conv conv, SourceRange range);

    // Emission
    /// Appends an instruction and returns its index.
    std::uint32_t emit(OpCode op, SourceRange range, std::uint32_t operand = 0, std::uint16_t argc = 0);
    /// Appends an instruction with a 24-bit `wide` field and returns its index.
    std::uint32_t emit_wide(OpCode op, SourceRange range, std::uint32_t operand, std::uint32_t wide);
    /// Pushes a constant of type `type`.
    void push_constant(std::int64_t bits, sema::TypeRef type, SourceRange range);
    /// How a constant appears in listings: "3 : int", "East : Direction".
    [[nodiscard]] std::string constant_text(std::int64_t bits, sema::TypeRef type) const;
    /// The `argc` operand kind arithmetic instructions use for `kind`.
    [[nodiscard]] std::uint16_t kind_of(sema::ScalarKind kind) const;
    /// Tracks the operand-stack depth, and its maximum.
    void adjust(long delta);
    /// A new, unbound label.
    std::uint32_t new_label();
    /// Binds `label` to the next instruction.
    void bind_label(std::uint32_t label);
    /// Emits a jump to `label`, patched once labels are bound.
    void jump(OpCode op, std::uint32_t label, SourceRange range);
    /// Writes the bound label addresses into the jumps of the current function.
    void patch();
    /// The code emitted since `start` is a statement or a condition: its first
    /// instruction is charged by CostModel::Unit::Statement (player code only).
    void count_from(std::size_t start);
    /// Peephole pass over the current function: jumps to jumps go straight
    /// to the final target, and a jump to a return returns.
    void thread_jumps(std::uint32_t entry);

    /// The program being generated.
    ProgramData& out_;
    /// The analyzed program.
    const sema::BoundProgram& program_;
    /// Its types.
    const sema::TypeTable& types_;
    /// Interns constants into `out_`.
    ConstantPool constants_;
    /// Evaluates foldable expressions.
    sema::ConstEvaluator evaluator_;
    mutable std::unordered_map<const sema::BExpr*, bool> shapes_;  ///< constant_shape(), this function
    /// Current operand-stack depth.
    long depth_ = 0;

    // Per function
    /// Id of the function being generated.
    std::uint32_t current_ = 0;
    /// Instruction index of each label (-1 while unbound).
    std::vector<std::int64_t> labels_;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> fixups_;  ///< (instruction, label)
    /// Scopes being generated whose exit runs code: their cleanup statements,
    /// and whether they registered an exception handler (popped on exit).
    struct Scope {
        /// Statements run on exit; null if none.
        const std::vector<sema::BStmt>* cleanup = nullptr;
        /// It pushed an exception handler.
        bool handler = false;
    };
    /// Open cleanup scopes, outermost first.
    std::vector<Scope> cleanups_;
    bool exceptions_ = false;  ///< some reachable code throws: blocks register cleanup pads
    /// Enclosing loops and switches, outermost first.
    std::vector<BreakTarget> targets_;
    /// Label of the function's common exit.
    std::uint32_t end_label_ = 0;
};

}  // namespace cppi::detail
