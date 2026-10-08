#pragma once

/// @file FeatureSet.hpp
/// A set of language features (e.g. "the features the player has not
/// unlocked yet").

#include <cppi/Feature.hpp>
#include <cppi/Standard.hpp>

#include <bitset>
#include <cstddef>

namespace cppi {

class FeatureSet {
public:
    constexpr FeatureSet() noexcept = default;

    FeatureSet& add(Feature f) noexcept {
        bits_.set(index(f));
        return *this;
    }
    FeatureSet& remove(Feature f) noexcept {
        bits_.reset(index(f));
        return *this;
    }
    [[nodiscard]] bool contains(Feature f) const noexcept { return bits_.test(index(f)); }
    [[nodiscard]] bool empty() const noexcept { return bits_.none(); }
    [[nodiscard]] std::size_t size() const noexcept { return bits_.count(); }

    /// Every feature that exists in `standard`.
    [[nodiscard]] static FeatureSet all_in(Standard standard) noexcept;

private:
    static constexpr std::size_t index(Feature f) noexcept { return static_cast<std::size_t>(f); }
    std::bitset<kFeatureCount> bits_;
};

}  // namespace cppi
