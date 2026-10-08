#include "sema/ConstexprInterpreter.hpp"

#include "support/Overloaded.hpp"

#include <bit>
#include <variant>

namespace cppi::sema {

namespace {

/// A type for the overflow checks of an operation done in `kind`.
TypeRef type_of(ScalarKind kind) {
    switch (kind) {
        case ScalarKind::Int32: return TypeTable::kInt;
        case ScalarKind::Int64: return TypeTable::kLong;
        case ScalarKind::Double: return TypeTable::kDouble;
        case ScalarKind::UInt32: return TypeTable::kUInt;
        case ScalarKind::UInt64: return TypeTable::kULong;
        case ScalarKind::Pointer: break;
    }
    return TypeTable::kError;
}

}  // namespace

std::int64_t ConstexprInterpreter::to_bits(const ConstEvaluator::Value& v) {
    return v.is_double ? std::bit_cast<std::int64_t>(v.d) : v.i;
}

std::optional<ConstEvaluator::Value> ConstexprInterpreter::as_value(std::int64_t bits, TypeRef type) const {
    if (types_.kind(type) == TypeKind::Double) {
        return ConstEvaluator::Value{true, 0, std::bit_cast<double>(bits)};
    }
    if (!types_.is_scalar(type) || types_.is_pointer(type)) {
        return std::nullopt;
    }
    return ConstEvaluator::Value{false, bits, 0};
}

std::optional<std::int64_t> ConstexprInterpreter::call(std::uint32_t function, const std::vector<std::int64_t>& args) {
    if (function >= functions_.size() || function >= program_.functions.size()) {
        return std::nullopt;
    }
    const FunctionInfo& fn = functions_[function];
    const BoundFunction& bound = program_.functions[function];
    const bool has_this = fn.record && !fn.is_static;
    if (!fn.is_constexpr || !fn.defined || has_this || !types_.is_scalar(fn.return_type) ||
        types_.is_pointer(fn.return_type) || args.size() != fn.params.size() || bound.params.size() != args.size()) {
        return std::nullopt;
    }
    for (const ParamInfo& p : fn.params) {
        if (!types_.is_scalar(p.type) || types_.is_pointer(p.type)) {
            return std::nullopt;  // references, pointers and objects need memory
        }
    }
    if (depth_ == 0) {
        steps_ = 0;
    }
    if (depth_ >= kMaxDepth) {
        return std::nullopt;
    }
    ConstEvaluator::Frame frame(bound.frame_cells);
    for (std::size_t i = 0; i < args.size(); ++i) {
        const ParamSlot& slot = bound.params[i];
        if (slot.copy_block || slot.offset >= frame.size()) {
            return std::nullopt;
        }
        frame[slot.offset] = args[i];
    }
    ++depth_;
    std::optional<std::int64_t> result;
    const Flow flow = bound.body.cleanup.empty() ? run_all(bound.body.statements, frame, result) : Flow::Fail;
    --depth_;
    if (flow != Flow::Return) {
        return std::nullopt;  // failed, or flowed off the end
    }
    return result;
}

ConstexprInterpreter::Flow ConstexprInterpreter::run_all(const std::vector<BStmt>& list, ConstEvaluator::Frame& frame,
                                                         std::optional<std::int64_t>& result) {
    for (const BStmt& s : list) {
        const Flow flow = run(s, frame, result);
        if (flow != Flow::Normal) {
            return flow;
        }
    }
    return Flow::Normal;
}

ConstexprInterpreter::Flow ConstexprInterpreter::run(const BStmt& stmt, ConstEvaluator::Frame& frame,
                                                     std::optional<std::int64_t>& result) {
    if (!tick()) {
        return Flow::Fail;
    }
    auto test = [&](const BExpr& condition) -> std::optional<bool> {
        auto v = value(condition, frame);
        if (!v) {
            return std::nullopt;
        }
        return *v != 0;
    };
    return std::visit(
        detail::Overloaded{
            [&](const BExprStmt& s) { return value(s.expr, frame) ? Flow::Normal : Flow::Fail; },
            [&](const BStore& s) {
                auto c = cell(*s.target, frame);
                auto v = value(*s.value, frame);
                if (!c || !v) {
                    return Flow::Fail;
                }
                frame[*c] = v;
                return Flow::Normal;
            },
            [&](const BZero& s) {
                auto c = cell(*s.target, frame);
                if (!c || *c + s.cells > frame.size()) {
                    return Flow::Fail;
                }
                for (std::uint32_t i = 0; i < s.cells; ++i) {
                    frame[*c + i] = 0;
                }
                return Flow::Normal;
            },
            [&](const BUninit& s) {
                auto c = cell(*s.target, frame);
                if (!c || *c + s.cells > frame.size()) {
                    return Flow::Fail;
                }
                for (std::uint32_t i = 0; i < s.cells; ++i) {
                    frame[*c + i].reset();
                }
                return Flow::Normal;
            },
            [&](const BBlock& b) { return b.cleanup.empty() ? run_all(b.statements, frame, result) : Flow::Fail; },
            [&](const BIf& s) {
                auto c = test(s.condition);
                if (!c || !s.condition_cleanup.empty()) {
                    return Flow::Fail;
                }
                if (*c) {
                    return run(*s.then_stmt, frame, result);
                }
                return s.else_stmt ? run(*s.else_stmt, frame, result) : Flow::Normal;
            },
            [&](const BLoop& s) {
                if (!s.condition_cleanup.empty() || !s.update_cleanup.empty()) {
                    return Flow::Fail;
                }
                bool first = true;
                while (true) {
                    if (!tick()) {
                        return Flow::Fail;
                    }
                    const bool check = s.condition && (s.kind != LoopKind::DoWhile || !first);
                    first = false;
                    if (check) {
                        auto c = test(*s.condition);
                        if (!c) {
                            return Flow::Fail;
                        }
                        if (!*c) {
                            return Flow::Normal;
                        }
                    }
                    const Flow flow = run(*s.body, frame, result);
                    if (flow == Flow::Break) {
                        return Flow::Normal;
                    }
                    if (flow == Flow::Return || flow == Flow::Fail) {
                        return flow;
                    }
                    if (s.update && !value(*s.update, frame)) {
                        return Flow::Fail;
                    }
                }
            },
            [&](const BBreak&) { return Flow::Break; }, [&](const BContinue&) { return Flow::Continue; },
            [&](const BReturn& s) {
                if (!s.value || !s.cleanup.empty()) {
                    return Flow::Fail;
                }
                result = value(*s.value, frame);
                return result ? Flow::Return : Flow::Fail;
            },
            [&](const BSwitch& s) {
                auto v = value(s.condition, frame);
                if (!v || !s.condition_cleanup.empty()) {
                    return Flow::Fail;
                }
                std::optional<std::uint32_t> start = s.default_section;
                for (const auto& [label, section] : s.labels) {
                    if (label == *v) {
                        start = section;
                    }
                }
                if (!start) {
                    return Flow::Normal;
                }
                for (std::size_t i = *start; i < s.sections.size(); ++i) {
                    const Flow flow = run_all(s.sections[i], frame, result);
                    if (flow == Flow::Break) {
                        return Flow::Normal;
                    }
                    if (flow != Flow::Normal) {
                        return flow;
                    }
                }
                return Flow::Normal;
            },
            [&](const auto&) { return Flow::Fail; },  // objects: copies, headers
        },
        stmt.node);
}

std::optional<std::int64_t> ConstexprInterpreter::value(const BExpr& expr, ConstEvaluator::Frame& frame) const {
    auto v = evaluator_.eval(expr, &frame);
    if (!v) {
        return std::nullopt;
    }
    return to_bits(*v);
}

std::optional<std::uint32_t> ConstexprInterpreter::cell(const BExpr& lvalue, ConstEvaluator::Frame& frame) const {
    if (const auto* var = std::get_if<BVar>(&lvalue.node)) {
        if (var->global || var->reference || var->offset >= frame.size()) {
            return std::nullopt;
        }
        return var->offset;
    }
    if (const auto* index = std::get_if<BIndex>(&lvalue.node)) {
        if (index->base_is_pointer || index->bound == 0) {
            return std::nullopt;
        }
        auto base = cell(*index->base, frame);
        auto i = value(*index->index, frame);
        if (!base || !i || *i < 0 || static_cast<std::uint64_t>(*i) >= index->bound) {
            return std::nullopt;  // out of bounds is not a constant either
        }
        const std::uint64_t at = *base + static_cast<std::uint64_t>(*i) * index->elem_cells;
        if (at >= frame.size()) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(at);
    }
    return std::nullopt;
}

std::optional<std::int64_t> ConstexprInterpreter::load(const BExpr& lvalue, ConstEvaluator::Frame& frame) const {
    auto c = cell(lvalue, frame);
    if (!c) {
        return std::nullopt;
    }
    return frame[*c];  // nullopt if never initialized: not a constant
}

std::optional<std::int64_t> ConstexprInterpreter::effect(const BExpr& expr, ConstEvaluator::Frame& frame) {
    if (!tick()) {
        return std::nullopt;
    }
    if (const auto* comma = std::get_if<BComma>(&expr.node)) {
        if (!value(*comma->lhs, frame)) {
            return std::nullopt;
        }
        return value(*comma->rhs, frame);
    }
    if (const auto* assign = std::get_if<BAssign>(&expr.node)) {
        if (assign->record_cells != 0 || assign->kind == ScalarKind::Pointer) {
            return std::nullopt;
        }
        auto c = cell(*assign->target, frame);
        auto rhs = value(*assign->value, frame);
        if (!c || !rhs) {
            return std::nullopt;
        }
        if (!assign->op) {
            frame[*c] = rhs;
            return rhs;
        }
        const TypeRef op_type = type_of(assign->kind);
        if (!frame[*c]) {
            return std::nullopt;
        }
        auto old = as_value(*frame[*c], assign->target->type);
        auto right = as_value(*rhs, op_type);
        if (!old || !right) {
            return std::nullopt;
        }
        for (const Conv conv : assign->load_conv) {
            old = ConstEvaluator::convert(*old, conv);
            if (!old) {
                return std::nullopt;
            }
        }
        const auto computed = evaluator_.compute(*assign->op, assign->kind, *old, *right, op_type);
        if (!computed) {
            return std::nullopt;
        }
        ConstEvaluator::Value result = *computed;
        for (const Conv conv : assign->store_conv) {
            const auto next = ConstEvaluator::convert(result, conv);
            if (!next) {
                return std::nullopt;
            }
            result = *next;
        }
        frame[*c] = to_bits(result);
        return frame[*c];
    }
    if (const auto* inc = std::get_if<BIncDec>(&expr.node)) {
        if (inc->kind == ScalarKind::Pointer) {
            return std::nullopt;
        }
        auto c = cell(*inc->target, frame);
        if (!c || !frame[*c]) {
            return std::nullopt;
        }
        const TypeRef op_type = type_of(inc->kind);
        auto old = as_value(*frame[*c], op_type);
        if (!old) {
            return std::nullopt;
        }
        const ConstEvaluator::Value one =
            inc->kind == ScalarKind::Double ? ConstEvaluator::Value{true, 0, 1.0} : ConstEvaluator::Value{false, 1, 0};
        auto r = evaluator_.compute(inc->increment ? BinOp::Add : BinOp::Sub, inc->kind, *old, one, op_type);
        for (const Conv conv : inc->store_conv) {
            if (!r) {
                return std::nullopt;
            }
            r = ConstEvaluator::convert(*r, conv);
        }
        if (!r) {
            return std::nullopt;
        }
        const std::int64_t before = *frame[*c];
        frame[*c] = to_bits(*r);
        return inc->prefix ? *frame[*c] : before;
    }
    return std::nullopt;
}

}  // namespace cppi::sema
