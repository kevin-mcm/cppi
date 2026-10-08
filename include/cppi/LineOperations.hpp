#pragma once

/// @file LineOperations.hpp
/// @brief Operations spent on one line of the player's code (a line profile).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>

namespace cppi {

/// What one source line has cost so far. Operations spent inside the
/// standard library, in member functions the compiler writes (implicit
/// copies) and in host functions count for the player's line that called
/// them; line 0 collects what no line of the player's caused.
struct LineOperations {
    /// 1-based line number; 0 for operations no line of the player caused.
    std::uint32_t line = 0;
    /// Operations spent on that line.
    std::uint64_t operations = 0;

    friend bool operator==(const LineOperations&, const LineOperations&) = default;
};

}  // namespace cppi
