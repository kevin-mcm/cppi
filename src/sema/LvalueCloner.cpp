/// @file LvalueCloner.cpp
/// @brief Implementation of LvalueCloner.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/LvalueCloner.hpp"

#include "support/Overloaded.hpp"

#include <utility>
#include <variant>

namespace cppi::sema {

BExprPtr LvalueCloner::clone(const BExpr& expr) {
    auto out = std::make_unique<BExpr>();
    out->type = expr.type;
    out->lvalue = expr.lvalue;
    out->is_const = expr.is_const;
    out->expiring = expr.expiring;
    out->range = expr.range;
    bool ok = true;
    auto sub = [&](const BExprPtr& p) -> BExprPtr {
        if (!p) {
            return nullptr;
        }
        auto c = clone(*p);
        ok = ok && c != nullptr;
        return c;
    };
    std::visit(detail::Overloaded{
                   [&](const BConst& c) { out->node = c; },
                   [&](const BVar& v) { out->node = v; },
                   [&](const BDeref& d) { out->node = BDeref{sub(d.pointer), d.cells}; },
                   [&](const BAddressOf& a) { out->node = BAddressOf{sub(a.lvalue)}; },
                   [&](const BMember& m) { out->node = BMember{sub(m.base), m.offset, m.cells, m.is_base}; },
                   [&](const BIndex& i) {
                       out->node = BIndex{sub(i.base), sub(i.index), i.elem_cells, i.bound, i.base_is_pointer};
                   },
                   [&](const BLoad& l) { out->node = BLoad{sub(l.lvalue)}; },
                   [&](const BConvert& c) {
                       BConvert copy;
                       copy.conv = c.conv;
                       copy.operand = sub(c.operand);
                       copy.offset = c.offset;
                       copy.cells = c.cells;
                       copy.virtual_base = c.virtual_base;
                       out->node = std::move(copy);
                   },
                   [&](const BTemp& t) {
                       if (t.init || !t.init_stmts.empty()) {
                           ok = false;
                           return;
                       }
                       out->node = BTemp{t.offset, t.cells, nullptr, {}};
                   },
                   [&](const auto&) { ok = false; },
               },
               expr.node);
    return ok ? std::move(out) : nullptr;
}

}  // namespace cppi::sema
