#pragma once

/// C++ naming rules, used to validate the names a host registers.

#include <string_view>

namespace cppi::detail {

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
