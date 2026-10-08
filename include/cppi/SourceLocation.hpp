#pragma once

/// @file SourceLocation.hpp
/// @brief A position inside the player's source code.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>

namespace cppi {

/// A position in the source. Both fields are 1-based; columns count UTF-8
/// code units (bytes), which is what most code editors expose as well.
struct SourceLocation {
    /// Line number, 1-based (0 when unknown).
    std::uint32_t line = 0;
    /// Column, 1-based, in bytes (0 when unknown).
    std::uint32_t column = 0;

    friend constexpr bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

}  // namespace cppi
