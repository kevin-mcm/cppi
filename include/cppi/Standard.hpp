#pragma once

/// @file Standard.hpp
/// C++ language standards a level can target.

#include <cstdint>
#include <optional>
#include <string_view>

namespace cppi {

enum class Standard : std::uint8_t {
    Cpp98,
    Cpp11,
    Cpp14,
    Cpp17,
    Cpp20,
    Cpp23,
    Cpp26,
};

/// Stable, human-oriented name ("C++98", "C++11", ...).
[[nodiscard]] std::string_view to_string(Standard standard) noexcept;

/// Parses "c++98", "C++11", "cpp17" or "17". Returns nullopt if unknown.
[[nodiscard]] std::optional<Standard> parse_standard(std::string_view text) noexcept;

}  // namespace cppi
