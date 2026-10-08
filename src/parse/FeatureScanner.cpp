#include "parse/FeatureScanner.hpp"

#include "parse/NodeFeatureMap.hpp"
#include "parse/TreeSitterUtils.hpp"

#include <algorithm>
#include <iterator>
#include <string>

namespace cppi::parse {

namespace {

void add_use(std::vector<ast::FeatureUse>& uses, Feature feature, SourceRange range) {
    const bool seen =
        std::any_of(uses.begin(), uses.end(), [feature](const ast::FeatureUse& u) { return u.feature == feature; });
    if (!seen) {
        uses.push_back(ast::FeatureUse{feature, range});
    }
}

}  // namespace

bool FeatureScanner::is_statement_kind(std::string_view kind) noexcept {
    constexpr std::string_view kinds[] = {
        "expression_statement", "compound_statement", "if_statement",     "while_statement",
        "do_statement",         "for_statement",      "for_range_loop",   "return_statement",
        "break_statement",      "continue_statement", "switch_statement", "case_statement",
        "labeled_statement",    "goto_statement",     "try_statement",    "throw_statement",
    };
    return std::find(std::begin(kinds), std::end(kinds), kind) != std::end(kinds);
}

ast::Unsupported FeatureScanner::scan(TSNode node, std::string_view source) {
    ast::Unsupported result;
    result.node_kind = std::string(kind_of(node));
    walk(node, [&](TSNode n, std::uint32_t) {
        if (auto feature = NodeFeatureMap::feature_of(n, source)) {
            add_use(result.uses, *feature, range_of(n));
        }
        return true;
    });
    return result;
}

std::vector<ast::FeatureUse> FeatureScanner::scan_own(TSNode statement, std::string_view source) {
    std::vector<ast::FeatureUse> uses;
    walk(statement, [&](TSNode n, std::uint32_t depth) {
        if (depth > 0 && is_statement_kind(kind_of(n))) {
            return false;  // nested statements carry their own uses
        }
        if (auto feature = NodeFeatureMap::feature_of(n, source)) {
            add_use(uses, *feature, range_of(n));
        }
        return true;
    });
    return uses;
}

}  // namespace cppi::parse
