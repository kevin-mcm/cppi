/// @file Reachability.cpp
/// @brief Implementation of Reachability.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/Reachability.hpp"

#include "support/Overloaded.hpp"

#include <variant>

namespace cppi::detail {

using namespace cppi::sema;

std::vector<bool> Reachability::compute(const BoundProgram& program) {
    Reachability r(program);
    r.functions_.assign(program.functions.size(), false);
    r.records_.assign(program.types->record_count(), false);
    r.function(0);
    while (!r.pending_.empty()) {
        const std::uint32_t id = r.pending_.back();
        r.pending_.pop_back();
        const BoundFunction& fn = program.functions[id];
        r.statements(fn.body.statements);
        r.statements(fn.body.cleanup);
    }
    return r.functions_;
}

void Reachability::function(std::uint32_t id) {
    if (id < functions_.size() && !functions_[id]) {
        functions_[id] = true;
        pending_.push_back(id);
    }
}

void Reachability::record(std::uint32_t id) {
    if (id >= records_.size() || records_[id]) {
        return;
    }
    records_[id] = true;
    // Objects of this class can dispatch to any of its final overriders.
    for (const auto& sub : program_.types->record_at(id).subobjects) {
        for (const auto& slot : sub.slots) {
            function(slot.function);
        }
    }
}

void Reachability::statements(const std::vector<BStmt>& list) {
    for (const BStmt& s : list) {
        statement(s);
    }
}

void Reachability::statement(const BStmt& s) {
    std::visit(Overloaded{
                   [&](const BExprStmt& e) { expression(e.expr); },
                   [&](const BStore& st) {
                       expression(*st.target);
                       expression(*st.value);
                   },
                   [&](const BCopy& c) {
                       expression(*c.target);
                       expression(*c.source);
                   },
                   [&](const BZero& z) { expression(*z.target); },
                   [&](const BUninit& u) { expression(*u.target); },
                   [&](const BInitHeaders& h) {
                       expression(*h.target);
                       record(h.record);
                   },
                   [&](const BBlock& b) {
                       statements(b.statements);
                       statements(b.cleanup);
                   },
                   [&](const BIf& i) {
                       expression(i.condition);
                       statements(i.condition_cleanup);
                       if (i.then_stmt) {
                           statement(*i.then_stmt);
                       }
                       if (i.else_stmt) {
                           statement(*i.else_stmt);
                       }
                   },
                   [&](const BLoop& l) {
                       if (l.condition) {
                           expression(*l.condition);
                       }
                       if (l.update) {
                           expression(*l.update);
                       }
                       statements(l.condition_cleanup);
                       statements(l.update_cleanup);
                       if (l.body) {
                           statement(*l.body);
                       }
                   },
                   [&](const BReturn& r) {
                       if (r.value) {
                           expression(*r.value);
                       }
                       statements(r.cleanup);
                   },
                   [&](const BSwitch& sw) {
                       expression(sw.condition);
                       statements(sw.condition_cleanup);
                       for (const auto& section : sw.sections) {
                           statements(section);
                       }
                   },
                   [&](const BThrow& t) {
                       if (t.object) {
                           expression(*t.object);
                           if (const auto& dtor = program_.thrown[t.thrown].destructor) {
                               function(*dtor);
                           }
                       }
                   },
                   [&](const BTry& t) {
                       statement(*t.body);
                       statements(t.handlers);
                   },
                   [&](const auto&) {},
               },
               s.node);
}

void Reachability::expression(const BExpr& e) {
    auto sub = [&](const BExprPtr& p) {
        if (p) {
            expression(*p);
        }
    };
    std::visit(Overloaded{
                   [&](const BDeref& d) { sub(d.pointer); },
                   [&](const BAddressOf& a) { sub(a.lvalue); },
                   [&](const BMember& m) { sub(m.base); },
                   [&](const BIndex& i) {
                       sub(i.base);
                       sub(i.index);
                   },
                   [&](const BLoad& l) { sub(l.lvalue); },
                   [&](const BUnary& u) { sub(u.operand); },
                   [&](const BBinary& b) {
                       sub(b.lhs);
                       sub(b.rhs);
                   },
                   [&](const BLogical& l) {
                       sub(l.lhs);
                       sub(l.rhs);
                   },
                   [&](const BAssign& a) {
                       sub(a.target);
                       sub(a.value);
                   },
                   [&](const BIncDec& i) { sub(i.target); },
                   [&](const BConditional& c) {
                       sub(c.condition);
                       sub(c.then_expr);
                       sub(c.else_expr);
                   },
                   [&](const BCall& c) {
                       function(c.function);
                       sub(c.result_target);
                       for (const auto& a : c.args) {
                           sub(a);
                       }
                   },
                   [&](const BHostCall& h) {
                       for (const auto& a : h.args) {
                           sub(a);
                       }
                   },
                   [&](const BIntrinsic& in) {
                       for (const auto& a : in.args) {
                           sub(a);
                       }
                   },
                   [&](const BConvert& c) { sub(c.operand); },
                   [&](const BComma& c) {
                       sub(c.lhs);
                       sub(c.rhs);
                   },
                   [&](const BTemp& t) {
                       sub(t.init);
                       statements(t.init_stmts);
                   },
                   [&](const BNew& n) {
                       sub(n.count);
                       sub(n.scalar_init);
                       if (n.constructor) {
                           function(*n.constructor);
                       }
                       if (n.record) {
                           record(*n.record);
                       }
                       for (const auto& a : n.ctor_args) {
                           sub(a);
                       }
                   },
                   [&](const BDelete& d) {
                       sub(d.pointer);
                       if (d.destructor) {
                           function(*d.destructor);
                       }
                   },
                   [&](const auto&) {},
               },
               e.node);
}

}  // namespace cppi::detail
