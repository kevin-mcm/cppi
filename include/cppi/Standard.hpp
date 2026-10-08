#pragma once

/// @file Standard.hpp
/// @brief C++ language standards a level can target.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>
#include <optional>
#include <string_view>

namespace cppi {

/// A C++ standard, oldest first: comparisons follow publication order.
enum class Standard : std::uint8_t {
    /// C++98 (and C++03).
    Cpp98,
    /// C++11.
    Cpp11,
    /// C++14.
    Cpp14,
    /// C++17.
    Cpp17,
    /// C++20.
    Cpp20,
    /// C++23.
    Cpp23,
    /// C++26.
    Cpp26,
};

/// Stable, human-oriented name ("C++98", "C++11", ...).
[[nodiscard]] std::string_view to_string(Standard standard) noexcept;

/// Parses "c++98", "C++11", "cpp17" or "17". Returns nullopt if unknown.
[[nodiscard]] std::optional<Standard> parse_standard(std::string_view text) noexcept;

}  // namespace cppi
