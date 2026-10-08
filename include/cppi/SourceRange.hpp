#pragma once

/// @file SourceRange.hpp
/// A span of the player's source code.

#include <cppi/SourceLocation.hpp>

namespace cppi {

/// A half-open range [begin, end) in the source.
struct SourceRange {
    SourceLocation begin;
    SourceLocation end;

    friend constexpr bool operator==(const SourceRange&, const SourceRange&) = default;
};

}  // namespace cppi
