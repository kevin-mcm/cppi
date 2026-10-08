#include "parse/Parser.hpp"

#include "parse/SyntaxChecker.hpp"
#include "parse/TreeConverter.hpp"
#include "parse/TreeSitterUtils.hpp"
#include "support/DiagnosticFactory.hpp"

#include <tree_sitter/api.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace cppi::parse {

void Parser::TreeDeleter::operator()(TSTree* tree) const noexcept {
    ts_tree_delete(tree);
}

Parser::Tree Parser::parse_tree(std::string_view source) {
    if (source.size() + 3 > std::numeric_limits<std::uint32_t>::max()) {
        return nullptr;
    }
    return Tree(
        ts_parser_parse_string(parser_.get(), nullptr, source.data(), static_cast<std::uint32_t>(source.size())));
}

void Parser::Deleter::operator()(TSParser* parser) const noexcept {
    ts_parser_delete(parser);
}

Parser::Parser() : parser_(ts_parser_new()) {
    if (!parser_ || !ts_parser_set_language(parser_.get(), tree_sitter_cpp())) {
        throw std::runtime_error("cppi: incompatible tree-sitter-cpp grammar");
    }
}

Parser::~Parser() = default;

ParseResult Parser::parse(std::string_view source) {
    repair_budget_ = kRepairBudget;
    return parse_repairing(source, 0);
}

ParseResult Parser::parse_once(std::string_view source, const TSTree* tree, bool convert) {
    ParseResult result;
    if (tree == nullptr) {
        result.diagnostics.push_back(detail::DiagnosticFactory::syntax_error({}));
        return result;
    }
    const TSNode root = ts_tree_root_node(tree);
    if (!ts_node_has_error(root)) {
        SyntaxChecker(result.diagnostics, kMaxSyntaxErrors).check(root);
        if (result.diagnostics.empty() && convert) {
            result.unit = TreeConverter(source).convert_unit(root);
        }
        return result;
    }

    // Script mode: tree-sitter's grammar does not accept every statement at
    // the top level (`cout << x;` reads like a broken declaration there).
    // Re-parse the program as the body of a block, keeping positions (only
    // the first row moves, by the "{"), and use that reading for the
    // top-level items that failed, when it is clean.
    std::string wrapped;
    wrapped.reserve(source.size() + 3);
    wrapped.push_back('{');
    wrapped.append(source);
    wrapped.append("\n}");
    const Tree block_tree = parse_tree(wrapped);
    std::vector<TSNode> block_items;
    if (block_tree) {
        const TSNode block_root = ts_tree_root_node(block_tree.get());
        if (auto block = first_named_child(block_root); block && kind_of(*block) == "compound_statement") {
            const std::uint32_t count = ts_node_named_child_count(*block);
            for (std::uint32_t i = 0; i < count; ++i) {
                const TSNode item = ts_node_named_child(*block, i);
                if (!is_comment(item)) {
                    block_items.push_back(item);
                }
            }
        }
    }

    struct Item {
        TSNode node;
        bool from_block = false;
    };
    std::vector<Item> items;
    std::uint32_t covered_until = 0;  // source bytes already taken from the block reading
    std::size_t next_block = 0;
    const std::uint32_t count = ts_node_named_child_count(root);
    for (std::uint32_t i = 0; i < count; ++i) {
        const TSNode item = ts_node_named_child(root, i);
        if (is_comment(item) || ts_node_end_byte(item) <= covered_until) {
            continue;
        }
        if (!ts_node_has_error(item)) {
            items.push_back(Item{item, false});
            continue;
        }
        const std::uint32_t start = ts_node_start_byte(item);
        const std::uint32_t end = ts_node_end_byte(item);
        std::vector<TSNode> replacement;
        bool clean = true;
        for (std::size_t b = next_block; b < block_items.size(); ++b) {
            const std::uint32_t b_start = ts_node_start_byte(block_items[b]) - 1;
            const std::uint32_t b_end = ts_node_end_byte(block_items[b]) - 1;
            if (b_end <= start) {
                continue;
            }
            if (b_start >= end) {
                break;
            }
            replacement.push_back(block_items[b]);
            // An item that reaches the closing "}" we added is missing its own.
            clean = clean && !ts_node_has_error(block_items[b]) && b_end <= source.size();
        }
        if (replacement.empty() || !clean) {
            items.push_back(Item{item, false});
            continue;
        }
        for (const TSNode r : replacement) {
            items.push_back(Item{r, true});
            covered_until = std::max(covered_until, ts_node_end_byte(r) - 1);
        }
        while (next_block < block_items.size() && ts_node_end_byte(block_items[next_block]) - 1 <= covered_until) {
            ++next_block;
        }
    }

    SyntaxChecker checker(result.diagnostics, kMaxSyntaxErrors);
    for (const Item& item : items) {
        const FirstRowShift shift(item.from_block ? 1 : 0);
        checker.check(item.node);
    }
    if (!result.diagnostics.empty() || !convert) {
        return result;
    }
    const TreeConverter top(source);
    const TreeConverter inner(wrapped);
    for (const Item& item : items) {
        if (item.from_block) {
            const FirstRowShift shift(1);
            inner.convert_into(item.node, result.unit.statements);
        } else {
            top.convert_into(item.node, result.unit.statements);
        }
    }
    return result;
}

ParseResult Parser::parse_repairing(std::string_view source, std::size_t depth) {
    const Tree tree = parse_tree(source);
    ParseResult result = parse_once(source, tree.get(), true);
    // As many errors as reported at most: garbage, not a forgotten token.
    if (result.ok() || depth >= kMaxRepairs || result.diagnostics.size() >= kMaxSyntaxErrors) {
        return result;
    }
    const Diagnostic& first = result.diagnostics.front();
    if (first.code != DiagCode::SyntaxError && first.code != DiagCode::MissingToken) {
        return result;
    }
    const auto repair = find_missing_token(source, tree.get(), first);
    if (!repair) {
        return result;
    }
    std::string repaired(source.substr(0, repair->offset));
    repaired.append(repair->token);
    repaired.append(source.substr(repair->offset));
    const ParseResult rest = parse_repairing(repaired, depth + 1);

    // Where the token goes: right after the last token before it.
    SourceLocation at{1, 1};
    std::size_t line_start = 0;
    for (std::size_t i = 0; i < repair->offset; ++i) {
        if (source[i] == '\n') {
            ++at.line;
            line_start = i + 1;
        }
    }
    at.column = static_cast<std::uint32_t>(repair->offset - line_start + 1);
    std::size_t next = repair->offset;
    while (next < source.size() && (source[next] == ' ' || source[next] == '\t' || source[next] == '\r')) {
        ++next;
    }
    const bool line_end = next == source.size() || source[next] == '\n' || source.substr(next, 2) == "//";

    ParseResult out;
    out.diagnostics.push_back(
        detail::DiagnosticFactory::missing_token(SourceRange{at, at}, repair->token, line_end ? at.line : 0));
    // Later errors were found with the token in place: undo its column shift.
    const auto shift = [&](SourceLocation& l) {
        if (l.line == at.line && l.column > at.column) {
            l.column -= static_cast<std::uint32_t>(repair->token.size());
        }
    };
    for (Diagnostic d : rest.diagnostics) {
        if (out.diagnostics.size() >= kMaxSyntaxErrors) {
            break;
        }
        shift(d.range.begin);
        shift(d.range.end);
        out.diagnostics.push_back(std::move(d));
    }
    return out;
}

std::optional<Parser::Repair> Parser::find_missing_token(std::string_view source, const TSTree* tree,
                                                         const Diagnostic& first_error) {
    if (tree == nullptr) {
        return std::nullopt;
    }
    std::vector<std::uint32_t> line_starts{0};
    for (std::uint32_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n') {
            line_starts.push_back(i + 1);
        }
    }
    const auto offset_of = [&](SourceLocation l) -> std::uint32_t {
        if (l.line == 0 || l.line > line_starts.size()) {
            return static_cast<std::uint32_t>(source.size());
        }
        return std::min(line_starts[l.line - 1] + l.column - 1, static_cast<std::uint32_t>(source.size()));
    };
    const std::uint32_t error_end = offset_of(first_error.range.end);
    const std::uint32_t first_line = first_error.range.begin.line > 3 ? first_error.range.begin.line - 3 : 1;
    const std::uint32_t window_start = first_line <= line_starts.size() ? line_starts[first_line - 1] : 0;

    // Ends of the tokens in the window, ERROR regions included.
    std::vector<std::uint32_t> ends;
    std::vector<TSNode> stack{ts_tree_root_node(tree)};
    while (!stack.empty()) {
        const TSNode node = stack.back();
        stack.pop_back();
        const std::uint32_t start = ts_node_start_byte(node);
        const std::uint32_t end = ts_node_end_byte(node);
        if (end < window_start || start > error_end || ts_node_is_missing(node)) {
            continue;
        }
        const std::uint32_t count = ts_node_child_count(node);
        if (count == 0 || is_comment(node) || kind_of(node) == "string_literal" || kind_of(node) == "char_literal") {
            if (end > start && end >= window_start && end <= error_end) {
                ends.push_back(end);
            }
            continue;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            stack.push_back(ts_node_child(node, i));
        }
    }
    std::sort(ends.begin(), ends.end());
    ends.erase(std::unique(ends.begin(), ends.end()), ends.end());
    const auto at_line_end = [&](std::uint32_t offset) {
        std::size_t next = offset;
        while (next < source.size() && (source[next] == ' ' || source[next] == '\t' || source[next] == '\r')) {
            ++next;
        }
        return next == source.size() || source[next] == '\n' || source.substr(next, 2) == "//" ||
               source.substr(next, 2) == "/*";
    };
    // ';' is tried from the first token on (a later ';' can close a broken
    // statement in a way that also parses: `void f() { g() };`), ')' and '}'
    // from the error back (what is left open usually ends near the error). Line ends before
    // the middle of lines, and a bounded number of tries for each token.
    const SourceLocation past = first_error.range.end;
    for (const std::string_view token : {std::string_view(";"), std::string_view(")"), std::string_view("}")}) {
        std::vector<std::uint32_t> candidates;
        for (const bool line_ends : {true, false}) {
            for (std::size_t i = 0; i < ends.size() && candidates.size() < kMaxRepairTries; ++i) {
                const std::uint32_t offset = token == ";" ? ends[i] : ends[ends.size() - 1 - i];
                if (at_line_end(offset) == line_ends) {
                    candidates.push_back(offset);
                }
            }
        }
        for (const std::uint32_t offset : candidates) {
            const std::size_t cost = 2 * (source.size() + token.size());  // both readings
            if (cost > repair_budget_) {
                return std::nullopt;
            }
            repair_budget_ -= cost;
            std::string attempt(source.substr(0, offset));
            attempt.append(token);
            attempt.append(source.substr(offset));
            const ParseResult r = parse_once(attempt, parse_tree(attempt).get(), false);
            if (r.diagnostics.empty()) {
                return Repair{offset, token};
            }
            const SourceLocation next = r.diagnostics.front().range.begin;
            if (r.diagnostics.front().code != DiagCode::NestingTooDeep &&
                (next.line > past.line || (next.line == past.line && next.column > past.column + token.size()))) {
                return Repair{offset, token};
            }
        }
    }
    return std::nullopt;
}

}  // namespace cppi::parse
