#pragma once

/// @file SourceRange.hpp
/// @brief A span of the player's source code.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/SourceLocation.hpp>

namespace cppi {

/// A half-open range [begin, end) in the source.
struct SourceRange {
    /// First position of the range.
    SourceLocation begin;
    /// Position just past the range.
    SourceLocation end;

    friend constexpr bool operator==(const SourceRange&, const SourceRange&) = default;
};

}  // namespace cppi
