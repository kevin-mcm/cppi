#pragma once

/// A language feature used at some position of the player's code.

#include <cppi/Feature.hpp>
#include <cppi/SourceRange.hpp>

namespace cppi::ast {

struct FeatureUse {
    Feature feature{};
    SourceRange range;
};

}  // namespace cppi::ast
