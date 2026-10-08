#pragma once

/// @file IntegerLiteralParser.hpp
/// @brief Turns the spelling of an integer literal into its value.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>
#include <optional>
#include <string_view>

namespace cppi::parse {

/// Parses the spelling of integer literals.
class IntegerLiteralParser {
public:
    /// Parses decimal, octal, hex and binary spellings; digit separators and
    /// suffixes are allowed. Returns nullopt if the value overflows uint64.
    [[nodiscard]] static std::optional<std::uint64_t> parse(std::string_view text) noexcept;
};

}  // namespace cppi::parse
