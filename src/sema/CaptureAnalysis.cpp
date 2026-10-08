/// @file CaptureAnalysis.cpp
/// @brief Implementation of CaptureAnalysis.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/CaptureAnalysis.hpp"

#include "support/Overloaded.hpp"

#include <variant>

namespace cppi::sema {

std::set<std::string> CaptureAnalysis::names(const ast::Stmt& stmt) {
    std::set<std::string> out;
    statement(stmt, out);
    return out;
}

void CaptureAnalysis::expression(const ast::Expr& e, std::set<std::string>& out) {
    auto sub = [&](const ast::ExprPtr& p) {
        if (p) {
            expression(*p, out);
        }
    };
    std::visit(detail::Overloaded{
                   [&](const ast::Identifier& id) {
                       if (id.scope.empty()) {
                           out.insert(id.name);
                       }
                   },
                   [&](const ast::CallExpr& c) {
                       if (c.scope.empty() && !c.callee.empty()) {
                           out.insert(c.callee);
                       }
                       sub(c.callee_expr);
                       for (const auto& a : c.args) {
                           expression(a, out);
                       }
                   },
                   [&](const ast::UnaryExpr& u) { sub(u.operand); },
                   [&](const ast::BinaryExpr& b) {
                       sub(b.lhs);
                       sub(b.rhs);
                   },
                   [&](const ast::AssignExpr& a) {
                       sub(a.lhs);
                       sub(a.rhs);
                   },
                   [&](const ast::IncDecExpr& i) { sub(i.operand); },
                   [&](const ast::ConditionalExpr& c) {
                       sub(c.condition);
                       sub(c.then_expr);
                       sub(c.else_expr);
                   },
                   [&](const ast::SubscriptExpr& s) {
                       sub(s.base);
                       sub(s.index);
                   },
                   [&](const ast::MemberExpr& m) { sub(m.base); },
                   [&](const ast::CastExpr& c) { sub(c.operand); },
                   [&](const ast::SizeofExpr& s) { sub(s.operand); },
                   [&](const ast::InitList& l) {
                       for (const auto& item : l.elements) {
                           expression(item, out);
                       }
                   },
                   [&](const ast::NewExpr& n) {
                       sub(n.array_size);
                       for (const auto& a : n.args) {
                           expression(a, out);
                       }
                   },
                   [&](const ast::DeleteExpr& d) { sub(d.operand); },
                   [&](const ast::LambdaExpr& l) {
                       // A nested lambda's own captures name variables of this scope.
                       for (const auto& c : l.captures) {
                           if (!c.init) {
                               out.insert(c.name);
                           }
                       }
                       if (l.default_capture != 0 && l.function && l.function->body) {
                           statement(*l.function->body, out);
                       }
                   },
                   [&](const auto&) {},
               },
               e.node);
}

void CaptureAnalysis::statement(const ast::Stmt& s, std::set<std::string>& out) {
    auto sub = [&](const ast::StmtPtr& p) {
        if (p) {
            statement(*p, out);
        }
    };
    auto expr = [&](const ast::ExprPtr& p) {
        if (p) {
            expression(*p, out);
        }
    };
    auto condition = [&](const ast::Condition& c) {
        expr(c.expr);
        if (c.decl) {
            for (const auto& v : c.decl->vars) {
                expr(v.init);
            }
        }
    };
    std::visit(detail::Overloaded{
                   [&](const ast::ExprStmt& e) { expression(e.expr, out); },
                   [&](const ast::DeclStmt& d) {
                       for (const auto& v : d.vars) {
                           expr(v.init);
                           for (const auto& a : v.args) {
                               expression(a, out);
                           }
                           for (const auto& part : v.declarator.parts) {
                               expr(part.size);
                           }
                       }
                   },
                   [&](const ast::Block& b) {
                       for (const auto& inner : b.statements) {
                           statement(inner, out);
                       }
                   },
                   [&](const ast::IfStmt& i) {
                       condition(i.condition);
                       sub(i.then_stmt);
                       sub(i.else_stmt);
                   },
                   [&](const ast::WhileStmt& w) {
                       condition(w.condition);
                       sub(w.body);
                   },
                   [&](const ast::DoWhileStmt& d) {
                       sub(d.body);
                       expr(d.condition);
                   },
                   [&](const ast::ForStmt& f) {
                       sub(f.init);
                       expr(f.condition);
                       expr(f.update);
                       sub(f.body);
                   },
                   [&](const ast::RangeForStmt& r) {
                       expr(r.range_expr);
                       sub(r.body);
                   },
                   [&](const ast::ReturnStmt& r) { expr(r.value); },
                   [&](const ast::ThrowStmt& t) { expr(t.value); },
                   [&](const ast::TryStmt& t) {
                       sub(t.body);
                       for (const auto& c : t.catches) {
                           sub(c.body);
                       }
                   },
                   [&](const ast::SwitchStmt& sw) {
                       condition(sw.condition);
                       for (const auto& c : sw.cases) {
                           for (const auto& inner : c.statements) {
                               statement(inner, out);
                           }
                       }
                   },
                   [&](const auto&) {},
               },
               s.node);
}

}  // namespace cppi::sema
