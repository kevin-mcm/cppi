#pragma once

/// @file SyntaxChecker.hpp
/// @brief Reports syntax errors (ERROR / MISSING nodes) and excessive nesting
/// before the tree is converted to the AST.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Diagnostic.hpp>

#include <tree_sitter/api.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cppi::parse {

/// Walks a tree-sitter tree and reports its ERROR and MISSING nodes and
/// excessive nesting as diagnostics.
class SyntaxChecker {
public:
    /// Expressions nested deeper than this are rejected. Keeps every later
    /// pass (which may recurse) safe from stack exhaustion on hostile input.
    static constexpr std::uint32_t kMaxNesting = 200;

    /// @param diagnostics Where errors are appended.
    /// @param max_errors  Stop reporting once `diagnostics` holds this many.
    SyntaxChecker(std::vector<Diagnostic>& diagnostics, std::size_t max_errors) noexcept
        : diagnostics_(diagnostics), max_errors_(max_errors) {}

    /// Checks the tree rooted at `root`.
    void check(TSNode root);

private:
    /// Output.
    std::vector<Diagnostic>& diagnostics_;
    /// Cap on reported errors.
    std::size_t max_errors_;
};

}  // namespace cppi::parse
