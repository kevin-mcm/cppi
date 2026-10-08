#pragma once

/// @file CaptureAnalysis.hpp
/// @brief Finds the names a lambda's body uses, so `[=]` and `[&]` capture
/// exactly the enclosing function's variables it needs.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"

#include <set>
#include <string>

namespace cppi::sema {

/// Collects the names a lambda body uses.
class CaptureAnalysis {
public:
    /// Every unqualified name used in `stmt` (variables and called names).
    [[nodiscard]] static std::set<std::string> names(const ast::Stmt& stmt);

private:
    /// Adds the names used in a statement.
    static void statement(const ast::Stmt& s, std::set<std::string>& out);
    /// Adds the names used in an expression.
    static void expression(const ast::Expr& e, std::set<std::string>& out);
};

}  // namespace cppi::sema
