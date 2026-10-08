#pragma once

/// @file CostModel.hpp
/// Pricing of operations (Strategy: each level can charge differently).

#include <cstdint>

namespace cppi {

/// How many operations each action costs. Host functions add their own
/// cost (see FunctionBuilder::cost). See docs/architecture.md ("Operations")
/// and docs/adr/0011-statement-cost-unit.md.
struct CostModel {
    /// What the player's code is charged for.
    enum class Unit : std::uint8_t {
        /// Every bytecode instruction costs `instruction`: precise, but how many
        /// instructions a line becomes is an implementation detail.
        Instruction,
        /// Every statement the player wrote costs `statement` each time it runs,
        /// and so does every evaluation of the condition of an `if`, a loop or a
        /// `switch`: "one line, one operation". Compound statements (blocks, the
        /// `if` or loop itself) cost nothing beyond their condition, nor does the
        /// update of a `for`.
        Statement,
    };
    Unit unit = Unit::Instruction;
    std::uint32_t instruction = 1;  ///< Unit::Instruction
    std::uint32_t statement = 1;    ///< Unit::Statement
    /// Instructions inside the standard library (std::vector, std::string...),
    /// in either unit. Free by default: the player pays for the call, not for
    /// its internals.
    std::uint32_t library_instruction = 0;
};

}  // namespace cppi
