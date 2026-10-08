#pragma once

/// Root of the AST: the player's whole program, in script mode.

#include "ast/Stmt.hpp"

#include <vector>

namespace cppi::ast {

struct TranslationUnit {
    std::vector<Stmt> statements;
};

}  // namespace cppi::ast
