#pragma once

/// @file TreeSitterUtils.hpp
/// @brief Small helpers over tree-sitter nodes, shared by the parse module.
///
/// Only files in src/parse may include this (it pulls in tree-sitter).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/SourceLocation.hpp>
#include <cppi/SourceRange.hpp>

#include <tree_sitter/api.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace cppi::parse {

/// While alive, positions on the first row are shifted left: used when a
/// statement is re-parsed from "{" + source (script mode; see Parser).
class FirstRowShift {
public:
    /// Shifts first-row columns left by `columns` until destroyed.
    explicit FirstRowShift(std::uint32_t columns) noexcept : previous_(value()) { value() = columns; }
    FirstRowShift(const FirstRowShift&) = delete;
    FirstRowShift& operator=(const FirstRowShift&) = delete;
    FirstRowShift(FirstRowShift&&) = delete;
    FirstRowShift& operator=(FirstRowShift&&) = delete;
    /// Restores the previous shift.
    ~FirstRowShift() { value() = previous_; }

    /// The current shift (per thread).
    [[nodiscard]] static std::uint32_t& value() noexcept {
        thread_local std::uint32_t shift = 0;
        return shift;
    }

private:
    /// The shift to restore.
    std::uint32_t previous_;
};

/// A tree-sitter point (0-based) as a 1-based SourceLocation, applying the
/// FirstRowShift.
inline SourceLocation to_location(TSPoint point) noexcept {
    std::uint32_t column = point.column;
    if (point.row == 0 && column >= FirstRowShift::value()) {
        column -= FirstRowShift::value();
    }
    return {point.row + 1, column + 1};
}

/// The source range a node spans.
inline SourceRange range_of(TSNode node) noexcept {
    return {to_location(ts_node_start_point(node)), to_location(ts_node_end_point(node))};
}

/// The node's kind ("identifier", "call_expression", ...).
inline std::string_view kind_of(TSNode node) noexcept {
    return ts_node_type(node);
}

/// True for comment nodes.
inline bool is_comment(TSNode node) noexcept {
    return kind_of(node) == "comment";
}

/// The node's spelling in `source` ("" if the node lies outside it).
inline std::string_view text_of(TSNode node, std::string_view source) noexcept {
    const std::uint32_t begin = ts_node_start_byte(node);
    const std::uint32_t end = ts_node_end_byte(node);
    if (begin > end || end > source.size()) {
        return {};
    }
    return source.substr(begin, end - begin);
}

/// The first named child that is not a comment.
inline std::optional<TSNode> first_named_child(TSNode node) noexcept {
    const std::uint32_t count = ts_node_named_child_count(node);
    for (std::uint32_t i = 0; i < count; ++i) {
        const TSNode child = ts_node_named_child(node, i);
        if (!is_comment(child)) {
            return child;
        }
    }
    return std::nullopt;
}

/// Pre-order traversal with a TSTreeCursor: iterative, so arbitrarily deep
/// trees never overflow the native stack. `visit(node, depth)` returns false
/// to skip the node's children.
template <typename Visit>
void walk(TSNode root, const Visit& visit) {
    TSTreeCursor cursor = ts_tree_cursor_new(root);
    bool descend = visit(ts_tree_cursor_current_node(&cursor), 0u);
    std::uint32_t depth = 0;
    while (true) {
        if (descend && ts_tree_cursor_goto_first_child(&cursor)) {
            ++depth;
        } else {
            while (!ts_tree_cursor_goto_next_sibling(&cursor)) {
                if (depth == 0 || !ts_tree_cursor_goto_parent(&cursor)) {
                    ts_tree_cursor_delete(&cursor);
                    return;
                }
                --depth;
            }
        }
        descend = visit(ts_tree_cursor_current_node(&cursor), depth);
    }
}

}  // namespace cppi::parse
