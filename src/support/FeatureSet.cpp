/// @file FeatureSet.cpp
/// @brief Implementation of FeatureSet.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/FeatureSet.hpp>

#include "support/FeatureTable.hpp"

namespace cppi {

FeatureSet FeatureSet::all_in(Standard standard) noexcept {
    FeatureSet set;
    for (const auto& r : detail::kFeatureTable) {
        if (r.since <= standard) {
            set.add(r.feature);
        }
    }
    return set;
}

}  // namespace cppi
