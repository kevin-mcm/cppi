/// @file FeatureGate.cpp
/// @brief Implementation of FeatureGate.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/FeatureGate.hpp"

#include "support/DiagnosticFactory.hpp"

#include <algorithm>

namespace cppi::sema {

using detail::DiagnosticFactory;

bool FeatureGate::is_implemented(Feature feature) noexcept {
    switch (feature) {
        case Feature::Strings:
        case Feature::Preprocessor:
        case Feature::Coroutines:
        case Feature::Modules: return false;
        default: return true;
    }
}

bool FeatureGate::standard_allows(Feature feature) const noexcept {
    return introduced_in(feature) <= options_.standard;
}

bool FeatureGate::allow(Feature feature, SourceRange range) {
    if (suspended_) {
        return true;
    }
    if (!standard_allows(feature)) {
        sink_.report(DiagnosticFactory::feature_requires_standard(range, feature, options_.standard));
        return false;
    }
    if (options_.locked.contains(feature)) {
        sink_.report(DiagnosticFactory::feature_locked(range, feature));
        return false;
    }
    return true;
}

bool FeatureGate::admit(const std::vector<ast::FeatureUse>& uses) {
    if (suspended_) {
        return true;
    }
    bool rejected = false;
    for (const auto& use : uses) {
        if (!standard_allows(use.feature)) {
            sink_.report(DiagnosticFactory::feature_requires_standard(use.range, use.feature, options_.standard));
            rejected = true;
        }
    }
    if (rejected) {
        return false;
    }
    const auto locked =
        std::ranges::find_if(uses, [&](const ast::FeatureUse& use) { return options_.locked.contains(use.feature); });
    if (locked != uses.end()) {
        sink_.report(DiagnosticFactory::feature_locked(locked->range, locked->feature));
        return false;
    }
    return true;
}

void FeatureGate::explain(const ast::Unsupported& construct, SourceRange range) {
    if (construct.uses.empty()) {
        sink_.report(DiagnosticFactory::unsupported_syntax(range, construct.node_kind));
        return;
    }
    bool reported = false;
    for (const auto& use : construct.uses) {
        if (!standard_allows(use.feature)) {
            allow(use.feature, use.range);
            reported = true;
        }
    }
    if (reported) {
        return;
    }
    for (const auto& use : construct.uses) {
        if (options_.locked.contains(use.feature)) {
            allow(use.feature, use.range);
            return;
        }
    }
    for (const auto& use : construct.uses) {
        if (!is_implemented(use.feature)) {
            sink_.report(DiagnosticFactory::feature_not_implemented(use.range, use.feature));
            return;
        }
    }
    sink_.report(DiagnosticFactory::unsupported_syntax(range, construct.node_kind));
}

}  // namespace cppi::sema
