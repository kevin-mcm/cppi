#pragma once

/// A construct the parser recognized but the current phase cannot express in
/// the AST yet. It carries every language feature found in the subtree, so
/// semantic analysis can tell the player *why* it is not accepted.

#include "ast/FeatureUse.hpp"

#include <string>
#include <vector>

namespace cppi::ast {

struct Unsupported {
    std::string node_kind;         ///< tree-sitter node type, for diagnostics
    std::vector<FeatureUse> uses;  ///< outermost first, one entry per feature
};

}  // namespace cppi::ast
