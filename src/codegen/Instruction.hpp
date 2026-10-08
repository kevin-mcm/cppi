#pragma once

/// @file Instruction.hpp
/// @brief One bytecode instruction (Command pattern: serializable and
/// executable step by step).
///
/// Instructions are 8 bytes, stored contiguously for cache-friendly dispatch.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/OpCode.hpp"

#include <cstdint>

namespace cppi::detail {

/// One instruction: an opcode plus up to three operand fields.
struct Instruction {
    /// The operation.
    OpCode op = OpCode::Halt;
    /// Opcode-specific flags; high byte of wide().
    std::uint8_t reserved = 0;
    /// Argument count or operand kind; low 16 bits of wide().
    std::uint16_t argc = 0;
    /// Main operand: an index, an address or a jump target.
    std::uint32_t operand = 0;

    /// 24-bit size field (`reserved` high byte, `argc` low 16 bits).
    [[nodiscard]] constexpr std::uint32_t wide() const noexcept {
        return (static_cast<std::uint32_t>(reserved) << 16) | argc;
    }
    /// Stores a 24-bit size in `reserved` and `argc`.
    constexpr void set_wide(std::uint32_t value) noexcept {
        reserved = static_cast<std::uint8_t>(value >> 16);
        argc = static_cast<std::uint16_t>(value & 0xFFFF);
    }
};
static_assert(sizeof(Instruction) == 8);

}  // namespace cppi::detail
