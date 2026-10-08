#pragma once

/// @file FeatureGate.hpp
/// @brief Decides whether a language feature may be used here, and explains why
/// not when it can't.
///
/// The order of reasons is a design decision: needs a newer standard > locked
/// by the game > not yet implemented by this version.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "ast/FeatureUse.hpp"
#include "ast/Unsupported.hpp"
#include "support/DiagnosticSink.hpp"

#include <cppi/Feature.hpp>
#include <cppi/Options.hpp>
#include <cppi/SourceRange.hpp>

#include <vector>

namespace cppi::sema {

/// Applies the level's rules (standard and locked features) to the player's code.
class FeatureGate {
public:
    /// @param options Standard and locked features.
    /// @param sink    Where rejections are reported.
    FeatureGate(const Options& options, detail::DiagnosticSink& sink) noexcept : options_(options), sink_(sink) {}

    /// Features this version of the interpreter can execute. Anything else is
    /// reported as "not implemented yet" when the standard and the game allow it.
    [[nodiscard]] static bool is_implemented(Feature feature) noexcept;

    /// True if `feature` is usable; otherwise reports why and returns false.
    bool allow(Feature feature, SourceRange range);

    /// Reports the most useful reason an unsupported construct was rejected.
    void explain(const ast::Unsupported& construct, SourceRange range);

    /// Checks the features a statement uses itself before it is analyzed.
    /// Reports every feature that needs a newer standard or, failing that,
    /// the first locked one, and returns false if the statement must be skipped.
    bool admit(const std::vector<ast::FeatureUse>& uses);

    /// The standard library prelude is not subject to the level's rules.
    void suspend(bool suspended) noexcept { suspended_ = suspended; }
    /// True while the prelude is being bound.
    [[nodiscard]] bool suspended() const noexcept { return suspended_; }

private:
    /// True if the selected standard includes `feature`.
    [[nodiscard]] bool standard_allows(Feature feature) const noexcept;

    /// The level's rules.
    const Options& options_;
    /// Where rejections go.
    detail::DiagnosticSink& sink_;
    /// See suspend().
    bool suspended_ = false;
};

}  // namespace cppi::sema
