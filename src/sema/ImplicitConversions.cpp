/// @file ImplicitConversions.cpp
/// @brief Implementation of ImplicitConversions.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/ImplicitConversions.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <utility>
#include <variant>

namespace cppi::sema {

namespace {

bool is_int_like(TypeKind k) {
    return TypeTable::integer_bits(k) > 0;
}

/// The value `v` takes in an integer type of kind `to` (modulo 2^bits).
std::int64_t fold_int(std::int64_t v, TypeKind to) {
    switch (to) {
        case TypeKind::Bool: return v != 0 ? 1 : 0;
        case TypeKind::Char: return static_cast<std::int8_t>(static_cast<std::uint8_t>(v & 0xFF));
        case TypeKind::UChar: return v & 0xFF;
        case TypeKind::Short: return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xFFFF));
        case TypeKind::UShort: return v & 0xFFFF;
        case TypeKind::Int:
        case TypeKind::Enum: return static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(v)));
        case TypeKind::UInt: return v & 0xFFFFFFFF;
        default: return v;
    }
}

/// Can every value of integer kind `from` be represented in `to`?
bool widens(TypeKind from, TypeKind to) {
    const std::uint32_t fb = TypeTable::integer_bits(from);
    const std::uint32_t tb = TypeTable::integer_bits(to);
    const bool fu = from == TypeKind::Bool || TypeTable::is_unsigned_kind(from);
    const bool tu = TypeTable::is_unsigned_kind(to);
    if (tu) {
        return fu && fb <= tb;
    }
    return fu ? fb < tb : fb <= tb;
}

/// The VM conversion that wraps a value into integer kind `to`.
Conv narrowing(TypeKind to) {
    switch (to) {
        case TypeKind::Char: return Conv::Trunc8;
        case TypeKind::UChar: return Conv::TruncU8;
        case TypeKind::Short: return Conv::Trunc16;
        case TypeKind::UShort: return Conv::TruncU16;
        case TypeKind::UInt: return Conv::TruncU32;
        case TypeKind::Int:
        case TypeKind::Enum: return Conv::Trunc32;
        default: return Conv::Retype;  // 64 bits: same bits
    }
}

}  // namespace

ScalarKind ImplicitConversions::scalar_kind(const TypeTable& types, TypeRef type) {
    switch (types.kind(type)) {
        case TypeKind::Long: return ScalarKind::Int64;
        case TypeKind::ULong: return ScalarKind::UInt64;
        case TypeKind::UInt: return ScalarKind::UInt32;
        case TypeKind::Double: return ScalarKind::Double;
        case TypeKind::Pointer:
        case TypeKind::Nullptr: return ScalarKind::Pointer;
        default: return ScalarKind::Int32;
    }
}

bool ImplicitConversions::is_null_pointer_constant(const BExpr& expr, const TypeTable& types) {
    if (types.kind(expr.type) == TypeKind::Nullptr) {
        return true;
    }
    const auto* c = std::get_if<BConst>(&expr.node);
    const TypeKind k = types.kind(expr.type);
    return c != nullptr && c->bits == 0 &&
           (k == TypeKind::Int || k == TypeKind::Long || k == TypeKind::UInt || k == TypeKind::ULong);
}

TypeRef ImplicitConversions::promoted(TypeRef type) const {
    switch (types_.kind(type)) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::UChar:
        case TypeKind::Short:
        case TypeKind::UShort: return TypeTable::kInt;
        case TypeKind::Enum: return types_.is_scoped_enum(type) ? type : TypeTable::kInt;
        default: return type;
    }
}

TypeRef ImplicitConversions::common_arithmetic(TypeRef a, TypeRef b) const {
    const TypeRef pa = promoted(a);
    const TypeRef pb = promoted(b);
    if (types_.kind(pa) == TypeKind::Double || types_.kind(pb) == TypeKind::Double) {
        return TypeTable::kDouble;
    }
    const TypeKind ka = types_.kind(pa);
    const TypeKind kb = types_.kind(pb);
    if (ka == TypeKind::ULong || kb == TypeKind::ULong) {
        return TypeTable::kULong;
    }
    if (ka == TypeKind::Long || kb == TypeKind::Long) {
        return TypeTable::kLong;  // long holds every unsigned int
    }
    if (ka == TypeKind::UInt || kb == TypeKind::UInt) {
        return TypeTable::kUInt;
    }
    return TypeTable::kInt;
}

ConversionRank ImplicitConversions::rank(const BExpr& from, TypeRef to) const {
    const TypeRef f = from.type;
    if (types_.is_error(f) || types_.is_error(to)) {
        return ConversionRank::Exact;  // already reported
    }
    if (f == to) {
        return ConversionRank::Exact;
    }
    const TypeKind fk = types_.kind(f);
    const TypeKind tk = types_.kind(to);

    if (tk == TypeKind::Enum) {
        return ConversionRank::None;  // nothing converts implicitly to an enum
    }
    if (tk == TypeKind::Bool) {
        if (types_.is_arithmetic(f) || fk == TypeKind::Pointer) {
            return ConversionRank::Conversion;
        }
        return ConversionRank::None;
    }
    if (types_.is_arithmetic(to) && types_.is_arithmetic(f)) {
        if (tk == TypeKind::Int && promoted(f) == TypeTable::kInt) {
            return ConversionRank::Promotion;
        }
        return ConversionRank::Conversion;
    }
    if (tk == TypeKind::Pointer) {
        if (is_null_pointer_constant(from, types_)) {
            return ConversionRank::Conversion;
        }
        if (fk != TypeKind::Pointer) {
            return ConversionRank::None;
        }
        const TypeInfo& fi = types_.info(f);
        const TypeInfo& ti = types_.info(to);
        if (fi.target_const && !ti.target_const) {
            return ConversionRank::None;  // would drop const
        }
        if (fi.target == ti.target) {
            return ConversionRank::Exact;  // adds const
        }
        if (types_.is_record(fi.target) && types_.is_record(ti.target)) {
            auto path = hierarchy_.find_base(types_.info(fi.target).decl, types_.info(ti.target).decl);
            if (path && !path->ambiguous) {
                return ConversionRank::Conversion;
            }
        }
        return ConversionRank::None;
    }
    if (tk == TypeKind::Record && fk == TypeKind::Record) {
        auto path = hierarchy_.find_base(types_.info(f).decl, types_.info(to).decl);
        if (path && !path->ambiguous) {
            return ConversionRank::Conversion;  // slicing
        }
    }
    return ConversionRank::None;
}

BExprPtr ImplicitConversions::wrap(BExprPtr expr, Conv conv, TypeRef to) {
    const SourceRange range = expr->range;
    BConvert c;
    c.conv = conv;
    c.operand = std::move(expr);
    auto out = std::make_unique<BExpr>();
    out->type = to;
    out->range = range;
    out->node = std::move(c);
    return out;
}

BExprPtr ImplicitConversions::arithmetic(BExprPtr expr, TypeRef to) const {
    const TypeKind fk = types_.kind(expr->type);
    const TypeKind tk = types_.kind(to);

    if (auto* c = std::get_if<BConst>(&expr->node)) {  // constant folding
        std::int64_t bits = c->bits;
        if (fk == TypeKind::Double) {
            const auto d = std::bit_cast<double>(bits);
            if (tk == TypeKind::Bool) {
                bits = d != 0.0 ? 1 : 0;
            } else if (tk != TypeKind::Double) {
                if (d <= -9.2e18 || d >= 9.2e18 || (TypeTable::is_unsigned_kind(tk) && d <= -1.0)) {
                    return runtime_arithmetic(std::move(expr), to);  // let the VM report it
                }
                bits = fold_int(static_cast<std::int64_t>(d), tk);
            }
        } else if (tk == TypeKind::Double) {
            bits = std::bit_cast<std::int64_t>(fk == TypeKind::ULong
                                                   ? static_cast<double>(static_cast<std::uint64_t>(bits))
                                                   : static_cast<double>(bits));
        } else {
            bits = fold_int(bits, tk);
        }
        expr->type = to;
        expr->node = BConst{bits};
        return expr;
    }
    return runtime_arithmetic(std::move(expr), to);
}

BExprPtr ImplicitConversions::runtime_arithmetic(BExprPtr expr, TypeRef to) const {
    const TypeKind fk = types_.kind(expr->type);
    const TypeKind tk = types_.kind(to);
    if (tk == TypeKind::Bool) {
        return wrap(std::move(expr), fk == TypeKind::Double ? Conv::DoubleToBool : Conv::IntToBool, to);
    }
    if (tk == TypeKind::Double) {
        return wrap(std::move(expr), fk == TypeKind::ULong ? Conv::ULongToDouble : Conv::IntToDouble, to);
    }
    if (fk == TypeKind::Double) {
        switch (tk) {
            case TypeKind::Long: return wrap(std::move(expr), Conv::DoubleToInt64, to);
            case TypeKind::ULong: return wrap(std::move(expr), Conv::DoubleToUInt64, to);
            case TypeKind::UInt: return wrap(std::move(expr), Conv::DoubleToUInt32, to);
            case TypeKind::Int:
            case TypeKind::Enum: return wrap(std::move(expr), Conv::DoubleToInt32, to);
            default: {  // char and short: through int, then wrap
                auto as_int = wrap(std::move(expr), Conv::DoubleToInt32, TypeTable::kInt);
                return wrap(std::move(as_int), narrowing(tk), to);
            }
        }
    }
    // Integer to integer: widening keeps the value; anything else wraps.
    if (widens(fk, tk)) {
        return wrap(std::move(expr), Conv::Retype, to);
    }
    return wrap(std::move(expr), narrowing(tk), to);
}

BExprPtr ImplicitConversions::convert(BExprPtr expr, TypeRef to) const {
    const TypeRef from = expr->type;
    if (from == to || types_.is_error(from) || types_.is_error(to)) {
        expr->type = types_.is_error(from) ? from : to;
        return expr;
    }
    const TypeKind fk = types_.kind(from);
    const TypeKind tk = types_.kind(to);

    if (tk == TypeKind::Bool && fk == TypeKind::Pointer) {
        return wrap(std::move(expr), Conv::PtrToBool, to);
    }
    if ((is_int_like(fk) || fk == TypeKind::Double) && (is_int_like(tk) || tk == TypeKind::Double)) {
        return arithmetic(std::move(expr), to);
    }
    if (tk == TypeKind::Pointer) {
        if (is_null_pointer_constant(*expr, types_)) {
            auto null = std::make_unique<BExpr>();
            null->type = to;
            null->range = expr->range;
            null->node = BConst{0};
            return null;
        }
        const TypeInfo& fi = types_.info(from);
        const TypeInfo& ti = types_.info(to);
        if (fi.target == ti.target) {
            expr->type = to;
            return expr;
        }
        if (types_.is_record(fi.target) && types_.is_record(ti.target)) {
            auto path = hierarchy_.find_base(types_.info(fi.target).decl, types_.info(ti.target).decl);
            auto out = wrap(std::move(expr), Conv::PtrOffset, to);
            auto& c = std::get<BConvert>(out->node);
            if (path && path->via_virtual) {
                c.virtual_base = path->virtual_base;
                c.offset = path->offset_in_virtual;
            } else {
                c.offset = path ? path->offset : 0;
            }
            return out;
        }
    }
    expr->type = to;
    return expr;
}

std::vector<Conv> ImplicitConversions::steps(TypeRef from, TypeRef to) const {
    std::vector<Conv> out;
    BExpr probe;
    probe.type = from;
    probe.node = BLoad{};  // not a constant: no folding
    auto converted = arithmetic(std::make_unique<BExpr>(std::move(probe)), to);
    const BExpr* e = converted.get();
    while (const auto* c = std::get_if<BConvert>(&e->node)) {
        if (c->conv != Conv::Retype) {
            out.insert(out.begin(), c->conv);
        }
        e = c->operand.get();
    }
    return out;
}

bool ImplicitConversions::can_cast(const BExpr& from, TypeRef to) const {
    if (rank(from, to) != ConversionRank::None) {
        return true;
    }
    const TypeKind fk = types_.kind(from.type);
    const TypeKind tk = types_.kind(to);
    const bool from_number = is_int_like(fk) || fk == TypeKind::Double;
    const bool to_number = is_int_like(tk) || tk == TypeKind::Double;
    if (from_number && to_number) {
        return true;  // includes int <-> enum and enum class <-> int
    }
    if (fk == TypeKind::Pointer && tk == TypeKind::Pointer) {
        const TypeRef ft = types_.info(from.type).target;
        const TypeRef tt = types_.info(to).target;
        if (types_.is_record(ft) && types_.is_record(tt)) {
            return hierarchy_.find_base(types_.info(tt).decl, types_.info(ft).decl).has_value();  // downcast
        }
    }
    return tk == TypeKind::Void;
}

BExprPtr ImplicitConversions::cast(BExprPtr expr, TypeRef to) const {
    if (types_.kind(to) == TypeKind::Void) {
        expr->type = to;
        return expr;
    }
    if (rank(*expr, to) != ConversionRank::None) {
        return convert(std::move(expr), to);
    }
    const TypeKind fk = types_.kind(expr->type);
    const TypeKind tk = types_.kind(to);
    if ((is_int_like(fk) || fk == TypeKind::Double) && (is_int_like(tk) || tk == TypeKind::Double)) {
        return arithmetic(std::move(expr), to);
    }
    if (fk == TypeKind::Pointer && tk == TypeKind::Pointer) {  // static downcast
        const TypeRef ft = types_.info(expr->type).target;
        const TypeRef tt = types_.info(to).target;
        auto path = hierarchy_.find_base(types_.info(tt).decl, types_.info(ft).decl);
        auto out = wrap(std::move(expr), Conv::PtrOffset, to);
        std::get<BConvert>(out->node).offset = path ? -static_cast<std::int64_t>(path->offset) : 0;
        return out;
    }
    expr->type = to;
    return expr;
}

}  // namespace cppi::sema
