#pragma once

/// @file FeatureUse.hpp
/// @brief A language feature used at some position of the player's code.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Feature.hpp>
#include <cppi/SourceRange.hpp>

namespace cppi::ast {

/// A language feature and where the player's code uses it.
struct FeatureUse {
    /// The feature used.
    Feature feature{};
    /// Where it is used.
    SourceRange range;
};

}  // namespace cppi::ast
