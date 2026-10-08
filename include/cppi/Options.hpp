#pragma once

/// @file Options.hpp
/// @brief Compile-time rules for a level.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/FeatureSet.hpp>
#include <cppi/Standard.hpp>

namespace cppi {

/// Compile-time rules: which standard is active and what is still locked.
/// Typically changes per level.
struct Options {
    /// The C++ standard the level targets.
    Standard standard = Standard::Cpp98;
    /// Features the player has not unlocked yet.
    FeatureSet locked;
};

}  // namespace cppi
