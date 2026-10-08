/// @file Feature.cpp
/// @brief Implementation of Feature.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Feature.hpp>

#include "support/FeatureTable.hpp"

namespace cppi {

Standard introduced_in(Feature feature) noexcept {
    return detail::feature_row(feature).since;
}

std::string_view feature_key(Feature feature) noexcept {
    return detail::feature_row(feature).key;
}

std::optional<Feature> parse_feature(std::string_view key) noexcept {
    // A loop rather than std::find_if: the iterator is a pointer with
    // libstdc++ but a class with libc++, and clang-tidy wants pointers spelled
    // out (readability-qualified-auto).
    for (const detail::FeatureRow& row : detail::kFeatureTable) {
        if (row.key == key) {
            return row.feature;
        }
    }
    return std::nullopt;
}

}  // namespace cppi
