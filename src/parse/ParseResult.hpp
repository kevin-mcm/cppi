#pragma once

/// @file ParseResult.hpp
/// @brief Outcome of parsing: the AST, or the syntax errors that prevented it.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "ast/TranslationUnit.hpp"

#include <cppi/Diagnostic.hpp>

#include <vector>

namespace cppi::parse {

/// What Parser::parse() returns.
struct ParseResult {
    /// The AST; meaningful only when ok().
    ast::TranslationUnit unit;
    std::vector<Diagnostic> diagnostics;  ///< syntax errors only

    /// @return true if there were no syntax errors.
    [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
};

}  // namespace cppi::parse
