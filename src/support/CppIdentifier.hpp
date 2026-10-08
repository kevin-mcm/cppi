#pragma once

/// @file CppIdentifier.hpp
/// @brief C++ naming rules, used to validate the names a host registers.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <string_view>

namespace cppi::detail {

/// Checks names against C++'s rules for identifiers.
class CppIdentifier {
public:
    /// Letters, digits and '_', not starting with a digit.
    [[nodiscard]] static bool is_well_formed(std::string_view name) noexcept;
    /// Any keyword up to C++26, alternative tokens and contextual keywords.
    [[nodiscard]] static bool is_keyword(std::string_view name) noexcept;
    /// Well formed, not a keyword and not reserved (`__x`).
    [[nodiscard]] static bool is_valid(std::string_view name) noexcept;
};

}  // namespace cppi::detail
