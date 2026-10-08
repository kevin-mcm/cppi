#pragma once

/// @file Options.hpp
/// Compile-time rules for a level.

#include <cppi/FeatureSet.hpp>
#include <cppi/Standard.hpp>

namespace cppi {

/// Compile-time rules: which standard is active and what is still locked.
/// Typically changes per level.
struct Options {
    Standard standard = Standard::Cpp98;
    /// Features the player has not unlocked yet.
    FeatureSet locked;
};

}  // namespace cppi
