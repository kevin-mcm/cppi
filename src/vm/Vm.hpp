#pragma once

/// The stack virtual machine. One Vm per execution; the ProgramData it runs
/// is immutable and may be shared by many Vms on many threads.
///
/// Undefined behavior the player's code triggers (overflow, division by
/// zero, reading uninitialized variables, out-of-bounds or dangling
/// pointers...) stops the program with a 5xx diagnostic instead of
/// corrupting anything: it is a game mechanic, not a crash.

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

class Vm {
public:
    struct Frame {
        std::uint32_t function = 0;
        std::uint32_t base = 0;  ///< address of the frame's first cell
        std::size_t return_pc = 0;
    };

    Vm(std::shared_ptr<const ProgramData> program, const RunOptions& options);

    RunStatus step();
    /// Runs until the next instruction belongs to another source line.
    RunStatus step_line();
    /// Runs to completion, or until a breakpoint pauses it.
    RunResult run();

    void set_breakpoint(std::uint32_t line) { breakpoints_.insert(line); }
    void clear_breakpoint(std::uint32_t line) { breakpoints_.erase(line); }
    void clear_breakpoints() { breakpoints_.clear(); }

    [[nodiscard]] RunStatus status() const noexcept { return status_; }
    [[nodiscard]] bool finished() const noexcept {
        return status_ != RunStatus::Running && status_ != RunStatus::Paused;
    }
    [[nodiscard]] std::uint64_t operations() const noexcept { return budget_.used(); }
    [[nodiscard]] std::vector<LineOperations> line_operations() const;
    [[nodiscard]] SourceRange current_location() const noexcept;
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] const std::string& output() const noexcept { return output_; }

    // For debuggers.
    [[nodiscard]] const std::vector<Frame>& frames() const noexcept { return frames_; }
    [[nodiscard]] std::vector<StackFrame> call_stack() const;
    [[nodiscard]] std::vector<Variable> globals() const;
    [[nodiscard]] const Memory& memory() const noexcept { return memory_; }
    [[nodiscard]] const ProgramData& program() const noexcept { return *program_; }
    [[nodiscard]] std::size_t pc() const noexcept { return pc_; }

private:
    [[nodiscard]] std::uint64_t cost_of(const Instruction& ins) const noexcept;
    void execute(const Instruction& ins);
    void call_host(const Instruction& ins);
    void intrinsic(const Instruction& ins);
    void write(std::string_view text);
    void call(std::uint32_t function, std::uint16_t argc);
    void call_virtual(const Instruction& ins);
    void ret(std::uint16_t argc);
    void finish();

    /// Stops with a runtime error at the current instruction.
    void fail(Diagnostic diagnostic);
    void fault(Memory::Fault f, std::int64_t pointer = 0);
    /// Name of the variable at `address`, for messages ("x", "grid[2]").
    [[nodiscard]] std::optional<std::string> variable_at(std::uint32_t address) const;
    [[nodiscard]] VariableInspector inspector() const;
    /// Where the player's code is: inside the standard library, the call site.
    [[nodiscard]] SourceRange here() const noexcept;
    /// The location of the instruction at `pc`, or, if it has none (library
    /// and implicit member functions), of the innermost call from player code.
    [[nodiscard]] SourceRange located(std::size_t pc) const noexcept;
    void arithmetic(const Instruction& ins);
    // Exceptions
    void unwind();
    void dispatch(std::uint32_t table);
    [[nodiscard]] std::optional<std::string> exception_message(std::uint32_t thrown, std::int64_t object) const;
    void unsigned_arithmetic(const Instruction& ins, std::int64_t a, std::int64_t b);
    void compare(const Instruction& ins);
    /// `a <op> b` for a comparison opcode and an operand kind (argc of Eq...Ge).
    [[nodiscard]] static bool compare_values(OpCode op, std::uint16_t kind, std::int64_t a, std::int64_t b) noexcept;

    [[nodiscard]] std::int64_t pop() noexcept {
        const std::int64_t v = stack_.back();
        stack_.pop_back();
        return v;
    }
    void push(std::int64_t v) { stack_.push_back(v); }

    std::shared_ptr<const ProgramData> program_;
    const Instruction* code_;  ///< cached program_->code.data()
    RunOptions options_;
    OperationBudget budget_;
    std::vector<std::uint64_t> line_operations_;  ///< operations spent, by line (see here())
    HostCallInvoker invoker_;
    Memory memory_;
    std::vector<std::int64_t> stack_;
    std::vector<Frame> frames_;
    std::size_t pc_ = 0;
    RunStatus status_ = RunStatus::Running;
    std::vector<Diagnostic> diagnostics_;
    std::set<std::uint32_t> breakpoints_;
    std::string output_;
    std::uint32_t last_line_ = 0;  ///< line of the last executed instruction

    /// A scope that wants to know about exceptions unwinding through it.
    struct Handler {
        std::size_t pc = 0;
        std::size_t frames = 0;  ///< frames alive in its scope
        std::size_t stack = 0;   ///< operand stack height there
    };
    struct Exception {
        std::uint32_t thrown = 0;  ///< ProgramData::thrown
        std::int64_t object = 0;   ///< the complete exception object (heap)
        std::int64_t view = 0;     ///< as the catch clause sees it (a base subobject)
        bool rethrown = false;
        SourceRange where;  ///< the throw
    };
    std::vector<Handler> handlers_;
    std::optional<Exception> in_flight_;  ///< being thrown, looking for a handler
    std::vector<Exception> caught_;       ///< being handled, innermost last
};

}  // namespace cppi::detail
