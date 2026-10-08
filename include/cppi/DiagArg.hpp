#pragma once

/// @file DiagArg.hpp
/// A named argument of a diagnostic ("name" = "harvst").

#include <cstdint>
#include <string>
#include <variant>

namespace cppi {

using DiagValue = std::variant<std::int64_t, std::string>;

struct DiagArg {
    std::string name;
    DiagValue value;
};

}  // namespace cppi
