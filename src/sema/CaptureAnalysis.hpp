#pragma once

/// Finds the names a lambda's body uses, so `[=]` and `[&]` capture exactly
/// the enclosing function's variables it needs.

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"

#include <set>
#include <string>

namespace cppi::sema {

class CaptureAnalysis {
public:
    /// Every unqualified name used in `stmt` (variables and called names).
    [[nodiscard]] static std::set<std::string> names(const ast::Stmt& stmt);

private:
    static void statement(const ast::Stmt& s, std::set<std::string>& out);
    static void expression(const ast::Expr& e, std::set<std::string>& out);
};

}  // namespace cppi::sema
