#pragma once

/// Reports syntax errors (ERROR / MISSING nodes) and excessive nesting before
/// the tree is converted to the AST.

#include <cppi/Diagnostic.hpp>

#include <tree_sitter/api.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cppi::parse {

class SyntaxChecker {
public:
    /// Expressions nested deeper than this are rejected. Keeps every later
    /// pass (which may recurse) safe from stack exhaustion on hostile input.
    static constexpr std::uint32_t kMaxNesting = 200;

    SyntaxChecker(std::vector<Diagnostic>& diagnostics, std::size_t max_errors) noexcept
        : diagnostics_(diagnostics), max_errors_(max_errors) {}

    void check(TSNode root);

private:
    std::vector<Diagnostic>& diagnostics_;
    std::size_t max_errors_;
};

}  // namespace cppi::parse
