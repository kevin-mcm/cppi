#pragma once

/// @file TranslationUnit.hpp
/// @brief Root of the AST: the player's whole program, in script mode.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "ast/Stmt.hpp"

#include <vector>

namespace cppi::ast {

/// The whole program: its top-level statements and declarations, in order.
struct TranslationUnit {
    /// Top-level statements and declarations, in source order.
    std::vector<Stmt> statements;
};

}  // namespace cppi::ast
