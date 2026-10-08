#pragma once

/// @file Vm.hpp
/// @brief The stack virtual machine.
///
/// One Vm per execution; the ProgramData it runs is immutable and may be shared
/// by many Vms on many threads.
///
/// Undefined behavior the player's code triggers (overflow, division by
/// zero, reading uninitialized variables, out-of-bounds or dangling
/// pointers...) stops the program with a 5xx diagnostic instead of
/// corrupting anything: it is a game mechanic, not a crash.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/Instruction.hpp"
#include "codegen/ProgramData.hpp"
#include "vm/HostCallInvoker.hpp"
#include "vm/Memory.hpp"
#include "vm/OperationBudget.hpp"
#include "vm/VariableInspector.hpp"

#include <cppi/Diagnostic.hpp>
#include <cppi/RunOptions.hpp>
#include <cppi/RunResult.hpp>
#include <cppi/RunStatus.hpp>
#include <cppi/SourceRange.hpp>
#include <cppi/StackFrame.hpp>
#include <cppi/Variable.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cppi::detail {

/// Executes a program's bytecode: one instance per run, owning the run's
/// memory, operand stack, call frames and budget.
class Vm {
public:
    /// One active call.
    struct Frame {
        /// Function id.
        std::uint32_t function = 0;
        std::uint32_t base = 0;  ///< address of the frame's first cell
        /// Where execution continues after the call returns.
        std::size_t return_pc = 0;
    };

    /// Prepares a run of `program`, starting at the script.
    Vm(std::shared_ptr<const ProgramData> program, const RunOptions& options);

    /// Executes one instruction (resuming a paused run) and returns the status.
    RunStatus step();
    /// Runs until the next instruction belongs to another source line.
    RunStatus step_line();
    /// Runs to completion, or until a breakpoint pauses it.
    RunResult run();

    /// Pauses before executing any instruction of `line`.
    void set_breakpoint(std::uint32_t line) { breakpoints_.insert(line); }
    /// Removes the breakpoint on `line`.
    void clear_breakpoint(std::uint32_t line) { breakpoints_.erase(line); }
    /// Removes every breakpoint.
    void clear_breakpoints() { breakpoints_.clear(); }

    /// Current state.
    [[nodiscard]] RunStatus status() const noexcept { return status_; }
    /// True once the run has ended (completed or stopped).
    [[nodiscard]] bool finished() const noexcept {
        return status_ != RunStatus::Running && status_ != RunStatus::Paused;
    }
    /// Operations consumed so far.
    [[nodiscard]] std::uint64_t operations() const noexcept { return budget_.used(); }
    /// Operations spent so far on each line that spent any.
    [[nodiscard]] std::vector<LineOperations> line_operations() const;
    /// Location of the next instruction, as the player sees it (see here()).
    [[nodiscard]] SourceRange current_location() const noexcept;
    /// Runtime diagnostics so far.
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
    /// Text printed so far.
    [[nodiscard]] const std::string& output() const noexcept { return output_; }

    // For debuggers.
    /// Active calls, outermost first.
    [[nodiscard]] const std::vector<Frame>& frames() const noexcept { return frames_; }
    /// Active calls with their variables, innermost first.
    [[nodiscard]] std::vector<StackFrame> call_stack() const;
    /// Global variables and their values.
    [[nodiscard]] std::vector<Variable> globals() const;
    /// The run's memory.
    [[nodiscard]] const Memory& memory() const noexcept { return memory_; }
    /// The program being run.
    [[nodiscard]] const ProgramData& program() const noexcept { return *program_; }
    /// Index of the next instruction.
    [[nodiscard]] std::size_t pc() const noexcept { return pc_; }

private:
    /// Operations the instruction at pc_ is charged, per the CostModel.
    [[nodiscard]] std::uint64_t cost_of(const Instruction& ins) const noexcept;
    /// Executes one instruction.
    void execute(const Instruction& ins);
    /// Calls a host function with arguments from the stack.
    void call_host(const Instruction& ins);
    /// Runs a native helper of the standard library prelude.
    void intrinsic(const Instruction& ins);
    /// Appends `text` to the output and notifies the observer.
    void write(std::string_view text);
    /// Calls player function `function` with `argc` arguments from the stack.
    void call(std::uint32_t function, std::uint16_t argc);
    /// Calls the final overrider of a virtual function.
    void call_virtual(const Instruction& ins);
    /// Returns from the current function (with a value if `argc` is 1).
    void ret(std::uint16_t argc);
    /// Ends the run successfully, reporting leaked memory.
    void finish();

    /// Stops with a runtime error at the current instruction.
    void fail(Diagnostic diagnostic);
    /// Stops with the diagnostic matching memory fault `f` (None: nothing).
    void fault(Memory::Fault f, std::int64_t pointer = 0);
    /// Name of the variable at `address`, for messages ("x", "grid[2]").
    [[nodiscard]] std::optional<std::string> variable_at(std::uint32_t address) const;
    /// An inspector over the current frames.
    [[nodiscard]] VariableInspector inspector() const;
    /// Where the player's code is: inside the standard library, the call site.
    [[nodiscard]] SourceRange here() const noexcept;
    /// The location of the instruction at `pc`, or, if it has none (library
    /// and implicit member functions), of the innermost call from player code.
    [[nodiscard]] SourceRange located(std::size_t pc) const noexcept;
    /// Executes an arithmetic instruction, checking for undefined behavior.
    void arithmetic(const Instruction& ins);
    // Exceptions
    /// Transfers control to the innermost exception handler, or stops with an
    /// uncaught exception.
    void unwind();
    /// Jumps to the clause of catch table `table` that takes the in-flight
    /// exception, or keeps unwinding.
    void dispatch(std::uint32_t table);
    /// The what() message of a std::exception object, if it is one.
    [[nodiscard]] std::optional<std::string> exception_message(std::uint32_t thrown, std::int64_t object) const;
    /// Arithmetic on unsigned operands (wraps around).
    void unsigned_arithmetic(const Instruction& ins, std::int64_t a, std::int64_t b);
    /// [a b] -> [a <op> b] for a comparison instruction.
    void compare(const Instruction& ins);
    /// `a <op> b` for a comparison opcode and an operand kind (argc of Eq...Ge).
    [[nodiscard]] static bool compare_values(OpCode op, std::uint16_t kind, std::int64_t a, std::int64_t b) noexcept;

    /// Pops the top of the operand stack.
    [[nodiscard]] std::int64_t pop() noexcept {
        const std::int64_t v = stack_.back();
        stack_.pop_back();
        return v;
    }
    /// Pushes onto the operand stack.
    void push(std::int64_t v) { stack_.push_back(v); }

    /// The program.
    std::shared_ptr<const ProgramData> program_;
    const Instruction* code_;  ///< cached program_->code.data()
    /// Budget, cost model and observer.
    RunOptions options_;
    /// Operations left.
    OperationBudget budget_;
    std::vector<std::uint64_t> line_operations_;  ///< operations spent, by line (see here())
    /// Calls host functions.
    HostCallInvoker invoker_;
    /// Globals, frames and heap.
    Memory memory_;
    /// The operand stack.
    std::vector<std::int64_t> stack_;
    /// Active calls, outermost first.
    std::vector<Frame> frames_;
    /// Index of the next instruction.
    std::size_t pc_ = 0;
    /// Current state.
    RunStatus status_ = RunStatus::Running;
    /// Runtime diagnostics.
    std::vector<Diagnostic> diagnostics_;
    /// Lines with a breakpoint.
    std::set<std::uint32_t> breakpoints_;
    /// Text printed.
    std::string output_;
    std::uint32_t last_line_ = 0;  ///< line of the last executed instruction

    /// A scope that wants to know about exceptions unwinding through it.
    struct Handler {
        /// Its first instruction.
        std::size_t pc = 0;
        std::size_t frames = 0;  ///< frames alive in its scope
        std::size_t stack = 0;   ///< operand stack height there
    };
    /// An exception being thrown or handled.
    struct Exception {
        std::uint32_t thrown = 0;  ///< ProgramData::thrown
        std::int64_t object = 0;   ///< the complete exception object (heap)
        std::int64_t view = 0;     ///< as the catch clause sees it (a base subobject)
        /// Rethrown by `throw;`: its handler does not destroy it.
        bool rethrown = false;
        SourceRange where;  ///< the throw
    };
    /// Handlers in scope, innermost last.
    std::vector<Handler> handlers_;
    std::optional<Exception> in_flight_;  ///< being thrown, looking for a handler
    std::vector<Exception> caught_;       ///< being handled, innermost last
};

}  // namespace cppi::detail
