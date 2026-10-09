/// @file StatementBinder.cpp
/// @brief Implementation of StatementBinder.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/StatementBinder.hpp"

#include "sema/DeclarationBinder.hpp"
#include "sema/ExpressionBinder.hpp"
#include "sema/InitializerBinder.hpp"
#include "sema/Intrinsics.hpp"
#include "sema/TemplateEngine.hpp"
#include "sema/TypeResolver.hpp"
#include "support/DiagnosticFactory.hpp"
#include "support/Overloaded.hpp"

#include <algorithm>
#include <set>
#include <utility>
#include <variant>

namespace cppi::sema {

using detail::DiagnosticFactory;

namespace {

/// Expressions whose evaluation changes something.
bool has_effect(const BExpr& e) {
    return std::visit(
        detail::Overloaded{
            [](const BCall&) { return true; },
            [](const BHostCall&) { return true; },
            [](const BIntrinsic&) { return true; },
            [](const BAssign&) { return true; },
            [](const BIncDec&) { return true; },
            [](const BNew&) { return true; },
            [](const BDelete&) { return true; },
            [](const BTemp& t) { return !t.init_stmts.empty(); },
            [](const BComma& c) { return has_effect(*c.lhs) || has_effect(*c.rhs); },
            [](const BConditional& c) { return has_effect(*c.then_expr) || has_effect(*c.else_expr); },
            [](const BDeref& d) { return std::holds_alternative<BCall>(d.pointer->node) && has_effect(*d.pointer); },
            [](const BLogical& l) { return has_effect(*l.rhs); },
            [](const BConvert& c) { return has_effect(*c.operand); },
            [](const BLoad& l) { return has_effect(*l.lvalue); },
            [](const auto&) { return false; },
        },
        e.node);
}

/// A `break` that can run and leaves the statement `s` is part of: not one
/// after a return (`case 1: return 1; break;`), nor one of a nested loop or switch.
bool breaks_out(const BStmt& s);

bool breaks_out(const std::vector<BStmt>& list) {
    for (const BStmt& x : list) {
        if (breaks_out(x)) {
            return true;
        }
        if (StatementBinder::always_returns(x)) {
            return false;  // what follows never runs
        }
    }
    return false;
}

bool breaks_out(const BStmt& s) {
    return std::visit(detail::Overloaded{
                          [](const BBreak&) { return true; }, [](const BBlock& b) { return breaks_out(b.statements); },
                          [](const BIf& i) {
                              return (i.then_stmt && breaks_out(*i.then_stmt)) ||
                                     (i.else_stmt && breaks_out(*i.else_stmt));
                          },
                          [](const BTry& t) {
                              return breaks_out(*t.body) ||
                                     std::ranges::any_of(t.handlers, [](const BStmt& h) { return breaks_out(h); });
                          },
                          [](const auto&) { return false; },  // nested loops and switches own their breaks
                      },
                      s.node);
}

}  // namespace

bool StatementBinder::always_returns(const BStmt& stmt) {
    return std::visit(
        detail::Overloaded{
            [](const BReturn&) { return true; },
            [](const BThrow&) { return true; },
            [](const BTry& t) {
                return always_returns(*t.body) &&
                       std::ranges::all_of(t.handlers, [](const BStmt& h) { return always_returns(h); });
            },
            [](const BBlock& b) {
                return std::ranges::any_of(b.statements, [](const BStmt& s) { return always_returns(s); });
            },
            [](const BIf& i) {
                return i.then_stmt && i.else_stmt && always_returns(*i.then_stmt) && always_returns(*i.else_stmt);
            },
            [](const BLoop& l) {
                bool infinite = !l.condition.has_value();
                if (l.condition) {
                    const auto* c = std::get_if<BConst>(&l.condition->node);
                    infinite = c != nullptr && c->bits != 0;
                }
                return infinite && (!l.body || !breaks_out(*l.body));
            },
            [](const BSwitch& s) {
                // Without `default` some value skips every case. Otherwise it
                // returns if no case can break out of it and the last section,
                // where every case ends up by falling through, returns.
                if (!s.default_section || s.sections.empty() ||
                    std::ranges::any_of(s.sections, [](const auto& section) { return breaks_out(section); })) {
                    return false;
                }
                return std::ranges::any_of(s.sections.back(), [](const BStmt& x) { return always_returns(x); });
            },
            [](const auto&) { return false; },
        },
        stmt.node);
}

std::vector<BStmt> StatementBinder::release_temporaries(std::size_t mark) const {
    std::vector<BStmt> out;
    auto& temps = ctx_.fn->temporaries;
    for (std::size_t i = temps.size(); i-- > mark;) {
        BVar slot;
        slot.offset = temps[i].offset;
        slot.cells = ctx_.types.cells(temps[i].type);
        ctx_.initializers->destroy(ExpressionBinder::make(temps[i].type, true, SourceRange{}, slot), temps[i].type,
                                   out);
    }
    temps.resize(std::min(mark, temps.size()));
    return out;
}

void StatementBinder::drop_temporaries(std::size_t mark) const {
    auto& temps = ctx_.fn->temporaries;
    temps.resize(std::min(mark, temps.size()));
}

void StatementBinder::bind_statements(const std::vector<ast::Stmt>& statements, std::vector<BStmt>& out) {
    std::vector<BStmt>* current = &out;
    for (const auto& s : statements) {
        bind(s, current);
    }
}

BBlock StatementBinder::bind_block(const ast::Block& block, SourceRange range) {
    BBlock out;
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    bind_statements(block.statements, out.statements);
    ctx_.symbols.pop();
    return out;
}

BStmtPtr StatementBinder::bind_substatement(const ast::Stmt* stmt, SourceRange range) {
    auto out = std::make_unique<BStmt>();
    out->range = range;
    BBlock block;
    if (stmt != nullptr) {
        ctx_.symbols.push(Scope::Kind::Block, range.end);
        std::vector<BStmt>* current = &block.statements;
        bind(*stmt, current);
        ctx_.symbols.pop();
    }
    out->node = std::move(block);
    return out;
}

void StatementBinder::bind(const ast::Stmt& stmt, std::vector<BStmt>*& out) {
    if (!ctx_.features.admit(stmt.uses)) {
        ctx_.declarations->poison(stmt);
        return;
    }
    const SourceRange range = stmt.range;
    // Simple statements are charged once each by the statement cost unit;
    // compound ones are charged through their conditions (see CodeGenerator).
    const bool simple =
        std::holds_alternative<ast::ExprStmt>(stmt.node) || std::holds_alternative<ast::DeclStmt>(stmt.node) ||
        std::holds_alternative<ast::ReturnStmt>(stmt.node) || std::holds_alternative<ast::BreakStmt>(stmt.node) ||
        std::holds_alternative<ast::ContinueStmt>(stmt.node) || std::holds_alternative<ast::ThrowStmt>(stmt.node) ||
        std::holds_alternative<ast::FunctionDef>(stmt.node);  // `T x(n);` in a block
    std::vector<BStmt>* const first_out = out;
    const std::size_t first = out->size();
    std::visit(
        detail::Overloaded{
            [&](const ast::ExprStmt& s) { bind_expression(s, range, *out); },
            [&](const ast::DeclStmt& s) { ctx_.declarations->bind_variables(s, range, out); },
            [&](const ast::Block& b) { out->push_back(wrap(range, bind_block(b, range))); },
            [&](const ast::IfStmt& s) { bind_if(s, range, *out); },
            [&](const ast::WhileStmt& s) { bind_while(s, range, *out); },
            [&](const ast::DoWhileStmt& s) { bind_do(s, range, *out); },
            [&](const ast::ForStmt& s) { bind_for(s, range, *out); },
            [&](const ast::RangeForStmt& s) { bind_range_for(s, range, *out); },
            [&](const ast::BreakStmt&) {
                if (ctx_.fn->breakable_depth == 0) {
                    ctx_.report(DiagnosticFactory::misplaced_jump(range, "break"));
                    return;
                }
                out->push_back(wrap(range, BBreak{}));
            },
            [&](const ast::ContinueStmt&) {
                if (ctx_.fn->loop_depth == 0) {
                    ctx_.report(DiagnosticFactory::misplaced_jump(range, "continue"));
                    return;
                }
                out->push_back(wrap(range, BContinue{}));
            },
            [&](const ast::ReturnStmt& s) { bind_return(s, range, *out); },
            [&](const ast::ThrowStmt& s) { bind_throw(s, range, *out); },
            [&](const ast::TryStmt& s) { bind_try(s, range, *out); },
            [&](const ast::SwitchStmt& s) { bind_switch(s, range, *out); },
            [&](const ast::StaticAssert& s) { bind_static_assert(s, range); },
            [&](const ast::FunctionDef& f) {
                if (const ast::DeclStmt* object = ctx_.declarations->as_object_declaration(f.decl)) {
                    ctx_.declarations->bind_variables(*object, range, out);
                    return;
                }
                if (!ctx_.symbols.at_global_scope()) {
                    ctx_.report(DiagnosticFactory::declaration_not_allowed(range, "function definition"));
                    return;
                }
                ctx_.declarations->bind_function(f.decl, range);
            },
            [&](const ast::RecordDef& r) { ctx_.declarations->bind_record(r, range, out); },
            [&](const ast::EnumDef& e) { ctx_.declarations->bind_enum(e, range); },
            [&](const ast::AliasDecl& a) { ctx_.declarations->bind_alias(a, range); },
            [&](const ast::TemplateDecl& t) {
                if (!ctx_.symbols.at_global_scope()) {
                    ctx_.report(DiagnosticFactory::declaration_not_allowed(range, "template"));
                    return;
                }
                ctx_.templates->declare(t, range);
            },
            [&](const ast::NamespaceDef& ns) {
                if (!ctx_.symbols.at_global_scope()) {
                    ctx_.report(DiagnosticFactory::declaration_not_allowed(range, "namespace"));
                    return;
                }
                ctx_.symbols.enter_namespace(ns.name, ns.name_range);
                for (const auto& inner : ns.body) {
                    bind(inner, out);
                }
                ctx_.symbols.leave_namespace();
            },
            [&](const ast::UsingDecl& u) {
                if (u.directive) {
                    std::vector<std::string> path = u.scope;
                    path.push_back(u.name);
                    Scope* ns = ctx_.symbols.find_namespace(path);
                    if (ns == nullptr) {
                        ctx_.report(DiagnosticFactory::unknown_identifier(range, u.name, std::nullopt));
                        return;
                    }
                    ctx_.symbols.current().add_using(ns);
                    return;
                }
                Symbol* target = ctx_.symbols.lookup_qualified(u.scope, u.name);
                if (target == nullptr) {
                    ctx_.report(DiagnosticFactory::unknown_identifier(range, u.name, std::nullopt));
                    return;
                }
                if (ctx_.symbols.current().add(u.name, *target) == nullptr) {
                    ctx_.report(DiagnosticFactory::redefinition(range, u.name));
                }
            },
            [&](const ast::ConceptDef& c) {
                ctx_.report(DiagnosticFactory::declaration_not_allowed(c.name_range, "concept without template"));
            },
            [&](const ast::Unsupported& u) { ctx_.features.explain(u, range); },
        },
        stmt.node);
    if (simple && first_out->size() > first) {
        (*first_out)[first].counted = true;
    }
}

void StatementBinder::bind_expression(const ast::ExprStmt& s, SourceRange range, std::vector<BStmt>& out) {
    const std::size_t mark = ctx_.fn->temporaries.size();
    auto e = ctx_.expressions->bind(s.expr, ExpressionBinder::Use::Statement);
    if (!e) {
        drop_temporaries(mark);
        return;
    }
    if (!has_effect(*e) && !ctx_.types.is_void(e->type)) {
        ctx_.report(DiagnosticFactory::expression_has_no_effect(range));
        drop_temporaries(mark);
        return;
    }
    out.push_back(wrap(range, BExprStmt{std::move(*e)}));
    for (auto& cleanup : release_temporaries(mark)) {
        out.push_back(std::move(cleanup));
    }
}

std::optional<BExpr> StatementBinder::bind_condition(const ast::Condition& c, std::vector<BStmt>& pre) {
    if (c.decl) {
        // `if (int n = count())`: declare, then test the variable.
        std::vector<BStmt>* target = &pre;
        ctx_.declarations->bind_variables(*c.decl, c.range, target);
        Symbol* symbol = ctx_.symbols.current().find(c.decl->vars.front().declarator.name);
        if (symbol == nullptr || symbol->kind != SymbolKind::Variable) {
            return std::nullopt;
        }
        auto var = ctx_.expressions->variable(*symbol, c.decl->vars.front().declarator.name, c.range);
        auto cond = ctx_.expressions->to_condition(ctx_.expressions->to_rvalue(std::move(var)));
        if (!cond) {
            return std::nullopt;
        }
        return std::move(*cond);
    }
    if (!c.expr) {
        return std::nullopt;
    }
    auto cond = ctx_.expressions->bind_condition(*c.expr);
    if (!cond) {
        return std::nullopt;
    }
    return std::move(*cond);
}

void StatementBinder::bind_if(const ast::IfStmt& s, SourceRange range, std::vector<BStmt>& out) {
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    BBlock outer;
    const std::size_t mark = ctx_.fn->temporaries.size();
    auto cond = bind_condition(s.condition, outer.statements);
    auto cond_cleanup = release_temporaries(mark);
    std::optional<std::int64_t> constant;
    if (s.is_constexpr && cond) {
        constant = ctx_.constants.integer(*cond);
        if (!constant) {
            ctx_.report(DiagnosticFactory::not_constant(s.condition.range));
        }
    }
    // `if constexpr`: the discarded branch is not bound at all (in a template
    // it may not even be valid for these template arguments).
    const bool keep_then = !constant || *constant != 0;
    const bool keep_else = !constant || *constant == 0;
    auto then_stmt = keep_then ? bind_substatement(s.then_stmt.get(), s.then_stmt ? s.then_stmt->range : range)
                               : bind_substatement(nullptr, range);
    BStmtPtr else_stmt;
    if (s.else_stmt && keep_else) {
        else_stmt = bind_substatement(s.else_stmt.get(), s.else_stmt->range);
    }
    ctx_.symbols.pop();
    if (!cond) {
        return;
    }
    if (constant && outer.statements.empty() && cond_cleanup.empty()) {
        // `if constexpr`: only the chosen branch remains.
        if (*constant != 0) {
            out.push_back(std::move(*then_stmt));
        } else if (else_stmt) {
            out.push_back(std::move(*else_stmt));
        }
        return;
    }
    BIf node;
    node.condition = std::move(*cond);
    node.then_stmt = std::move(then_stmt);
    node.else_stmt = std::move(else_stmt);
    node.condition_cleanup = std::move(cond_cleanup);
    if (outer.statements.empty()) {
        out.push_back(wrap(range, std::move(node)));
        return;
    }
    outer.statements.push_back(wrap(range, std::move(node)));
    out.push_back(wrap(range, std::move(outer)));
}

void StatementBinder::bind_while(const ast::WhileStmt& s, SourceRange range, std::vector<BStmt>& out) {
    if (s.condition.decl) {
        // The declaration is re-created on every iteration: lower to
        // `for (;;) { T x = init; if (!x) break; body }`.
        ctx_.symbols.push(Scope::Kind::Block, range.end);
        BBlock body;
        auto cond = bind_condition(s.condition, body.statements);
        ++ctx_.fn->loop_depth;
        ++ctx_.fn->breakable_depth;
        auto inner = bind_substatement(s.body.get(), s.body ? s.body->range : range);
        --ctx_.fn->loop_depth;
        --ctx_.fn->breakable_depth;
        ctx_.symbols.pop();
        if (!cond) {
            return;
        }
        BIf exit;
        auto negated =
            ExpressionBinder::make(TypeTable::kBool, false, range,
                                   BUnary{UnOp::Not, ScalarKind::Int32, std::make_unique<BExpr>(std::move(*cond))});
        exit.condition = std::move(*negated);
        exit.then_stmt = std::make_unique<BStmt>(wrap(range, BBreak{}));
        body.statements.push_back(wrap(range, std::move(exit)));
        body.statements.push_back(std::move(*inner));
        BLoop loop;
        loop.kind = LoopKind::For;
        loop.body = std::make_unique<BStmt>(wrap(range, std::move(body)));
        out.push_back(wrap(range, std::move(loop)));
        return;
    }
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    std::vector<BStmt> pre;
    const std::size_t mark = ctx_.fn->temporaries.size();
    auto cond = bind_condition(s.condition, pre);
    auto cond_cleanup = release_temporaries(mark);
    ++ctx_.fn->loop_depth;
    ++ctx_.fn->breakable_depth;
    auto body = bind_substatement(s.body.get(), s.body ? s.body->range : range);
    --ctx_.fn->loop_depth;
    --ctx_.fn->breakable_depth;
    ctx_.symbols.pop();
    if (!cond) {
        return;
    }
    BLoop loop;
    loop.kind = LoopKind::While;
    loop.condition = std::move(*cond);
    loop.condition_cleanup = std::move(cond_cleanup);
    loop.body = std::move(body);
    out.push_back(wrap(range, std::move(loop)));
}

void StatementBinder::bind_do(const ast::DoWhileStmt& s, SourceRange range, std::vector<BStmt>& out) {
    ++ctx_.fn->loop_depth;
    ++ctx_.fn->breakable_depth;
    auto body = bind_substatement(s.body.get(), s.body ? s.body->range : range);
    --ctx_.fn->loop_depth;
    --ctx_.fn->breakable_depth;
    if (!s.condition) {
        return;
    }
    const std::size_t mark = ctx_.fn->temporaries.size();
    auto cond = ctx_.expressions->bind_condition(*s.condition);
    auto cond_cleanup = release_temporaries(mark);
    if (!cond) {
        return;
    }
    BLoop loop;
    loop.kind = LoopKind::DoWhile;
    loop.condition = std::move(*cond);
    loop.condition_cleanup = std::move(cond_cleanup);
    loop.body = std::move(body);
    out.push_back(wrap(range, std::move(loop)));
}

void StatementBinder::bind_for(const ast::ForStmt& s, SourceRange range, std::vector<BStmt>& out) {
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    BBlock outer;
    std::vector<BStmt>* init_out = &outer.statements;
    if (s.init) {
        bind(*s.init, init_out);
    }
    std::optional<BExpr> cond;
    bool ok = true;
    std::size_t mark = ctx_.fn->temporaries.size();
    if (s.condition) {
        auto c = ctx_.expressions->bind_condition(*s.condition);
        ok = c != nullptr;
        if (c) {
            cond = std::move(*c);
        }
    }
    auto cond_cleanup = release_temporaries(mark);
    std::optional<BExpr> update;
    mark = ctx_.fn->temporaries.size();
    if (s.update) {
        auto u = ctx_.expressions->bind(*s.update, ExpressionBinder::Use::Statement);
        ok = ok && u != nullptr;
        if (u) {
            update = std::move(*u);
        }
    }
    auto update_cleanup = release_temporaries(mark);
    ++ctx_.fn->loop_depth;
    ++ctx_.fn->breakable_depth;
    auto body = bind_substatement(s.body.get(), s.body ? s.body->range : range);
    --ctx_.fn->loop_depth;
    --ctx_.fn->breakable_depth;
    ctx_.symbols.pop();
    if (!ok) {
        return;
    }
    BLoop loop;
    loop.kind = LoopKind::For;
    loop.condition = std::move(cond);
    loop.update = std::move(update);
    loop.condition_cleanup = std::move(cond_cleanup);
    loop.update_cleanup = std::move(update_cleanup);
    loop.body = std::move(body);
    init_out->push_back(wrap(range, std::move(loop)));
    out.push_back(wrap(range, std::move(outer)));
}

void StatementBinder::bind_range_for(const ast::RangeForStmt& s, SourceRange range, std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    auto container = ctx_.expressions->bind(*s.range_expr);
    if (!container) {
        ctx_.symbols.pop();
        return;
    }
    if (types.is_record(container->type)) {
        bind_range_for_class(s, std::move(container), range, out);
        ctx_.symbols.pop();
        return;
    }
    if (!types.is_array(container->type) || !container->lvalue) {
        ctx_.report(DiagnosticFactory::invalid_operands(s.range_expr->range, "for (:)", types.name(container->type),
                                                        std::nullopt));
        ctx_.symbols.pop();
        return;
    }
    const TypeInfo array = types.info(container->type);
    BBlock outer;
    // auto* __range = &container; for (long __i = 0; __i < N; ++__i) { T x = (*__range)[__i]; body }
    const TypeRef range_type = types.pointer_to(container->type, container->is_const);
    const bool range_const = container->is_const;
    BVar range_slot;
    range_slot.offset = ctx_.allocate_local(1);
    auto range_target = ExpressionBinder::make(range_type, true, range, range_slot);
    outer.statements.push_back(
        wrap(range, BStore{std::move(range_target),
                           ExpressionBinder::make(range_type, false, range, BAddressOf{std::move(container)})}));
    BVar index_slot;
    index_slot.offset = ctx_.allocate_local(1);
    outer.statements.push_back(wrap(range, BStore{ExpressionBinder::make(TypeTable::kLong, true, range, index_slot),
                                                  ExpressionBinder::constant(TypeTable::kLong, 0, range)}));
    auto load = [&](const BVar& v, TypeRef t) {
        return ExpressionBinder::make(t, false, range, BLoad{ExpressionBinder::make(t, true, range, v)});
    };
    BLoop loop;
    loop.kind = LoopKind::For;
    loop.condition = std::move(
        *ExpressionBinder::make(TypeTable::kBool, false, range,
                                BBinary{BinOp::Lt, ScalarKind::Int64, load(index_slot, TypeTable::kLong),
                                        ExpressionBinder::constant(TypeTable::kLong, array.count, range), 1}));
    BIncDec inc;
    inc.kind = ScalarKind::Int64;
    inc.target = ExpressionBinder::make(TypeTable::kLong, true, range, index_slot);
    loop.update = std::move(*ExpressionBinder::make(TypeTable::kLong, true, range, std::move(inc)));

    // The element: (*__range)[__i]
    BDeref whole;
    whole.cells = array.count * types.cells(array.target);
    whole.pointer = load(range_slot, range_type);
    auto array_lvalue =
        ExpressionBinder::make(types.array_of(array.target, array.count), true, range, std::move(whole));
    array_lvalue->is_const = range_const;
    BIndex idx;
    idx.base = std::move(array_lvalue);
    idx.index = load(index_slot, TypeTable::kLong);
    idx.elem_cells = types.cells(array.target);
    idx.bound = array.count;
    auto element = ExpressionBinder::make(array.target, true, range, std::move(idx));
    element->is_const = range_const;

    BBlock body;
    std::vector<BStmt>* body_out = &body.statements;
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    if (!ctx_.declarations->bind_loop_variable(s.variable, std::move(element), range, body_out)) {
        ctx_.symbols.pop();
        ctx_.symbols.pop();
        return;
    }
    ++ctx_.fn->loop_depth;
    ++ctx_.fn->breakable_depth;
    if (s.body) {
        bind(*s.body, body_out);
    }
    --ctx_.fn->loop_depth;
    --ctx_.fn->breakable_depth;
    ctx_.symbols.pop();
    ctx_.symbols.pop();
    loop.body = std::make_unique<BStmt>(wrap(range, std::move(body)));
    outer.statements.push_back(wrap(range, std::move(loop)));
    out.push_back(wrap(range, std::move(outer)));
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks): owned by the variant

void StatementBinder::bind_range_for_class(const ast::RangeForStmt& s, BExprPtr container, SourceRange range,
                                           std::vector<BStmt>& out) {
    // for (x : c)  ==>  auto* r = &c; for (auto b = r->begin(), e = r->end(); b != e; ++b) { T x = *b; body }
    TypeTable& types = ctx_.types;
    const TypeRef record = container->type;
    const bool is_const = container->is_const;
    const TypeRef record_ptr = types.pointer_to(record, is_const);
    if (!container->lvalue) {
        ctx_.report(
            DiagnosticFactory::invalid_operands(s.range_expr->range, "for (:)", types.name(record), std::nullopt));
        return;
    }
    BBlock outer;
    BVar range_slot;
    range_slot.offset = ctx_.allocate_local(1);
    outer.statements.push_back(
        wrap(range, BStore{ExpressionBinder::make(record_ptr, true, range, range_slot),
                           ExpressionBinder::make(record_ptr, false, range, BAddressOf{std::move(container)})}));
    auto object = [&]() {
        BDeref d;
        d.cells = types.cells(record);
        d.pointer = ExpressionBinder::make(record_ptr, false, range,
                                           BLoad{ExpressionBinder::make(record_ptr, true, range, range_slot)});
        auto e = ExpressionBinder::make(record, true, range, std::move(d));
        e->is_const = is_const;
        return e;
    };
    auto begin = ctx_.expressions->call_method(object(), "begin", {}, range);
    auto end = ctx_.expressions->call_method(object(), "end", {}, range);
    if (!begin || !end) {
        return;
    }
    begin = ctx_.expressions->to_rvalue(std::move(begin));
    end = ctx_.expressions->to_rvalue(std::move(end));
    if (!types.is_pointer(begin->type) || begin->type != end->type) {
        ctx_.report(
            DiagnosticFactory::invalid_operands(range, "for (:)", types.name(begin->type), types.name(end->type)));
        return;
    }
    const TypeRef iterator = begin->type;
    BVar begin_slot;
    begin_slot.offset = ctx_.allocate_local(1);
    BVar end_slot;
    end_slot.offset = ctx_.allocate_local(1);
    outer.statements.push_back(
        wrap(range, BStore{ExpressionBinder::make(iterator, true, range, begin_slot), std::move(begin)}));
    outer.statements.push_back(
        wrap(range, BStore{ExpressionBinder::make(iterator, true, range, end_slot), std::move(end)}));
    auto load = [&](const BVar& v) {
        return ExpressionBinder::make(iterator, false, range, BLoad{ExpressionBinder::make(iterator, true, range, v)});
    };
    const TypeInfo it_info = types.info(iterator);
    const std::uint32_t elem_cells = std::max<std::uint32_t>(1, types.cells(it_info.target));

    BLoop loop;
    loop.kind = LoopKind::For;
    loop.condition = std::move(*ExpressionBinder::make(
        TypeTable::kBool, false, range, BBinary{BinOp::Ne, ScalarKind::Pointer, load(begin_slot), load(end_slot), 1}));
    BIncDec inc;
    inc.kind = ScalarKind::Pointer;
    inc.elem_cells = elem_cells;
    inc.target = ExpressionBinder::make(iterator, true, range, begin_slot);
    loop.update = std::move(*ExpressionBinder::make(iterator, true, range, std::move(inc)));

    BDeref current;
    current.cells = types.cells(it_info.target);
    current.pointer = load(begin_slot);
    auto element = ExpressionBinder::make(it_info.target, true, range, std::move(current));
    element->is_const = it_info.target_const;

    BBlock body;
    std::vector<BStmt>* body_out = &body.statements;
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    if (!ctx_.declarations->bind_loop_variable(s.variable, std::move(element), range, body_out)) {
        ctx_.symbols.pop();
        return;
    }
    ++ctx_.fn->loop_depth;
    ++ctx_.fn->breakable_depth;
    if (s.body) {
        bind(*s.body, body_out);
    }
    --ctx_.fn->loop_depth;
    --ctx_.fn->breakable_depth;
    ctx_.symbols.pop();
    loop.body = std::make_unique<BStmt>(wrap(range, std::move(body)));
    outer.statements.push_back(wrap(range, std::move(loop)));
    out.push_back(wrap(range, std::move(outer)));
}

void StatementBinder::bind_return(const ast::ReturnStmt& s, SourceRange range, std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    FunctionContext& fn = *ctx_.fn;
    const FunctionInfo& info = ctx_.functions[fn.function];
    const TypeRef ret = fn.return_type;
    const std::size_t mark = fn.temporaries.size();
    auto finish = [&](std::optional<BExpr> value) {
        BReturn r;
        r.value = std::move(value);
        r.cleanup = release_temporaries(mark);
        out.push_back(wrap(range, std::move(r)));
    };

    if (fn.deduce_return) {
        // A lambda without `-> T`, or `auto f()`: the first return statement decides.
        fn.deduce_return = false;
        TypeRef deduced = TypeTable::kVoid;
        if (s.value) {
            auto probe = ctx_.expressions->bind_rvalue(*s.value);
            if (!probe) {
                return;
            }
            if (types.is_record(probe->type)) {
                ctx_.report(DiagnosticFactory::unsupported_type(s.value->range, "deducing a class return type"));
                return;
            }
            deduced = probe->type;
            fn.return_type = deduced;
            ctx_.functions[fn.function].return_type = deduced;
            if (types.is_void(deduced)) {
                // `return say(n);` returns nothing: evaluate the call, then return without a value.
                out.push_back(wrap(range, BExprStmt{std::move(*probe)}));
                finish(std::nullopt);
                return;
            }
            finish(std::move(*probe));
            return;
        }
        fn.return_type = deduced;
        ctx_.functions[fn.function].return_type = deduced;
        finish(std::nullopt);
        return;
    }
    if (fn.function == 0) {
        // Top-level `return`: ends the program (its value, if any, is ignored).
        if (s.value) {
            auto value = ctx_.expressions->bind(*s.value, ExpressionBinder::Use::Statement);
            if (!value) {
                return;
            }
            out.push_back(wrap(range, BExprStmt{std::move(*value)}));
        }
        finish(std::nullopt);
        return;
    }
    if (types.is_void(ret)) {
        if (s.value) {
            auto value = ctx_.expressions->bind(*s.value, ExpressionBinder::Use::Statement);
            if (!value) {
                return;
            }
            if (!types.is_void(value->type)) {
                ctx_.report(
                    DiagnosticFactory::return_type_mismatch(range, info.display, "void", types.name(value->type)));
                return;
            }
            out.push_back(wrap(range, BExprStmt{std::move(*value)}));
        }
        finish(std::nullopt);
        return;
    }
    if (!s.value) {
        ctx_.report(DiagnosticFactory::return_type_mismatch(range, info.display, types.name(ret), "void"));
        return;
    }
    if (types.is_reference(ret)) {
        const TypeInfo ref = types.info(ret);
        auto value = ctx_.expressions->bind(*s.value);
        if (!value) {
            return;
        }
        if (!value->lvalue || value->type != ref.target || (value->is_const && !ref.target_const)) {
            ctx_.report(DiagnosticFactory::reference_needs_lvalue(s.value->range, types.name(ret)));
            return;
        }
        auto address = ExpressionBinder::make(types.pointer_to(ref.target, ref.target_const), false, range,
                                              BAddressOf{std::move(value)});
        finish(std::move(*address));
        return;
    }
    if (types.is_record(ret)) {
        // Construct the result where the caller asked for it.
        BVar slot;
        // Every function returning a record has a result slot.
        slot.offset = *fn.result_slot;  // NOLINT(bugprone-unchecked-optional-access)
        auto pointer = ExpressionBinder::make(types.pointer_to(ret), true, range, slot);
        BDeref deref;
        deref.cells = types.cells(ret);
        deref.pointer = ExpressionBinder::make(types.pointer_to(ret), false, range, BLoad{std::move(pointer)});
        auto target = ExpressionBinder::make(ret, true, range, std::move(deref));
        bool ok = false;
        if (const auto* list = std::get_if<ast::InitList>(&s.value->node)) {
            ok = ctx_.initializers->initialize_list(std::move(target), ret, *list, s.value->range, out);
        } else {
            auto value = ctx_.expressions->bind(*s.value);
            if (!value) {
                return;
            }
            // `return local;` moves the local out (it is about to be destroyed).
            if (const auto* var = std::get_if<BVar>(&value->node);
                var != nullptr && !var->global && !var->reference && value->type == ret) {
                value->expiring = true;
            }
            ok = ctx_.initializers->initialize_from(std::move(target), ret, std::move(value), out);
        }
        if (ok) {
            finish(std::nullopt);
        }
        return;
    }
    BExprPtr value;
    if (const auto* list = std::get_if<ast::InitList>(&s.value->node)) {
        if (list->elements.size() != 1) {
            ctx_.report(DiagnosticFactory::too_many_initializers(s.value->range, 1, list->elements.size()));
            return;
        }
        value = ctx_.expressions->bind_rvalue(list->elements.front());
    } else {
        value = ctx_.expressions->bind_rvalue(*s.value);
    }
    if (!value) {
        return;
    }
    if (ctx_.conversions.rank(*value, ret) == ConversionRank::None || types.is_record(value->type)) {
        ctx_.report(DiagnosticFactory::return_type_mismatch(s.value->range, info.display, types.name(ret),
                                                            types.name(value->type)));
        return;
    }
    value = ctx_.conversions.convert(std::move(value), ret);
    finish(std::move(*value));
}

void StatementBinder::bind_switch(const ast::SwitchStmt& s, SourceRange range, std::vector<BStmt>& out) {
    const TypeTable& types = ctx_.types;
    ctx_.symbols.push(Scope::Kind::Block, range.end);
    BBlock outer;
    std::optional<BExpr> cond;
    const std::size_t mark = ctx_.fn->temporaries.size();
    if (s.condition.decl) {
        auto c = bind_condition(s.condition, outer.statements);
        (void)c;
        Symbol* symbol = ctx_.symbols.current().find(s.condition.decl->vars.front().declarator.name);
        if (symbol != nullptr && symbol->kind == SymbolKind::Variable) {
            auto var = ctx_.expressions->variable(*symbol, s.condition.decl->vars.front().declarator.name, range);
            auto rv = ctx_.expressions->to_rvalue(std::move(var));
            cond = std::move(*rv);
        }
    } else if (s.condition.expr) {
        auto c = ctx_.expressions->bind_rvalue(*s.condition.expr);
        if (c) {
            cond = std::move(*c);
        }
    }
    if (cond && !types.is_integral(cond->type) && !types.is_enum(cond->type)) {
        ctx_.report(DiagnosticFactory::cannot_convert(cond->range, types.name(cond->type), "int"));
        cond.reset();
    }
    TypeRef cond_type = TypeTable::kInt;
    if (cond) {
        cond_type = types.is_scoped_enum(cond->type) ? cond->type : ctx_.conversions.promoted(cond->type);
        auto converted = ctx_.conversions.convert(std::make_unique<BExpr>(std::move(*cond)), cond_type);
        cond = std::move(*converted);
    }

    BSwitch node;
    node.condition_cleanup = release_temporaries(mark);
    std::set<std::int64_t> seen;
    ++ctx_.fn->breakable_depth;
    bool ok = cond.has_value();
    for (std::uint32_t i = 0; i < s.cases.size(); ++i) {
        const ast::SwitchCase& c = s.cases[i];
        if (!c.value) {
            if (node.default_section) {
                ctx_.report(DiagnosticFactory::duplicate_case(c.range, 0));
                ok = false;
            }
            node.default_section = i;
        } else {
            auto value = ctx_.expressions->bind_rvalue(*c.value);
            std::optional<std::int64_t> constant;
            if (value) {
                if (ctx_.conversions.rank(*value, cond_type) == ConversionRank::None) {
                    ctx_.report(DiagnosticFactory::cannot_convert(c.value->range, types.name(value->type),
                                                                  types.name(cond_type)));
                    ok = false;
                } else {
                    value = ctx_.conversions.convert(std::move(value), cond_type);
                    constant = ctx_.constants.integer(*value);
                    if (!constant) {
                        ctx_.report(DiagnosticFactory::not_constant(c.value->range));
                        ok = false;
                    }
                }
            } else {
                ok = false;
            }
            if (constant) {
                if (!seen.insert(*constant).second) {
                    ctx_.report(DiagnosticFactory::duplicate_case(c.value->range, *constant));
                    ok = false;
                }
                node.labels.emplace_back(*constant, i);
            }
        }
        std::vector<BStmt> section;
        bind_statements(c.statements, section);
        node.sections.push_back(std::move(section));
    }
    --ctx_.fn->breakable_depth;
    ctx_.symbols.pop();
    if (!ok || !cond) {
        return;
    }
    node.condition = std::move(*cond);
    if (outer.statements.empty()) {
        out.push_back(wrap(range, std::move(node)));
        return;
    }
    outer.statements.push_back(wrap(range, std::move(node)));
    out.push_back(wrap(range, std::move(outer)));
}

void StatementBinder::bind_static_assert(const ast::StaticAssert& s, SourceRange range) {
    if (!s.condition) {
        return;
    }
    auto cond = ctx_.expressions->bind_condition(*s.condition);
    if (!cond) {
        return;
    }
    auto value = ctx_.constants.integer(*cond);
    if (!value) {
        ctx_.report(DiagnosticFactory::not_constant(s.condition->range));
        return;
    }
    if (*value == 0) {
        ctx_.report(DiagnosticFactory::static_assertion_failed(range, s.message));
    }
}

// =============================================================================
// Exceptions
// =============================================================================

void StatementBinder::bind_throw(const ast::ThrowStmt& s, SourceRange range, std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    ctx_.program.functions[ctx_.fn->function].throws = true;
    if (!s.value) {
        out.push_back(wrap(range, BThrow{}));  // `throw;`
        return;
    }
    const std::size_t temps_mark = ctx_.fn->temporaries.size();
    auto value = ctx_.expressions->bind(*s.value);
    if (!value) {
        drop_temporaries(temps_mark);
        return;
    }
    if (!types.is_record(value->type)) {
        value = ctx_.expressions->to_rvalue(std::move(value));  // arrays decay: `throw "text"` throws a pointer
    }
    const TypeRef type = value->type;
    if (types.is_void(type) || types.kind(type) == TypeKind::Nullptr ||
        (types.is_record(type) && types.record(type).is_abstract)) {
        ctx_.report(DiagnosticFactory::unsupported_type(value->range, types.name(type)));
        drop_temporaries(temps_mark);
        return;
    }
    // The exception object: a heap copy of the value, alive until handled.
    BNew node;
    node.elem_cells = std::max<std::uint32_t>(1, types.cells(type));
    BThrow t;
    t.thrown = ctx_.declarations->thrown_index(type);
    if (types.is_record(type)) {
        node.record = types.info(type).decl;
        const std::uint32_t slot = ctx_.allocate_local(1);
        BVar pointer;
        pointer.offset = slot;
        const TypeRef pointer_type = types.pointer_to(type);
        out.push_back(wrap(range, BStore{ExpressionBinder::make(pointer_type, true, range, pointer),
                                         ExpressionBinder::make(pointer_type, false, range, std::move(node))}));
        auto loaded = ExpressionBinder::make(pointer_type, false, range,
                                             BLoad{ExpressionBinder::make(pointer_type, true, range, pointer)});
        BDeref deref;
        deref.cells = types.cells(type);
        deref.pointer = std::move(loaded);
        auto object = ExpressionBinder::make(type, true, range, std::move(deref));
        if (!ctx_.initializers->initialize_from(std::move(object), type, std::move(value), out)) {
            drop_temporaries(temps_mark);
            return;
        }
        t.object = ExpressionBinder::make(pointer_type, false, range,
                                          BLoad{ExpressionBinder::make(pointer_type, true, range, pointer)});
    } else {
        node.scalar_init = std::move(value);
        t.object = ExpressionBinder::make(types.pointer_to(type), false, range, std::move(node));
    }
    for (auto& cleanup : release_temporaries(temps_mark)) {
        out.push_back(std::move(cleanup));
    }
    out.push_back(wrap(range, std::move(t)));
}

void StatementBinder::bind_try(const ast::TryStmt& s, SourceRange range, std::vector<BStmt>& out) {
    BTry t;
    t.body = bind_substatement(s.body.get(), s.body->range);
    t.table = static_cast<std::uint32_t>(ctx_.program.catch_tables.size());
    ctx_.program.catch_tables.emplace_back();
    for (const auto& clause : s.catches) {
        auto handler = bind_catch(clause, t.table);
        if (!handler) {
            return;
        }
        t.handlers.push_back(std::move(*handler));
    }
    out.push_back(wrap(range, std::move(t)));
}

std::optional<BStmt> StatementBinder::bind_catch(const ast::CatchClause& clause, std::uint32_t table) {
    TypeTable& types = ctx_.types;
    CatchClauseInfo info;
    BBlock handling;  // its cleanup ends the handling of the exception
    handling.cleanup.push_back(wrap(clause.range, BEndCatch{}));
    ctx_.symbols.push(Scope::Kind::Block, clause.range.end);
    bool ok = true;
    std::vector<BStmt>* current = &handling.statements;
    if (!clause.param) {
        info.catch_all = true;
    } else {
        const ast::Param& param = *clause.param;
        auto resolved = ctx_.type_resolver->resolve(param.type, param.declarator);
        ok = resolved.has_value() && !resolved->is_auto;
        if (ok) {
            const bool reference = types.is_reference(resolved->type);
            const TypeRef object_type = reference ? types.info(resolved->type).target : resolved->type;
            const bool is_const = reference ? types.info(resolved->type).target_const : resolved->is_const;
            info.type = object_type;
            if (types.is_void(object_type) || types.is_array(object_type) ||
                (!reference && types.is_record(object_type) && types.record(object_type).is_abstract)) {
                ctx_.report(DiagnosticFactory::unsupported_type(param.range, types.name(object_type)));
                ok = false;
            }
            if (ok && !param.declarator.name.empty()) {
                // The handled object, adjusted to the caught class when it is a base.
                const TypeRef pointer_type = types.pointer_to(object_type, is_const);
                BIntrinsic caught;
                caught.which = static_cast<std::uint8_t>(Intrinsic::Caught);
                auto pointer = ExpressionBinder::make(pointer_type, false, param.range, std::move(caught));
                Symbol symbol;
                symbol.kind = SymbolKind::Variable;
                symbol.range = param.declarator.range;
                symbol.type = object_type;
                symbol.is_const = is_const;
                if (reference) {
                    symbol.reference = true;
                    symbol.offset = ctx_.allocate_local(1);
                    BVar slot;
                    slot.offset = symbol.offset;
                    current->push_back(wrap(
                        param.range,
                        BStore{ExpressionBinder::make(pointer_type, true, param.range, slot), std::move(pointer)}));
                } else {
                    const std::uint32_t cells = std::max<std::uint32_t>(1, types.cells(object_type));
                    symbol.offset = ctx_.allocate_local(cells);
                    BVar slot;
                    slot.offset = symbol.offset;
                    slot.cells = cells;
                    slot.name = param.declarator.name;
                    BDeref deref;
                    deref.cells = types.cells(object_type);
                    deref.pointer = std::move(pointer);
                    auto source = ExpressionBinder::make(object_type, true, param.range, std::move(deref));
                    source->is_const = is_const;
                    if (!types.is_record(object_type)) {
                        source = ctx_.expressions->to_rvalue(std::move(source));
                    }
                    ok =
                        ctx_.initializers->initialize_from(ExpressionBinder::make(object_type, true, param.range, slot),
                                                           object_type, std::move(source), *current);
                    if (ok && ctx_.initializers->needs_destruction(object_type)) {
                        BBlock scope;
                        ctx_.initializers->destroy(ExpressionBinder::make(object_type, true, param.range, slot),
                                                   object_type, scope.cleanup);
                        current->push_back(wrap(param.range, std::move(scope)));
                        current = &std::get<BBlock>(current->back().node).statements;
                    }
                }
                ctx_.symbols.current().add(param.declarator.name, symbol);
                ctx_.program.functions[ctx_.fn->function].locals.push_back(
                    LocalDebug{param.declarator.name, symbol.offset, object_type, reference,
                               SourceRange{param.declarator.range.begin, clause.range.end}});
            }
        }
    }
    if (ok) {
        if (const auto* block = std::get_if<ast::Block>(&clause.body->node)) {
            for (const auto& inner : block->statements) {
                bind(inner, current);
            }
        }
    }
    ctx_.symbols.pop();
    if (!ok) {
        return std::nullopt;
    }
    ctx_.program.catch_tables[table].push_back(std::move(info));
    return wrap(clause.range, std::move(handling));
}

}  // namespace cppi::sema
