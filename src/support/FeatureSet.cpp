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
