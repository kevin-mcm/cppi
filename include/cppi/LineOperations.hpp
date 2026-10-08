#pragma once

/// @file LineOperations.hpp
/// Operations spent on one line of the player's code (a line profile).

#include <cstdint>

namespace cppi {

/// What one source line has cost so far. Operations spent inside the
/// standard library, in member functions the compiler writes (implicit
/// copies) and in host functions count for the player's line that called
/// them; line 0 collects what no line of the player's caused.
struct LineOperations {
    std::uint32_t line = 0;
    std::uint64_t operations = 0;

    friend bool operator==(const LineOperations&, const LineOperations&) = default;
};

}  // namespace cppi
