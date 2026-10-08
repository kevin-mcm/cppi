#pragma once

/// Semantic analysis: checks the player's program against the level's rules
/// and C++'s, and produces the bound program for code generation. Wires the
/// binders together (they talk through the AnalysisContext) and walks the
/// top-level statements in order, as script mode runs them.

#include "ast/TranslationUnit.hpp"
#include "sema/AnalysisResult.hpp"

#include <cppi/HostRegistry.hpp>
#include <cppi/Options.hpp>

namespace cppi::sema {

class Analyzer {
public:
    Analyzer(const HostRegistry& host, const Options& options) noexcept : host_(host), options_(options) {}

    /// `prelude`: the standard library, bound first (may be null).
    [[nodiscard]] AnalysisResult analyze(const ast::TranslationUnit& unit,
                                         const ast::TranslationUnit* prelude = nullptr) const;

private:
    const HostRegistry& host_;
    const Options& options_;
};

}  // namespace cppi::sema
