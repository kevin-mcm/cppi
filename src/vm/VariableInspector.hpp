#pragma once

/// @file VariableInspector.hpp
/// @brief Reads variables out of a paused VM for debuggers: which ones are in
/// scope, their values (or that they are not initialized yet), the elements of
/// arrays and the fields of records.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/ProgramData.hpp"
#include "vm/Memory.hpp"

#include <cppi/StackFrame.hpp>
#include <cppi/Variable.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cppi::detail {

/// Renders the variables of a paused VM for debuggers.
class VariableInspector {
public:
    /// One active call, as the inspector needs it.
    struct FrameView {
        /// Function id.
        std::uint32_t function = 0;
        /// Address of the frame's first cell.
        std::uint32_t base = 0;
        /// Where the call is executing.
        SourceRange location;
    };

    /// `frames` innermost first.
    /// @param program The running program.
    /// @param memory  The VM's memory.
    /// @param frames  The active calls.
    VariableInspector(const ProgramData& program, const Memory& memory, std::vector<FrameView> frames) noexcept
        : program_(program), memory_(memory), frames_(std::move(frames)) {}

    /// Every active call with the locals in scope, innermost first.
    [[nodiscard]] std::vector<StackFrame> call_stack() const;
    /// The global variables.
    [[nodiscard]] std::vector<Variable> globals() const;

    /// The variable containing `address`, as "x", "grid[1]" or "p.y".
    [[nodiscard]] std::optional<std::string> describe(std::uint32_t address) const;

private:
    /// The variable `name` of type `type` at `address`, with its elements or
    /// fields down to a nesting limit (`depth` is the current level).
    [[nodiscard]] Variable render(std::string name, std::uint32_t address, std::uint32_t type, int depth) const;
    /// std::vector and std::string as their contents, like a debugger's pretty
    /// printers; false if `t` is not one of them.
    [[nodiscard]] bool render_container(Variable& v, const TypeMeta& t, std::uint32_t address, int depth) const;
    /// A scalar value as text: "42", "true", "East", "nullptr", "&grid[1]".
    [[nodiscard]] std::string scalar_text(const TypeMeta& type, std::int64_t raw) const;
    /// The path to `address` inside the object `name` of type `type` starting at
    /// `start` ("grid[1]", "p.y"), or nullopt if it lies outside.
    [[nodiscard]] std::optional<std::string> path_in(std::uint32_t address, std::uint32_t start, std::uint32_t type,
                                                     const std::string& name) const;

    /// The program.
    const ProgramData& program_;
    /// Its memory.
    const Memory& memory_;
    /// Active calls, innermost first.
    std::vector<FrameView> frames_;
};

}  // namespace cppi::detail
