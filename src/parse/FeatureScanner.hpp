#pragma once

/// Lists the language features a piece of the syntax tree uses.

#include "ast/FeatureUse.hpp"
#include "ast/Unsupported.hpp"

#include <tree_sitter/api.h>

#include <string_view>
#include <vector>

namespace cppi::parse {

class FeatureScanner {
public:
    /// Every feature used in the subtree, outermost first, once each. Used to
    /// explain a construct the converter does not understand.
    [[nodiscard]] static ast::Unsupported scan(TSNode node, std::string_view source);

    /// The features a statement uses itself: its subtree minus nested
    /// statements, which are checked on their own.
    [[nodiscard]] static std::vector<ast::FeatureUse> scan_own(TSNode statement, std::string_view source);

    /// True for node kinds that are statements of their own.
    [[nodiscard]] static bool is_statement_kind(std::string_view kind) noexcept;
};

}  // namespace cppi::parse
