#pragma once

/// @file Analyzer.hpp
/// @brief Semantic analysis: checks the player's program against the level's
/// rules and C++'s, and produces the bound program for code generation.
///
/// Wires the binders together (they talk through the AnalysisContext) and walks
/// the top-level statements in order, as script mode runs them.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "ast/TranslationUnit.hpp"
#include "sema/AnalysisResult.hpp"

#include <cppi/HostRegistry.hpp>
#include <cppi/Options.hpp>

namespace cppi::sema {

/// Entry point of semantic analysis.
class Analyzer {
public:
    /// @param host    What the host exposes to player code.
    /// @param options Standard and locked features.
    Analyzer(const HostRegistry& host, const Options& options) noexcept : host_(host), options_(options) {}

    /// `prelude`: the standard library, bound first (may be null).
    /// `unit`: the player's program.
    [[nodiscard]] AnalysisResult analyze(const ast::TranslationUnit& unit,
                                         const ast::TranslationUnit* prelude = nullptr) const;

private:
    /// What the host exposes.
    const HostRegistry& host_;
    /// Standard and locked features.
    const Options& options_;
};

}  // namespace cppi::sema
