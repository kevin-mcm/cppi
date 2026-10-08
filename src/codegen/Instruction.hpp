#pragma once

/// One bytecode instruction (Command pattern: serializable and executable
/// step by step). Instructions are 8 bytes, stored contiguously for
/// cache-friendly dispatch.

#include "codegen/OpCode.hpp"

#include <cstdint>

namespace cppi::detail {

struct Instruction {
    OpCode op = OpCode::Halt;
    std::uint8_t reserved = 0;
    std::uint16_t argc = 0;
    std::uint32_t operand = 0;

    /// 24-bit size field (`reserved` high byte, `argc` low 16 bits).
    [[nodiscard]] constexpr std::uint32_t wide() const noexcept {
        return (static_cast<std::uint32_t>(reserved) << 16) | argc;
    }
    constexpr void set_wide(std::uint32_t value) noexcept {
        reserved = static_cast<std::uint8_t>(value >> 16);
        argc = static_cast<std::uint16_t>(value & 0xFFFF);
    }
};
static_assert(sizeof(Instruction) == 8);

}  // namespace cppi::detail
