#pragma once

/// @file Execution.hpp
/// A program in the middle of running. Lets the host advance one
/// instruction at a time, e.g. one per animation frame or debugger click.

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

class Execution {
public:
    Execution(Execution&&) noexcept;
    Execution& operator=(Execution&&) noexcept;
    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;
    ~Execution();

    /// Executes a single instruction (if still running) and returns the status.
    RunStatus step();
    /// Runs until execution reaches another line (or stops): "next line" in a debugger.
    RunStatus step_line();
    /// Runs until the program finishes, stops, or reaches a breakpoint
    /// (status Paused; call run() again to continue).
    RunResult run();

    // --- Debugging ------------------------------------------------------------
    void set_breakpoint(std::uint32_t line);
    void clear_breakpoint(std::uint32_t line);
    void clear_breakpoints();
    /// Active calls, innermost first, with their variables in scope.
    [[nodiscard]] std::vector<StackFrame> call_stack() const;
    [[nodiscard]] std::vector<Variable> globals() const;

    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] RunStatus status() const noexcept;
    [[nodiscard]] std::uint64_t operations() const noexcept;
    /// Operations spent so far on each line, by line number (see LineOperations).
    [[nodiscard]] std::vector<LineOperations> line_operations() const;
    /// Location of the next instruction to execute.
    [[nodiscard]] SourceRange current_location() const noexcept;
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;
    /// Text printed so far (std::cout).
    [[nodiscard]] const std::string& output() const noexcept;

private:
    friend class Interpreter;
    explicit Execution(std::unique_ptr<detail::Vm> vm) noexcept;

    std::unique_ptr<detail::Vm> vm_;  // Pimpl: the VM stays out of public headers
};

}  // namespace cppi
