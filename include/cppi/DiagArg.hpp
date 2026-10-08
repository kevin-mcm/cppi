#pragma once

/// @file DiagArg.hpp
/// @brief A named argument of a diagnostic ("name" = "harvst").
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>
#include <string>
#include <variant>

namespace cppi {

/// The value of a diagnostic argument: an integer or a text.
using DiagValue = std::variant<std::int64_t, std::string>;

/// A named argument of a diagnostic, e.g. `"name" = "harvst"`.
struct DiagArg {
    /// Argument name, as documented for each DiagCode.
    std::string name;
    /// Argument value.
    DiagValue value;
};

}  // namespace cppi
