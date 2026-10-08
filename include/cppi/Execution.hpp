#pragma once

/// @file Execution.hpp
/// @brief A program in the middle of running.
///
/// Lets the host advance one instruction at a time, e.g. one per animation
/// frame or debugger click.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Diagnostic.hpp>
#include <cppi/LineOperations.hpp>
#include <cppi/RunResult.hpp>
#include <cppi/RunStatus.hpp>
#include <cppi/SourceRange.hpp>
#include <cppi/StackFrame.hpp>
#include <cppi/Variable.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cppi {

namespace detail {
class Vm;
}

/// A program being executed step by step, created by Interpreter::start().
/// Move-only; owns the virtual machine running the program.
class Execution {
public:
    Execution(Execution&&) noexcept;
    Execution& operator=(Execution&&) noexcept;
    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;
    ~Execution();

    /// Executes a single instruction (if still running) and returns the status.
    /// @return The status after the step.
    RunStatus step();
    /// Runs until execution reaches another line (or stops): "next line" in a debugger.
    /// @return The status once the line changed or execution stopped.
    RunStatus step_line();
    /// Runs until the program finishes, stops, or reaches a breakpoint
    /// (status Paused; call run() again to continue).
    /// @return The outcome so far: final when the program ended, Paused at a
    /// breakpoint.
    RunResult run();

    // --- Debugging ------------------------------------------------------------
    /// Pauses execution whenever it reaches `line` (1-based).
    void set_breakpoint(std::uint32_t line);
    /// Removes the breakpoint on `line`, if any.
    void clear_breakpoint(std::uint32_t line);
    /// Removes every breakpoint.
    void clear_breakpoints();
    /// Active calls, innermost first, with their variables in scope.
    [[nodiscard]] std::vector<StackFrame> call_stack() const;
    /// The program's global variables and their current values.
    [[nodiscard]] std::vector<Variable> globals() const;

    /// True once the program has ended, successfully or not.
    [[nodiscard]] bool finished() const noexcept;
    /// Current state of the execution.
    [[nodiscard]] RunStatus status() const noexcept;
    /// Operations consumed so far, as priced by the CostModel.
    [[nodiscard]] std::uint64_t operations() const noexcept;
    /// Operations spent so far on each line, by line number (see LineOperations).
    [[nodiscard]] std::vector<LineOperations> line_operations() const;
    /// Location of the next instruction to execute.
    [[nodiscard]] SourceRange current_location() const noexcept;
    /// Diagnostics reported at runtime so far.
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;
    /// Text printed so far (std::cout).
    [[nodiscard]] const std::string& output() const noexcept;

private:
    friend class Interpreter;
    /// Takes ownership of a ready-to-run virtual machine.
    explicit Execution(std::unique_ptr<detail::Vm> vm) noexcept;

    std::unique_ptr<detail::Vm> vm_;  // Pimpl: the VM stays out of public headers
};

}  // namespace cppi
