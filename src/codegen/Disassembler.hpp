#pragma once

/// Renders bytecode as a human-readable listing, for debugging and teaching
/// (`cppi-run --dump-bytecode`).

#include "codegen/Instruction.hpp"
#include "codegen/ProgramData.hpp"

#include <cstddef>
#include <string>

namespace cppi::detail {

class Disassembler {
public:
    explicit Disassembler(const ProgramData& program) noexcept : program_(program) {}

    [[nodiscard]] std::string listing() const;

private:
    [[nodiscard]] std::string operand_text(const Instruction& ins) const;
    [[nodiscard]] static std::string pad_left(std::string text, std::size_t width, char fill);
    [[nodiscard]] static std::string pad_right(std::string text, std::size_t width);

    const ProgramData& program_;
};

}  // namespace cppi::detail
