#pragma once

/// @file SourceLocation.hpp
/// A position inside the player's source code.

#include <cstdint>

namespace cppi {

/// A position in the source. Both fields are 1-based; columns count UTF-8
/// code units (bytes), which is what most code editors expose as well.
struct SourceLocation {
    std::uint32_t line = 0;
    std::uint32_t column = 0;

    friend constexpr bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

}  // namespace cppi
