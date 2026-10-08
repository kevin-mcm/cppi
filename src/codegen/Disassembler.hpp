#pragma once

/// @file Disassembler.hpp
/// @brief Renders bytecode as a human-readable listing, for debugging and
/// teaching (`cppi-run --dump-bytecode`).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/Instruction.hpp"
#include "codegen/ProgramData.hpp"

#include <cstddef>
#include <string>

namespace cppi::detail {

/// Writes a program's bytecode as text, one instruction per line, with its
/// source line and decoded operands.
class Disassembler {
public:
    /// @param program The program to list; must outlive the disassembler.
    explicit Disassembler(const ProgramData& program) noexcept : program_(program) {}

    /// The whole listing: every function, then its instructions.
    [[nodiscard]] std::string listing() const;

private:
    /// The operands of `ins`, decoded for its opcode.
    [[nodiscard]] std::string operand_text(const Instruction& ins) const;
    /// `text` right-aligned in `width` columns, padded with `fill`.
    [[nodiscard]] static std::string pad_left(std::string text, std::size_t width, char fill);
    /// `text` left-aligned in `width` columns.
    [[nodiscard]] static std::string pad_right(std::string text, std::size_t width);

    const ProgramData& program_;
};

}  // namespace cppi::detail
