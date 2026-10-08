#pragma once

/// Outcome of parsing: the AST, or the syntax errors that prevented it.

#include "ast/TranslationUnit.hpp"

#include <cppi/Diagnostic.hpp>

#include <vector>

namespace cppi::parse {

struct ParseResult {
    ast::TranslationUnit unit;
    std::vector<Diagnostic> diagnostics;  ///< syntax errors only

    [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
};

}  // namespace cppi::parse
