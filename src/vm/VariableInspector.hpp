#pragma once

/// Reads variables out of a paused VM for debuggers: which ones are in
/// scope, their values (or that they are not initialized yet), the elements
/// of arrays and the fields of records.

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

class VariableInspector {
public:
    struct FrameView {
        std::uint32_t function = 0;
        std::uint32_t base = 0;
        SourceRange location;
    };

    /// `frames` innermost first.
    VariableInspector(const ProgramData& program, const Memory& memory, std::vector<FrameView> frames) noexcept
        : program_(program), memory_(memory), frames_(std::move(frames)) {}

    [[nodiscard]] std::vector<StackFrame> call_stack() const;
    [[nodiscard]] std::vector<Variable> globals() const;

    /// The variable containing `address`, as "x", "grid[1]" or "p.y".
    [[nodiscard]] std::optional<std::string> describe(std::uint32_t address) const;

private:
    [[nodiscard]] Variable render(std::string name, std::uint32_t address, std::uint32_t type, int depth) const;
    /// std::vector and std::string as their contents, like a debugger's pretty
    /// printers; false if `t` is not one of them.
    [[nodiscard]] bool render_container(Variable& v, const TypeMeta& t, std::uint32_t address, int depth) const;
    [[nodiscard]] std::string scalar_text(const TypeMeta& type, std::int64_t raw) const;
    [[nodiscard]] std::optional<std::string> path_in(std::uint32_t address, std::uint32_t start, std::uint32_t type,
                                                     const std::string& name) const;

    const ProgramData& program_;
    const Memory& memory_;
    std::vector<FrameView> frames_;
};

}  // namespace cppi::detail
