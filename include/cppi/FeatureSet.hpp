#pragma once

/// @file FeatureSet.hpp
/// @brief A set of language features (e.g. "the features the player has not
/// unlocked yet").
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Feature.hpp>
#include <cppi/Standard.hpp>

#include <bitset>
#include <cstddef>

namespace cppi {

/// A set of Feature values, stored as a bitset. Mutators return `*this` so
/// calls can be chained: `FeatureSet{}.add(Feature::Loops).add(Feature::Lambdas)`.
class FeatureSet {
public:
    /// An empty set.
    constexpr FeatureSet() noexcept = default;

    /// Adds `f` to the set (no effect if already present).
    FeatureSet& add(Feature f) noexcept {
        bits_.set(index(f));
        return *this;
    }
    /// Removes `f` from the set (no effect if absent).
    FeatureSet& remove(Feature f) noexcept {
        bits_.reset(index(f));
        return *this;
    }
    /// True if `f` is in the set.
    [[nodiscard]] bool contains(Feature f) const noexcept { return bits_.test(index(f)); }
    /// True if the set has no features.
    [[nodiscard]] bool empty() const noexcept { return bits_.none(); }
    /// Number of features in the set.
    [[nodiscard]] std::size_t size() const noexcept { return bits_.count(); }

    /// Every feature that exists in `standard`.
    [[nodiscard]] static FeatureSet all_in(Standard standard) noexcept;

private:
    /// Bit position of `f`.
    static constexpr std::size_t index(Feature f) noexcept { return static_cast<std::size_t>(f); }
    /// One bit per feature.
    std::bitset<kFeatureCount> bits_;
};

}  // namespace cppi
