#include "sema/ConstEvaluator.hpp"

#include "sema/ConstexprInterpreter.hpp"
#include "support/CheckedArithmetic.hpp"

#include <bit>
#include <limits>
#include <variant>

namespace cppi::sema {

bool ConstEvaluator::fits(std::int64_t value, TypeRef type) const {
    const TypeKind k = types_.kind(type);
    if (k == TypeKind::Long || k == TypeKind::ULong) {
        return true;
    }
    if (k == TypeKind::UInt) {
        return value >= 0 && value <= 0xFFFFFFFF;
    }
    return value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max();
}

std::optional<std::int64_t> ConstEvaluator::integer(const BExpr& expr) const {
    auto v = eval(expr);
    if (!v) {
        return std::nullopt;
    }
    if (v->is_double) {
        return static_cast<std::int64_t>(v->d);
    }
    return v->i;
}

std::optional<ConstEvaluator::Value> ConstEvaluator::convert(Value v, Conv conv) {
    switch (conv) {
        case Conv::IntToDouble: return Value{true, 0, static_cast<double>(v.i)};
        case Conv::ULongToDouble: return Value{true, 0, static_cast<double>(static_cast<std::uint64_t>(v.i))};
        // Out of range (or NaN, which every comparison rejects) is undefined
        // behavior: not a constant, so the VM's DoubleToInt reports it (same ranges).
        case Conv::DoubleToInt32:
            if (v.d > -2147483649.0 && v.d < 2147483648.0) return Value{false, static_cast<std::int64_t>(v.d), 0};
            return std::nullopt;
        case Conv::DoubleToInt64:
            if (v.d >= -9223372036854775808.0 && v.d < 9223372036854775808.0) {
                return Value{false, static_cast<std::int64_t>(v.d), 0};
            }
            return std::nullopt;
        case Conv::DoubleToUInt32:
            if (v.d > -1.0 && v.d < 4294967296.0) return Value{false, static_cast<std::int64_t>(v.d), 0};
            return std::nullopt;
        case Conv::DoubleToUInt64:
            if (v.d > -1.0 && v.d < 18446744073709551616.0) {
                return Value{false, static_cast<std::int64_t>(static_cast<std::uint64_t>(v.d)), 0};
            }
            return std::nullopt;
        case Conv::Trunc16: return Value{false, static_cast<std::int16_t>(v.i & 0xFFFF), 0};
        case Conv::TruncU8: return Value{false, v.i & 0xFF, 0};
        case Conv::TruncU16: return Value{false, v.i & 0xFFFF, 0};
        case Conv::TruncU32: return Value{false, v.i & 0xFFFFFFFF, 0};
        case Conv::IntToBool: return Value{false, v.i != 0 ? 1 : 0, 0};
        case Conv::DoubleToBool: return Value{false, v.d != 0 ? 1 : 0, 0};
        case Conv::Trunc32: return Value{false, static_cast<std::int32_t>(static_cast<std::uint32_t>(v.i)), 0};
        case Conv::Trunc8: return Value{false, static_cast<std::int8_t>(v.i & 0xFF), 0};
        case Conv::Retype: return v;
        default: return std::nullopt;
    }
}

std::optional<ConstEvaluator::Value> ConstEvaluator::eval(const BExpr& expr, Frame* frame) const {
    const bool is_double = types_.kind(expr.type) == TypeKind::Double;
    if (const auto* c = std::get_if<BConst>(&expr.node)) {
        if (types_.kind(expr.type) == TypeKind::Pointer) {
            return std::nullopt;
        }
        return is_double ? Value{true, 0, std::bit_cast<double>(c->bits)} : Value{false, c->bits, 0};
    }
    auto from_bits = [&](std::int64_t bits) {
        return is_double ? Value{true, 0, std::bit_cast<double>(bits)} : Value{false, bits, 0};
    };
    if (const auto* load = std::get_if<BLoad>(&expr.node)) {
        if (const auto* var = std::get_if<BVar>(&load->lvalue->node); var != nullptr && var->constant) {
            return from_bits(*var->constant);
        }
        if (frame != nullptr && interpreter_ != nullptr) {
            if (auto bits = interpreter_->load(*load->lvalue, *frame)) {
                return from_bits(*bits);
            }
        }
        return std::nullopt;
    }
    if (frame != nullptr && interpreter_ != nullptr) {
        // Expressions with effects, only inside a constexpr function being evaluated.
        if (std::holds_alternative<BAssign>(expr.node) || std::holds_alternative<BIncDec>(expr.node) ||
            std::holds_alternative<BComma>(expr.node)) {
            auto bits = interpreter_->effect(expr, *frame);
            return bits ? std::optional<Value>(from_bits(*bits)) : std::nullopt;
        }
    }
    if (const auto* call = std::get_if<BCall>(&expr.node)) {
        if (interpreter_ == nullptr || call->virtual_slot || call->result_temp || call->result_target) {
            return std::nullopt;
        }
        std::vector<std::int64_t> args;
        for (const auto& a : call->args) {
            auto v = eval(*a, frame);
            if (!v) {
                return std::nullopt;
            }
            args.push_back(v->is_double ? std::bit_cast<std::int64_t>(v->d) : v->i);
        }
        auto bits = interpreter_->call(call->function, args);
        return bits ? std::optional<Value>(from_bits(*bits)) : std::nullopt;
    }
    if (const auto* conv = std::get_if<BConvert>(&expr.node)) {
        auto v = eval(*conv->operand, frame);
        if (!v) {
            return std::nullopt;
        }
        return convert(*v, conv->conv);
    }
    if (const auto* u = std::get_if<BUnary>(&expr.node)) {
        auto v = eval(*u->operand, frame);
        if (!v) {
            return std::nullopt;
        }
        switch (u->op) {
            case UnOp::Neg:
                if (v->is_double) {
                    return Value{true, 0, -v->d};
                }
                if (u->kind == ScalarKind::UInt32 || u->kind == ScalarKind::UInt64) {
                    const std::uint64_t r = 0 - static_cast<std::uint64_t>(v->i);
                    return Value{false, static_cast<std::int64_t>(u->kind == ScalarKind::UInt32 ? r & 0xFFFFFFFFU : r),
                                 0};
                }
                if (v->i == std::numeric_limits<std::int64_t>::min() || !fits(-v->i, expr.type)) {
                    return std::nullopt;
                }
                return Value{false, -v->i, 0};
            case UnOp::BitNot:
                if (u->kind == ScalarKind::UInt32) {
                    return Value{false, ~v->i & 0xFFFFFFFF, 0};
                }
                return Value{false, fits(~v->i, expr.type) || u->kind == ScalarKind::UInt64 ? ~v->i : 0, 0};
            case UnOp::Not: return Value{false, v->i == 0 ? 1 : 0, 0};
        }
    }
    if (const auto* b = std::get_if<BBinary>(&expr.node)) {
        return binary(*b, expr.type, frame);
    }
    if (const auto* l = std::get_if<BLogical>(&expr.node)) {
        auto lhs = eval(*l->lhs, frame);
        if (!lhs) {
            return std::nullopt;
        }
        if (l->is_and ? lhs->i == 0 : lhs->i != 0) {
            return Value{false, l->is_and ? 0 : 1, 0};
        }
        auto rhs = eval(*l->rhs, frame);
        if (!rhs) {
            return std::nullopt;
        }
        return Value{false, rhs->i != 0 ? 1 : 0, 0};
    }
    if (const auto* c = std::get_if<BConditional>(&expr.node)) {
        auto cond = eval(*c->condition, frame);
        if (!cond) {
            return std::nullopt;
        }
        return eval(cond->i != 0 ? *c->then_expr : *c->else_expr, frame);
    }
    return std::nullopt;
}

std::optional<ConstEvaluator::Value> ConstEvaluator::binary(const BBinary& b, TypeRef type, Frame* frame) const {
    auto lhs = eval(*b.lhs, frame);
    auto rhs = eval(*b.rhs, frame);
    if (!lhs || !rhs) {
        return std::nullopt;
    }
    return compute(b.op, b.kind, *lhs, *rhs, type);
}

std::optional<ConstEvaluator::Value> ConstEvaluator::compute(BinOp op, ScalarKind kind, Value lhs, Value rhs,
                                                             TypeRef type) const {
    if (kind == ScalarKind::Pointer) {
        return std::nullopt;
    }
    if (kind == ScalarKind::Double) {
        const double x = lhs.d;
        const double y = rhs.d;
        switch (op) {
            case BinOp::Add: return Value{true, 0, x + y};
            case BinOp::Sub: return Value{true, 0, x - y};
            case BinOp::Mul: return Value{true, 0, x * y};
            case BinOp::Div: return y == 0 ? std::nullopt : std::optional<Value>(Value{true, 0, x / y});
            case BinOp::Eq: return Value{false, x == y ? 1 : 0, 0};
            case BinOp::Ne: return Value{false, x != y ? 1 : 0, 0};
            case BinOp::Lt: return Value{false, x < y ? 1 : 0, 0};
            case BinOp::Gt: return Value{false, x > y ? 1 : 0, 0};
            case BinOp::Le: return Value{false, x <= y ? 1 : 0, 0};
            case BinOp::Ge: return Value{false, x >= y ? 1 : 0, 0};
            default: return std::nullopt;
        }
    }
    if (kind == ScalarKind::UInt32 || kind == ScalarKind::UInt64) {
        return unsigned_binary(op, kind, lhs.i, rhs.i);
    }
    const std::int64_t x = lhs.i;
    const std::int64_t y = rhs.i;
    std::int64_t r = 0;
    switch (op) {
        case BinOp::Add:
            if (!detail::CheckedArithmetic::add(x, y, r)) return std::nullopt;
            break;
        case BinOp::Sub:
            if (!detail::CheckedArithmetic::sub(x, y, r)) return std::nullopt;
            break;
        case BinOp::Mul:
            if (!detail::CheckedArithmetic::mul(x, y, r)) return std::nullopt;
            break;
        case BinOp::Div:
            if (y == 0 || (x == std::numeric_limits<std::int64_t>::min() && y == -1)) return std::nullopt;
            r = x / y;
            break;
        case BinOp::Mod:
            if (y == 0 || (x == std::numeric_limits<std::int64_t>::min() && y == -1)) return std::nullopt;
            r = x % y;
            break;
        case BinOp::Shl:
            if (y < 0 || y >= (kind == ScalarKind::Int64 ? 64 : 32) || x < 0) return std::nullopt;
            r = static_cast<std::int64_t>(static_cast<std::uint64_t>(x) << y);
            break;
        case BinOp::Shr:
            if (y < 0 || y >= (kind == ScalarKind::Int64 ? 64 : 32)) return std::nullopt;
            r = x >> y;
            break;
        case BinOp::BitAnd: r = x & y; break;
        case BinOp::BitOr: r = x | y; break;
        case BinOp::BitXor: r = x ^ y; break;
        case BinOp::Eq: return Value{false, x == y ? 1 : 0, 0};
        case BinOp::Ne: return Value{false, x != y ? 1 : 0, 0};
        case BinOp::Lt: return Value{false, x < y ? 1 : 0, 0};
        case BinOp::Gt: return Value{false, x > y ? 1 : 0, 0};
        case BinOp::Le: return Value{false, x <= y ? 1 : 0, 0};
        case BinOp::Ge: return Value{false, x >= y ? 1 : 0, 0};
        default: return std::nullopt;
    }
    if (!fits(r, type)) {
        return std::nullopt;
    }
    return Value{false, r, 0};
}

std::optional<ConstEvaluator::Value> ConstEvaluator::unsigned_binary(BinOp op, ScalarKind kind, std::int64_t lhs,
                                                                     std::int64_t rhs) {
    const bool wide = kind == ScalarKind::UInt64;
    const auto x = static_cast<std::uint64_t>(lhs);
    const auto y = static_cast<std::uint64_t>(rhs);
    std::uint64_t r = 0;
    switch (op) {
        case BinOp::Add: r = x + y; break;
        case BinOp::Sub: r = x - y; break;
        case BinOp::Mul: r = x * y; break;
        case BinOp::Div:
            if (y == 0) return std::nullopt;
            r = x / y;
            break;
        case BinOp::Mod:
            if (y == 0) return std::nullopt;
            r = x % y;
            break;
        case BinOp::Shl:
        case BinOp::Shr:
            if (rhs < 0 || rhs >= (wide ? 64 : 32)) return std::nullopt;
            r = op == BinOp::Shl ? x << y : x >> y;
            break;
        case BinOp::BitAnd: r = x & y; break;
        case BinOp::BitOr: r = x | y; break;
        case BinOp::BitXor: r = x ^ y; break;
        case BinOp::Eq: return Value{false, x == y ? 1 : 0, 0};
        case BinOp::Ne: return Value{false, x != y ? 1 : 0, 0};
        case BinOp::Lt: return Value{false, x < y ? 1 : 0, 0};
        case BinOp::Gt: return Value{false, x > y ? 1 : 0, 0};
        case BinOp::Le: return Value{false, x <= y ? 1 : 0, 0};
        case BinOp::Ge: return Value{false, x >= y ? 1 : 0, 0};
        default: return std::nullopt;
    }
    return Value{false, static_cast<std::int64_t>(wide ? r : r & 0xFFFFFFFFU), 0};
}

}  // namespace cppi::sema
