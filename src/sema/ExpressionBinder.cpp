#include "sema/ExpressionBinder.hpp"

#include "sema/CaptureAnalysis.hpp"
#include "sema/DeclarationBinder.hpp"
#include "sema/InitializerBinder.hpp"
#include "sema/NameSuggester.hpp"
#include "sema/OverloadResolver.hpp"
#include "sema/StatementBinder.hpp"
#include "sema/TemplateEngine.hpp"
#include "sema/TypeResolver.hpp"
#include "support/CheckedArithmetic.hpp"
#include "support/DiagnosticFactory.hpp"
#include "support/Overloaded.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <set>
#include <utility>
#include <variant>

namespace cppi::sema {

using detail::DiagnosticFactory;

namespace {

std::string_view op_text(ast::BinaryOp op) {
    using ast::BinaryOp;
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::Shl: return "<<";
        case BinaryOp::Shr: return ">>";
        case BinaryOp::BitAnd: return "&";
        case BinaryOp::BitOr: return "|";
        case BinaryOp::BitXor: return "^";
        case BinaryOp::LogicalAnd: return "&&";
        case BinaryOp::LogicalOr: return "||";
        case BinaryOp::Eq: return "==";
        case BinaryOp::Ne: return "!=";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Gt: return ">";
        case BinaryOp::Le: return "<=";
        case BinaryOp::Ge: return ">=";
        case BinaryOp::Comma: return ",";
    }
    return "?";
}

std::string_view unary_text(ast::UnaryOp op) {
    switch (op) {
        case ast::UnaryOp::Plus: return "+";
        case ast::UnaryOp::Minus: return "-";
        case ast::UnaryOp::Not: return "!";
        case ast::UnaryOp::BitNot: return "~";
        case ast::UnaryOp::AddressOf: return "&";
        case ast::UnaryOp::Deref: return "*";
    }
    return "?";
}

BinOp to_binop(ast::BinaryOp op) {
    using ast::BinaryOp;
    switch (op) {
        case BinaryOp::Add: return BinOp::Add;
        case BinaryOp::Sub: return BinOp::Sub;
        case BinaryOp::Mul: return BinOp::Mul;
        case BinaryOp::Div: return BinOp::Div;
        case BinaryOp::Mod: return BinOp::Mod;
        case BinaryOp::Shl: return BinOp::Shl;
        case BinaryOp::Shr: return BinOp::Shr;
        case BinaryOp::BitAnd: return BinOp::BitAnd;
        case BinaryOp::BitOr: return BinOp::BitOr;
        case BinaryOp::BitXor: return BinOp::BitXor;
        case BinaryOp::Eq: return BinOp::Eq;
        case BinaryOp::Ne: return BinOp::Ne;
        case BinaryOp::Lt: return BinOp::Lt;
        case BinaryOp::Gt: return BinOp::Gt;
        case BinaryOp::Le: return BinOp::Le;
        case BinaryOp::Ge: return BinOp::Ge;
        default: return BinOp::Add;
    }
}

bool is_comparison(ast::BinaryOp op) {
    using ast::BinaryOp;
    return op == BinaryOp::Eq || op == BinaryOp::Ne || op == BinaryOp::Lt || op == BinaryOp::Gt || op == BinaryOp::Le ||
           op == BinaryOp::Ge;
}

}  // namespace

// =============================================================================
// Entry points
// =============================================================================

BExprPtr ExpressionBinder::bind(const ast::Expr& expr, Use use) {
    return bind_node(expr, use);
}

BExprPtr ExpressionBinder::bind_rvalue(const ast::Expr& expr) {
    auto e = bind(expr);
    return e ? to_rvalue(std::move(e)) : nullptr;
}

BExprPtr ExpressionBinder::bind_converted(const ast::Expr& expr, TypeRef to) {
    auto e = bind_rvalue(expr);
    return e ? convert_to(std::move(e), to) : nullptr;
}

BExprPtr ExpressionBinder::bind_condition(const ast::Expr& expr) {
    if (std::holds_alternative<ast::AssignExpr>(expr.node) && !std::get<ast::AssignExpr>(expr.node).op.has_value()) {
        ctx_.report(DiagnosticFactory::assignment_in_condition(expr.range));
    }
    auto e = bind_rvalue(expr);
    return e ? to_condition(std::move(e)) : nullptr;
}

BExprPtr ExpressionBinder::to_rvalue(BExprPtr expr) {
    if (!expr || !expr->lvalue) {
        return expr;
    }
    const TypeTable& types = ctx_.types;
    if (types.is_array(expr->type)) {
        const TypeRef element = types.info(expr->type).target;
        const TypeRef pointer = ctx_.types.pointer_to(element, expr->is_const);
        const SourceRange range = expr->range;
        BConvert decay;
        decay.conv = Conv::ArrayDecay;
        decay.cells = ctx_.types.cells(element);
        decay.operand = std::move(expr);
        return make(pointer, false, range, std::move(decay));
    }
    if (types.is_record(expr->type)) {
        return expr;  // records are handled through their address
    }
    const TypeRef type = expr->type;
    const SourceRange range = expr->range;
    return make(type, false, range, BLoad{std::move(expr)});
}

BExprPtr ExpressionBinder::convert_to(BExprPtr rvalue, TypeRef to) {
    if (!rvalue) {
        return nullptr;
    }
    if (rvalue->type == to) {
        return rvalue;
    }
    if (ctx_.types.is_record(rvalue->type) && !ctx_.types.is_record(to)) {
        if (auto op = ctx_.overloads->conversion_operator(rvalue->type, to, false)) {
            return convert_to(to_rvalue(call_conversion(std::move(rvalue), *op)), to);
        }
    }
    if (ctx_.types.is_record(rvalue->type) || ctx_.types.is_record(to) ||
        ctx_.conversions.rank(*rvalue, to) == ConversionRank::None) {
        ctx_.report(
            DiagnosticFactory::cannot_convert(rvalue->range, ctx_.types.name(rvalue->type), ctx_.types.name(to)));
        return nullptr;
    }
    return ctx_.conversions.convert(std::move(rvalue), to);
}

BExprPtr ExpressionBinder::to_number(BExprPtr expr) {
    if (!expr || !ctx_.types.is_record(expr->type)) {
        return expr;
    }
    for (const TypeRef target : {TypeTable::kDouble, TypeTable::kLong, TypeTable::kInt, TypeTable::kBool}) {
        if (auto op = ctx_.overloads->conversion_operator(expr->type, target, false)) {
            return call_conversion(std::move(expr), *op);
        }
    }
    return expr;
}

BExprPtr ExpressionBinder::call_conversion(BExprPtr object, std::uint32_t function) {
    const SourceRange range = object->range;
    return call_method(std::move(object), ctx_.functions[function].name, {}, range);
}

BExprPtr ExpressionBinder::to_condition(BExprPtr expr) {
    if (!expr) {
        return nullptr;
    }
    const TypeTable& types = ctx_.types;
    if (types.is_record(expr->type)) {
        // `if (ptr)` on a class: its (possibly explicit) operator bool.
        if (auto op = ctx_.overloads->conversion_operator(expr->type, TypeTable::kBool, true)) {
            auto converted = to_rvalue(call_conversion(std::move(expr), *op));
            return converted ? to_condition(std::move(converted)) : nullptr;
        }
    }
    if (types.is_arithmetic(expr->type) || types.is_pointer(expr->type)) {
        return ctx_.conversions.convert(std::move(expr), TypeTable::kBool);
    }
    ctx_.report(DiagnosticFactory::cannot_convert(expr->range, types.name(expr->type), "bool"));
    return nullptr;
}

BExprPtr ExpressionBinder::as_argument(BExprPtr arg, TypeRef param) {
    TypeTable& types = ctx_.types;
    if (types.is_rvalue_reference(param)) {
        const TypeInfo ref = types.info(param);
        if (arg->lvalue && arg->type != ref.target && types.is_record(arg->type) && types.is_record(ref.target)) {
            arg = base_subobject(std::move(arg), types.info(ref.target).decl);
        }
        if (!arg->lvalue || arg->type != ref.target) {
            if (types.is_record(ref.target)) {
                const SourceRange at = arg->range;
                std::vector<BExprPtr> one;
                one.push_back(std::move(arg));
                arg = construct_temporary(ref.target, std::move(one), at);
            } else {
                auto value = convert_to(to_rvalue(std::move(arg)), ref.target);
                arg = value ? materialize(std::move(value)) : nullptr;
            }
            if (!arg) {
                return nullptr;
            }
            arg->is_const = false;
        }
        const SourceRange range = arg->range;
        return make(types.pointer_to(ref.target), false, range, BAddressOf{std::move(arg)});
    }
    if (types.is_reference(param)) {
        const TypeInfo ref = types.info(param);
        const bool same = arg->type == ref.target;
        if (arg->lvalue && types.is_record(arg->type) && !same) {
            arg = base_subobject(std::move(arg), types.info(ref.target).decl);
        }
        if (arg->lvalue && arg->type == ref.target && (ref.target_const || !arg->is_const)) {
            const SourceRange range = arg->range;
            return make(types.pointer_to(ref.target, ref.target_const), false, range, BAddressOf{std::move(arg)});
        }
        if (types.is_record(ref.target)) {
            // A temporary built by a converting constructor (or a record temporary).
            const SourceRange range = arg->range;
            BExprPtr temp = arg->type == ref.target ? std::move(arg) : [&] {
                std::vector<BExprPtr> one;
                one.push_back(std::move(arg));
                return construct_temporary(ref.target, std::move(one), range);
            }();
            if (!temp) {
                return nullptr;
            }
            return make(types.pointer_to(ref.target, true), false, range, BAddressOf{std::move(temp)});
        }
        // const T& bound to a value: the value lives in a temporary.
        auto value = convert_to(to_rvalue(std::move(arg)), ref.target);
        if (!value) {
            return nullptr;
        }
        auto temp = materialize(std::move(value));
        const SourceRange range = temp->range;
        return make(types.pointer_to(ref.target, true), false, range, BAddressOf{std::move(temp)});
    }
    if (types.is_record(param)) {
        // Pass by value: the callee works on a copy made here. A temporary
        // of the right type already is such a copy.
        const bool is_temporary =
            arg->type == param &&
            (std::holds_alternative<BTemp>(arg->node) ||
             (std::holds_alternative<BCall>(arg->node) && std::get<BCall>(arg->node).result_temp.has_value()));
        if (!is_temporary) {
            const SourceRange range = arg->range;
            std::vector<BExprPtr> one;
            one.push_back(std::move(arg));
            arg = construct_temporary(param, std::move(one), range);
            if (!arg) {
                return nullptr;
            }
        }
        const SourceRange range = arg->range;
        return make(types.pointer_to(param), false, range, BAddressOf{std::move(arg)});
    }
    return convert_to(to_rvalue(std::move(arg)), param);
}

BExprPtr ExpressionBinder::materialize(BExprPtr value) {
    const TypeRef type = value->type;
    const SourceRange range = value->range;
    BTemp temp;
    temp.offset = ctx_.allocate_local(1);
    temp.cells = 1;
    temp.init = std::move(value);
    auto e = make(type, true, range, std::move(temp));
    e->is_const = true;
    e->expiring = true;
    return e;
}

Symbol* ExpressionBinder::static_member(std::uint32_t record, std::string_view name) const {
    const RecordInfo& info = ctx_.types.record_at(record);
    if (info.statics != nullptr) {
        if (Symbol* s = info.statics->find(name); s != nullptr && s->kind == SymbolKind::Variable) {
            return s;
        }
    }
    for (const BaseInfo& base : info.bases) {
        if (Symbol* s = static_member(base.record, name)) {
            return s;
        }
    }
    return nullptr;
}

BExprPtr ExpressionBinder::variable(const Symbol& symbol, const std::string& name, SourceRange range) const {
    if (symbol.member_of) {  // a static data member: its class decides who may use it
        const auto access = static_cast<ast::Access>(symbol.member_access);
        if (!members_.accessible(access, *symbol.member_of, ctx_.fn != nullptr ? ctx_.fn->record : std::nullopt)) {
            ctx_.report(
                DiagnosticFactory::inaccessible_member(range, name, ctx_.types.record_at(*symbol.member_of).name,
                                                       access == ast::Access::Private ? "private" : "protected"));
            return nullptr;
        }
    }
    BVar v;
    v.offset = symbol.offset;
    v.global = symbol.global;
    v.reference = symbol.reference;
    v.cells = ctx_.types.cells(symbol.type);
    v.constant = symbol.constant;
    v.name = name;
    auto e = make(symbol.type, true, range, std::move(v));
    e->is_const = symbol.is_const;
    return e;
}

BExprPtr ExpressionBinder::this_pointer(SourceRange range) {
    if (ctx_.fn == nullptr || !ctx_.fn->record || !ctx_.fn->has_this) {
        ctx_.report(DiagnosticFactory::declaration_not_allowed(range, "this"));
        return nullptr;
    }
    const TypeRef record = ctx_.types.record_type(*ctx_.fn->record);
    BVar self;
    self.offset = 0;
    self.cells = 1;
    const TypeRef pointer = ctx_.types.pointer_to(record, ctx_.fn->const_this);
    auto var = make(pointer, true, range, std::move(self));
    return make(pointer, false, range, BLoad{std::move(var)});
}

BExprPtr ExpressionBinder::this_object(SourceRange range) {
    auto self = this_pointer(range);
    if (!self) {
        return nullptr;
    }
    const TypeInfo& info = ctx_.types.info(self->type);
    BDeref deref;
    deref.cells = ctx_.types.cells(info.target);
    deref.pointer = std::move(self);
    auto e = make(info.target, true, range, std::move(deref));
    e->is_const = info.target_const;
    return e;
}

BExprPtr ExpressionBinder::base_subobject(BExprPtr object, std::uint32_t base_record) {
    TypeTable& types = ctx_.types;
    if (!types.is_record(object->type) || types.info(object->type).decl == base_record) {
        return object;
    }
    auto path = ctx_.hierarchy.find_base(types.info(object->type).decl, base_record);
    if (!path) {
        return object;
    }
    const TypeRef base_type = types.record_type(base_record);
    const SourceRange range = object->range;
    const bool is_const = object->is_const;
    if (path->via_virtual) {
        const TypeRef pointer = types.pointer_to(object->type, is_const);
        auto address = make(pointer, false, range, BAddressOf{std::move(object)});
        auto adjusted = ctx_.conversions.convert(std::move(address), types.pointer_to(base_type, is_const));
        BDeref deref;
        deref.cells = types.cells(base_type);
        deref.pointer = std::move(adjusted);
        auto e = make(base_type, true, range, std::move(deref));
        e->is_const = is_const;
        return e;
    }
    BMember m;
    m.base = std::move(object);
    m.offset = path->offset;
    m.cells = types.record_at(base_record).nv_size;
    m.is_base = true;
    auto e = make(base_type, true, range, std::move(m));
    e->is_const = is_const;
    return e;
}

BExprPtr ExpressionBinder::make_call(const Callee& callee, std::vector<BExprPtr> args, SourceRange range,
                                     std::optional<std::uint32_t> virtual_slot) {
    TypeTable& types = ctx_.types;
    if (callee.intrinsic) {
        const TypeRef result = ctx_.overloads->return_type(callee);
        return make(result, false, range, BIntrinsic{static_cast<std::uint8_t>(callee.id), std::move(args)});
    }
    if (callee.host) {
        const TypeRef result = ctx_.overloads->return_type(callee);
        return make(result, false, range, BHostCall{FunctionId{callee.id}, std::move(args)});
    }
    const FunctionInfo& info = ctx_.functions[callee.id];
    ctx_.called.insert(callee.id);
    BCall call;
    call.function = callee.id;
    call.args = std::move(args);
    call.virtual_slot = virtual_slot;
    const TypeRef ret = info.return_type;
    if (types.is_reference(ret)) {
        const TypeInfo ref = types.info(ret);
        auto pointer = make(types.pointer_to(ref.target, ref.target_const), false, range, std::move(call));
        BDeref deref;
        deref.cells = types.cells(ref.target);
        deref.pointer = std::move(pointer);
        auto e = make(ref.target, true, range, std::move(deref));
        e->is_const = ref.target_const;
        e->expiring = ref.count == 1;  // a function returning T&& yields an expiring object
        return e;
    }
    if (types.is_record(ret)) {
        call.result_temp = ctx_.allocate_local(types.cells(ret));
        if (ctx_.initializers->needs_destruction(ret)) {
            ctx_.fn->temporaries.push_back(FunctionContext::Temporary{*call.result_temp, ret});
        }
        auto e = make(ret, true, range, std::move(call));
        e->expiring = true;
        return e;
    }
    return make(ret, false, range, std::move(call));
}

// =============================================================================
// Dispatch
// =============================================================================

BExprPtr ExpressionBinder::bind_node(const ast::Expr& e, Use use) {
    const SourceRange range = e.range;
    return std::visit(
        detail::Overloaded{
            [&](const ast::IntLiteral& lit) { return int_literal(lit, range); },
            [&](const ast::FloatLiteral& lit) {
                return constant(TypeTable::kDouble, std::bit_cast<std::int64_t>(lit.value), range);
            },
            [&](const ast::BoolLiteral& lit) { return constant(TypeTable::kBool, lit.value ? 1 : 0, range); },
            [&](const ast::CharLiteral& lit) { return constant(TypeTable::kChar, lit.value, range); },
            [&](const ast::StringLiteral&) -> BExprPtr {
                if (ctx_.templates != nullptr) {
                    return ctx_.templates->string_literal(std::get<ast::StringLiteral>(e.node), range);
                }
                ctx_.report(DiagnosticFactory::feature_not_implemented(range, Feature::Strings));
                return nullptr;
            },
            [&](const ast::NullptrLiteral&) { return constant(TypeTable::kNullptr, 0, range); },
            [&](const ast::Identifier& id) { return identifier(id, range, use); },
            [&](const ast::CallExpr& c) { return call(c, range); },
            [&](const ast::UnaryExpr& u) { return unary(u, range); },
            [&](const ast::BinaryExpr& b) { return binary(b, range); },
            [&](const ast::AssignExpr& a) { return assign(a, range); },
            [&](const ast::IncDecExpr& i) { return inc_dec(i, range); },
            [&](const ast::ConditionalExpr& c) { return conditional(c, range); },
            [&](const ast::SubscriptExpr& s) { return subscript(s, range); },
            [&](const ast::MemberExpr& m) { return member(m, range); },
            [&](const ast::CastExpr& c) { return cast(c, range); },
            [&](const ast::SizeofExpr& s) { return size_of(s, range); },
            [&](const ast::InitList&) -> BExprPtr {
                ctx_.report(DiagnosticFactory::unsupported_syntax(range, "initializer_list"));
                return nullptr;
            },
            [&](const ast::ThisExpr&) { return this_pointer(range); },
            [&](const ast::NewExpr& n) { return new_expr(n, range); },
            [&](const ast::DeleteExpr& d) { return delete_expr(d, range); },
            [&](const ast::LambdaExpr& l) { return lambda(l, range); },
            [&](const ast::RequiresExpr& r) { return requires_expr(r, range); },
            [&](const ast::Unsupported& u) -> BExprPtr {
                ctx_.features.explain(u, range);
                return nullptr;
            },
        },
        e.node);
}

// =============================================================================
// Literals and names
// =============================================================================

BExprPtr ExpressionBinder::int_literal(const ast::IntLiteral& lit, SourceRange range) {
    if (lit.out_of_range) {
        ctx_.report(DiagnosticFactory::integer_out_of_range(range, lit.text));
        return nullptr;
    }
    // The first type that can hold the value, as [lex.icon] lists them.
    const auto value = static_cast<std::uint64_t>(lit.value);
    const bool fits_int = value <= 0x7FFFFFFFU;
    const bool fits_uint = value <= 0xFFFFFFFFU;
    const bool fits_long = value <= 0x7FFFFFFFFFFFFFFFU;
    const bool may_be_unsigned = lit.is_unsigned || !lit.is_decimal;
    TypeRef type = TypeTable::kError;
    if (lit.is_unsigned) {
        type = (fits_uint && !lit.is_long) ? TypeTable::kUInt : TypeTable::kULong;
    } else if (!lit.is_long && fits_int) {
        type = TypeTable::kInt;
    } else if (!lit.is_long && fits_uint && may_be_unsigned) {
        type = TypeTable::kUInt;
    } else if (fits_long) {
        type = TypeTable::kLong;
    } else if (may_be_unsigned) {
        type = TypeTable::kULong;
    } else {
        ctx_.report(DiagnosticFactory::integer_out_of_range(range, lit.text));
        return nullptr;
    }
    return constant(type, lit.value, range);
}

std::vector<std::string> ExpressionBinder::value_names() const {
    auto names = ctx_.symbols.visible_names(
        [](const Symbol& s) { return s.kind == SymbolKind::Variable || s.kind == SymbolKind::Constant; });
    if (ctx_.fn != nullptr && ctx_.fn->record) {
        auto more = members_.names(*ctx_.fn->record);
        names.insert(names.end(), more.begin(), more.end());
    }
    return names;
}

BExprPtr ExpressionBinder::identifier(const ast::Identifier& id, SourceRange range, Use use) {
    TypeTable& types = ctx_.types;
    if (!id.scope.empty()) {
        if (Symbol* qualified = ctx_.symbols.lookup_qualified(id.scope, id.name)) {
            return from_symbol(*qualified, id.name, range, use);
        }
        // `Color::Red`: an enumerator of a (scoped or unscoped) enum.
        Symbol* scope = id.scope.size() == 1 ? ctx_.symbols.lookup(id.scope.front()) : nullptr;
        if (scope != nullptr && scope->kind == SymbolKind::Type && types.is_enum(scope->type)) {
            for (const auto& [name, value] : types.enum_info(scope->type).enumerators) {
                if (name == id.name) {
                    return constant(scope->type, value, range);
                }
            }
            std::vector<std::string> names;
            for (const auto& [name, value] : types.enum_info(scope->type).enumerators) {
                names.push_back(name);
            }
            ctx_.report(DiagnosticFactory::no_member(range, types.name(scope->type), id.name,
                                                     NameSuggester::closest(id.name, names)));
            return nullptr;
        }
        if (scope != nullptr && scope->kind == SymbolKind::Type && types.is_record(scope->type)) {
            auto self = this_object(range);
            if (!self) {
                return nullptr;
            }
            auto base = base_subobject(std::move(self), types.info(scope->type).decl);
            return member_of(std::move(base), id.name, range, range, false);
        }
        ctx_.report(DiagnosticFactory::unknown_identifier(range, id.scope.front() + "::" + id.name, std::nullopt));
        return nullptr;
    }

    Symbol* symbol = ctx_.symbols.lookup_local(id.name);
    if (symbol == nullptr && ctx_.fn != nullptr && ctx_.fn->record) {
        if (Symbol* member = static_member(*ctx_.fn->record, id.name)) {
            return variable(*member, id.name, range);
        }
        const MemberResult m = members_.find(*ctx_.fn->record, id.name);
        if (m.kind != MemberResult::Kind::None) {
            auto self = this_object(range);
            if (!self) {
                return nullptr;
            }
            if (m.kind == MemberResult::Kind::Methods) {
                const Severity severity = use == Use::Statement ? Severity::Warning : Severity::Error;
                ctx_.report(DiagnosticFactory::function_not_called(range, id.name, severity));
                return nullptr;
            }
            return member_of(std::move(self), id.name, range, range, false);
        }
    }
    if (symbol == nullptr) {
        symbol = ctx_.symbols.lookup(id.name);
    }
    if (symbol == nullptr) {
        auto suggestion = ctx_.symbols.qualified_name(
            id.name, [](const Symbol& s) { return s.kind == SymbolKind::Variable || s.kind == SymbolKind::Constant; });
        if (!suggestion) {
            suggestion = NameSuggester::closest(id.name, value_names());
        }
        ctx_.report(DiagnosticFactory::unknown_identifier(range, id.name, suggestion));
        return nullptr;
    }
    return from_symbol(*symbol, id.name, range, use);
}

BExprPtr ExpressionBinder::from_symbol(Symbol& found, const std::string& name, SourceRange range, Use use) {
    Symbol* symbol = &found;
    const ast::Identifier id{name, {}};
    switch (symbol->kind) {
        case SymbolKind::Variable: return variable(*symbol, id.name, range);
        case SymbolKind::Constant: return constant(symbol->type, symbol->value, range);
        case SymbolKind::Functions: {
            // `harvest;` is legal C++ that does nothing: warn, don't fail.
            const Severity severity = use == Use::Statement ? Severity::Warning : Severity::Error;
            ctx_.report(DiagnosticFactory::function_not_called(range, id.name, severity));
            return nullptr;
        }
        case SymbolKind::Type:
        case SymbolKind::Template:
        case SymbolKind::Namespace:
            ctx_.report(DiagnosticFactory::unsupported_syntax(range, "type name"));
            return nullptr;
        case SymbolKind::Poisoned: return nullptr;
    }
    return nullptr;
}

// =============================================================================
// Calls
// =============================================================================

BExprPtr ExpressionBinder::call(const ast::CallExpr& c, SourceRange range) {
    const TypeTable& types = ctx_.types;
    if (c.concept_id) {
        return ctx_.templates->concept_value(c, range);  // `Number<int>`
    }
    if (c.callee_expr) {
        if (const auto* m = std::get_if<ast::MemberExpr>(&c.callee_expr->node)) {
            return method_call(c, *m, range);
        }
        auto callee = bind(*c.callee_expr);
        if (!callee) {
            return nullptr;
        }
        if (types.is_record(callee->type) && is_function_object(types.info(callee->type).decl)) {
            std::vector<BExprPtr> args;
            for (const auto& a : c.args) {
                auto bound = bind(a);
                if (!bound) {
                    return nullptr;
                }
                args.push_back(std::move(bound));
            }
            return call_function_object(std::move(callee), std::move(args), range, c.callee_range);
        }
        ctx_.report(DiagnosticFactory::not_callable(c.callee_expr->range, types.name(callee->type)));
        return nullptr;
    }

    auto bind_args = [&]() -> std::optional<std::vector<BExprPtr>> {
        std::vector<BExprPtr> args;
        bool ok = true;
        for (const auto& a : c.args) {
            auto bound = bind(a);
            ok = ok && bound != nullptr;
            args.push_back(std::move(bound));
        }
        if (!ok) {
            return std::nullopt;
        }
        return args;
    };

    Symbol* qualified = c.scope.empty() ? nullptr : ctx_.symbols.lookup_qualified(c.scope, c.callee);
    // std::move(x): x as an expiring object (it may be moved from).
    const bool std_scope = c.scope.size() == 1 && c.scope.front() == "std";
    if (c.callee == "move" && c.args.size() == 1 && c.template_args.empty() &&
        ((std_scope && qualified == nullptr) || (c.scope.empty() && ctx_.symbols.lookup("move") == nullptr))) {
        auto object = bind(c.args.front());
        if (object && object->lvalue) {
            object->expiring = true;
            object->range = range;
        }
        return object;
    }
    if (!c.template_args.empty()) {
        auto args = bind_args();
        if (!args) {
            return nullptr;
        }
        return ctx_.templates->call(c, std::move(*args), range);
    }

    if (!c.scope.empty() && qualified == nullptr) {
        // `Base::method(...)`: a non-virtual call to a base class member.
        Symbol* scope = c.scope.size() == 1 ? ctx_.symbols.lookup(c.scope.front()) : nullptr;
        if (scope != nullptr && scope->kind == SymbolKind::Type && types.is_record(scope->type) && ctx_.fn != nullptr &&
            ctx_.fn->record) {
            const MemberResult m = members_.find(types.info(scope->type).decl, c.callee);
            if (m.kind == MemberResult::Kind::Methods) {
                auto args = bind_args();
                auto self = this_object(range);
                if (!args || !self) {
                    return nullptr;
                }
                auto base = base_subobject(std::move(self), types.info(scope->type).decl);
                return call_candidates(c.callee, {}, std::move(*args), range, c.callee_range, std::move(base), false);
            }
        }
        ctx_.report(
            DiagnosticFactory::unknown_function(c.callee_range, c.scope.front() + "::" + c.callee, std::nullopt));
        return nullptr;
    }

    Symbol* symbol = qualified != nullptr ? qualified : ctx_.symbols.lookup_local(c.callee);
    if (symbol == nullptr && ctx_.fn != nullptr && ctx_.fn->record &&
        ctx_.templates->has_member_template(*ctx_.fn->record, c.callee)) {
        auto args = bind_args();
        if (!args) {
            return nullptr;
        }
        auto instances = ctx_.templates->member_candidates(*ctx_.fn->record, c.callee, {}, *args, range);
        if (!ctx_.fn->has_this) {
            std::erase_if(instances, [&](const Callee& i) { return !ctx_.functions[i.id].is_static; });
            return call_candidates(c.callee, instances, std::move(*args), range, c.callee_range, nullptr, false);
        }
        auto self = this_object(range);
        if (!self) {
            return nullptr;
        }
        return call_candidates(c.callee, {}, std::move(*args), range, c.callee_range, std::move(self), true, instances);
    }
    if (symbol == nullptr && ctx_.fn != nullptr && ctx_.fn->record) {
        const MemberResult m = members_.find(*ctx_.fn->record, c.callee);
        if (m.kind == MemberResult::Kind::Methods && !ctx_.fn->has_this) {
            // In a static member function only static members can be called.
            std::vector<Callee> statics;
            for (const std::uint32_t id : *m.methods) {
                if (ctx_.functions[id].is_static) {
                    statics.push_back(Callee{false, id});
                }
            }
            if (statics.empty()) {
                ctx_.report(DiagnosticFactory::declaration_not_allowed(c.callee_range, "this"));
                return nullptr;
            }
            auto args = bind_args();
            if (!args) {
                return nullptr;
            }
            return call_candidates(c.callee, statics, std::move(*args), range, c.callee_range, nullptr, false);
        }
        if (m.kind == MemberResult::Kind::Methods) {
            auto args = bind_args();
            auto self = this_object(range);
            if (!args || !self) {
                return nullptr;
            }
            // Unqualified calls to virtual members dispatch dynamically.
            return call_candidates(c.callee, {}, std::move(*args), range, c.callee_range, std::move(self), true);
        }
        if (m.kind == MemberResult::Kind::Ambiguous) {
            ctx_.report(DiagnosticFactory::ambiguous_name(c.callee_range, c.callee));
            return nullptr;
        }
    }
    if (symbol == nullptr) {
        symbol = ctx_.symbols.lookup(c.callee);
    }
    if (symbol == nullptr) {
        auto names = ctx_.symbols.visible_names([](const Symbol& s) { return s.kind == SymbolKind::Functions; });
        if (ctx_.fn != nullptr && ctx_.fn->record) {
            for (const auto& [name, ids] : ctx_.types.record_at(*ctx_.fn->record).methods) {
                names.push_back(name);
            }
        }
        auto suggestion =
            ctx_.symbols.qualified_name(c.callee, [](const Symbol& s) { return s.kind == SymbolKind::Functions; });
        if (!suggestion) {
            suggestion = NameSuggester::closest(c.callee, names);
        }
        ctx_.report(DiagnosticFactory::unknown_function(c.callee_range, c.callee, suggestion));
        return nullptr;
    }
    switch (symbol->kind) {
        case SymbolKind::Functions: {
            auto args = bind_args();
            if (!args) {
                return nullptr;
            }
            return call_candidates(c.callee, symbol->functions, std::move(*args), range, c.callee_range, nullptr,
                                   false);
        }
        case SymbolKind::Type: {
            const TypeRef type = symbol->type;
            auto args = bind_args();
            if (!args) {
                return nullptr;
            }
            if (types.is_record(type)) {
                return construct_temporary(type, std::move(*args), range);
            }
            if (args->empty() && types.is_scalar(type)) {
                return constant(type, 0, range);  // `T()` value-initializes: zero
            }
            if (args->size() != 1) {
                ctx_.report(
                    DiagnosticFactory::cannot_convert(range, ctx_.overloads->describe(*args), types.name(type)));
                return nullptr;
            }
            auto value = to_rvalue(std::move(args->front()));
            if (!ctx_.conversions.can_cast(*value, type)) {
                ctx_.report(DiagnosticFactory::cannot_convert(range, types.name(value->type), types.name(type)));
                return nullptr;
            }
            return ctx_.conversions.cast(std::move(value), type);
        }
        case SymbolKind::Template: {
            auto args = bind_args();
            if (!args || ctx_.templates == nullptr) {
                return nullptr;
            }
            return ctx_.templates->call(c, std::move(*args), range);
        }
        case SymbolKind::Variable:
            if (types.is_record(symbol->type) && is_function_object(types.info(symbol->type).decl)) {
                // A function object: `f(x)` calls f.operator()(x).
                auto object = variable(*symbol, c.callee, c.callee_range);
                auto args = bind_args();
                if (!object || !args) {
                    return nullptr;
                }
                return call_function_object(std::move(object), std::move(*args), range, c.callee_range);
            }
            ctx_.report(DiagnosticFactory::not_callable(c.callee_range, c.callee));
            return nullptr;
        case SymbolKind::Constant:
        case SymbolKind::Namespace:
            ctx_.report(DiagnosticFactory::not_callable(c.callee_range, c.callee));
            return nullptr;
        case SymbolKind::Poisoned: return nullptr;
    }
    return nullptr;
}

BExprPtr ExpressionBinder::method_call(const ast::CallExpr& c, const ast::MemberExpr& callee, SourceRange range) {
    const TypeTable& types = ctx_.types;
    auto base = bind(*callee.base);
    if (!base) {
        return nullptr;
    }
    BExprPtr object;
    if (callee.arrow) {
        base = through_arrow(std::move(base), callee.base->range);
        if (!base) {
            return nullptr;
        }
        base = to_rvalue(std::move(base));
        if (!types.is_pointer(base->type) || !types.is_record(types.info(base->type).target)) {
            report_invalid_operands("->", *base, nullptr, callee.base->range);
            return nullptr;
        }
        const TypeInfo info = types.info(base->type);
        BDeref deref;
        deref.cells = types.cells(info.target);
        deref.pointer = std::move(base);
        object = make(info.target, true, callee.base->range, std::move(deref));
        object->is_const = info.target_const;
    } else {
        object = std::move(base);
    }
    if (!types.is_record(object->type)) {
        ctx_.report(
            DiagnosticFactory::no_member(callee.member_range, types.name(object->type), callee.member, std::nullopt));
        return nullptr;
    }
    const std::uint32_t record = types.info(object->type).decl;
    const MemberResult m = members_.find(record, callee.member);
    if (ctx_.templates->has_member_template(record, callee.member)) {
        std::vector<BExprPtr> args;
        for (const auto& a : c.args) {
            auto bound = bind(a);
            if (!bound) {
                return nullptr;
            }
            args.push_back(std::move(bound));
        }
        auto instances = ctx_.templates->member_candidates(record, callee.member, callee.template_args, args, range);
        return call_candidates(callee.member, {}, std::move(args), range, callee.member_range, std::move(object), true,
                               instances, !callee.template_args.empty());
    }
    if (!callee.template_args.empty()) {
        ctx_.report(DiagnosticFactory::template_deduction(callee.member_range, callee.member));
        return nullptr;
    }
    if (m.kind == MemberResult::Kind::None) {
        ctx_.report(DiagnosticFactory::no_member(
            callee.member_range, types.name(object->type), callee.member,
            NameSuggester::closest(callee.member, members_.names(types.info(object->type).decl))));
        return nullptr;
    }
    if (m.kind == MemberResult::Kind::Ambiguous) {
        ctx_.report(DiagnosticFactory::ambiguous_name(callee.member_range, callee.member));
        return nullptr;
    }
    if (m.kind == MemberResult::Kind::Field) {
        ctx_.report(DiagnosticFactory::not_callable(callee.member_range, callee.member));
        return nullptr;
    }
    std::vector<BExprPtr> args;
    for (const auto& a : c.args) {
        auto bound = bind(a);
        if (!bound) {
            return nullptr;
        }
        args.push_back(std::move(bound));
    }
    return call_candidates(callee.member, {}, std::move(args), range, callee.member_range, std::move(object), true);
}

BExprPtr ExpressionBinder::call_candidates(const std::string& name, const std::vector<Callee>& free_candidates,
                                           std::vector<BExprPtr> args, SourceRange range, SourceRange callee_range,
                                           BExprPtr object, bool dynamic, const std::vector<Callee>& member_templates,
                                           bool templates_only) {
    TypeTable& types = ctx_.types;
    std::vector<Callee> candidates = free_candidates;
    std::uint32_t owner = 0;  // record declaring the methods, when there is an object
    if (object) {
        MemberResult m = members_.find(types.info(object->type).decl, name);
        if (templates_only) {
            m = MemberResult{};
        }
        if (m.kind != MemberResult::Kind::Methods && member_templates.empty()) {
            ctx_.report(templates_only ? DiagnosticFactory::template_deduction(callee_range, name)
                                       : DiagnosticFactory::unknown_function(callee_range, name, std::nullopt));
            return nullptr;
        }
        owner = m.kind == MemberResult::Kind::Methods ? m.owner
                                                      : ctx_.functions[member_templates.front().id].record.value_or(0);
        // The object is an implicit argument: a const object can only use
        // const methods, and a non-const one prefers the non-const overload.
        std::vector<std::uint32_t> all_methods;
        if (m.kind == MemberResult::Kind::Methods) {
            all_methods = *m.methods;
        }
        for (const Callee& t : member_templates) {
            all_methods.push_back(t.id);
        }
        std::vector<std::uint32_t> methods;
        for (const std::uint32_t id : all_methods) {
            const FunctionInfo& fn = ctx_.functions[id];
            if (fn.is_constructor || fn.is_destructor || (object->is_const && !fn.is_const && !fn.is_static)) {
                continue;
            }
            methods.push_back(id);
        }
        for (const std::uint32_t id : methods) {
            const FunctionInfo& fn = ctx_.functions[id];
            const bool shadowed =
                !object->is_const && fn.is_const &&
                std::any_of(methods.begin(), methods.end(), [&](std::uint32_t other) {
                    const FunctionInfo& o = ctx_.functions[other];
                    return !o.is_const && std::ranges::equal(o.params, fn.params, [](const auto& x, const auto& y) {
                        return x.type == y.type;
                    });
                });
            if (!shadowed) {
                candidates.push_back(Callee{false, id});
            }
        }
        if (candidates.empty() && !all_methods.empty() && object->is_const) {
            ctx_.report(DiagnosticFactory::not_assignable(object->range, std::nullopt));
            return nullptr;
        }
    }
    auto chosen = ctx_.overloads->resolve(name, candidates, args, range);
    if (!chosen) {
        return nullptr;
    }
    auto prepared = ctx_.overloads->prepare(*chosen, std::move(args), range);
    if (!prepared) {
        return nullptr;
    }
    if (!object) {
        if (!chosen->host && !chosen->intrinsic) {
            const FunctionInfo& fn = ctx_.functions[chosen->id];
            if (fn.is_static && fn.record &&
                !members_.accessible(fn.access, *fn.record, ctx_.fn != nullptr ? ctx_.fn->record : std::nullopt)) {
                ctx_.report(DiagnosticFactory::inaccessible_member(
                    callee_range, name, types.record_at(*fn.record).name,
                    fn.access == ast::Access::Private ? "private" : "protected"));
                return nullptr;
            }
        }
        return make_call(*chosen, std::move(*prepared), range);
    }

    const FunctionInfo& fn = ctx_.functions[chosen->id];
    if (fn.record) {
        owner = *fn.record;
    }
    if (!members_.accessible(fn.access, owner, ctx_.fn != nullptr ? ctx_.fn->record : std::nullopt)) {
        ctx_.report(
            DiagnosticFactory::inaccessible_member(callee_range, name, types.record_at(owner).name,
                                                   fn.access == ast::Access::Private ? "private" : "protected"));
        return nullptr;
    }
    if (fn.is_static) {
        return make_call(*chosen, std::move(*prepared), range);  // no `this`
    }
    if (object->is_const && !fn.is_const && !fn.is_static) {
        ctx_.report(DiagnosticFactory::not_assignable(object->range, std::nullopt));
        return nullptr;
    }
    // `this` points to the subobject of the class that declares the method.
    auto sub = base_subobject(std::move(object), owner);
    const bool is_const = sub->is_const;
    const TypeRef sub_type = sub->type;
    const SourceRange sub_range = sub->range;
    auto self = make(types.pointer_to(sub_type, is_const), false, sub_range, BAddressOf{std::move(sub)});
    std::vector<BExprPtr> all;
    all.push_back(std::move(self));
    for (auto& a : *prepared) {
        all.push_back(std::move(a));
    }
    std::optional<std::uint32_t> slot;
    if (fn.is_virtual && dynamic) {
        const auto& own = types.record_at(owner).virtual_functions;
        for (std::uint32_t i = 0; i < own.size(); ++i) {
            if (own[i] == chosen->id) {
                slot = i;
            }
        }
    }
    return make_call(*chosen, std::move(all), range, slot);
}

BExprPtr ExpressionBinder::heap_copy(BNew node, BExprPtr source, TypeRef type, bool is_const, SourceRange range) {
    // (p = new T, *p = source, p) with a frame cell for p.
    TypeTable& types = ctx_.types;
    const TypeRef pointer_type = types.pointer_to(type, is_const);
    BVar slot;
    slot.offset = ctx_.allocate_local(1);
    auto cell = [&]() { return make(types.pointer_to(type), true, range, slot); };
    BAssign store;
    store.kind = ScalarKind::Pointer;
    store.target = cell();
    store.value = make(types.pointer_to(type), false, range, std::move(node));
    auto allocate = make(types.pointer_to(type), true, range, std::move(store));
    BDeref deref;
    deref.cells = types.cells(type);
    deref.pointer = make(types.pointer_to(type), false, range, BLoad{cell()});
    BAssign copy;
    copy.target = make(type, true, range, std::move(deref));
    copy.value = std::move(source);
    copy.record_cells = types.cells(type);
    auto fill = make(type, true, range, std::move(copy));
    auto both = make(type, true, range, BComma{std::move(allocate), std::move(fill)});
    return make(pointer_type, false, range, BComma{std::move(both), make(pointer_type, false, range, BLoad{cell()})});
}

BExprPtr ExpressionBinder::requires_expr(const ast::RequiresExpr& r, SourceRange range) {
    // True if every requirement would compile; nothing inside is reported or run.
    if (!ctx_.features.allow(Feature::Concepts, range)) {
        return nullptr;
    }
    const TypeTable& types = ctx_.types;
    const std::size_t errors_before = ctx_.sink.dropped_errors();
    const std::size_t temps_mark = ctx_.fn->temporaries.size();
    ctx_.symbols.push(Scope::Kind::Block);
    ctx_.sink.mute();
    bool ok = true;
    for (const auto& param : r.params) {
        auto resolved = ctx_.type_resolver->resolve(param->type, param->declarator);
        if (!resolved || resolved->is_auto || types.is_void(resolved->type)) {
            ok = false;
            break;
        }
        const bool reference = types.is_reference(resolved->type);
        Symbol symbol;
        symbol.kind = SymbolKind::Variable;
        symbol.reference = reference;
        symbol.type = reference ? types.info(resolved->type).target : resolved->type;
        symbol.is_const = reference ? types.info(resolved->type).target_const : resolved->is_const;
        symbol.offset = ctx_.allocate_local(reference ? 1 : std::max<std::uint32_t>(1, types.cells(symbol.type)));
        if (!param->declarator.name.empty()) {
            ctx_.symbols.current().add(param->declarator.name, symbol);
        }
    }
    for (const ast::Requirement& req : r.requirements) {
        if (!ok) {
            break;
        }
        switch (req.kind) {
            case ast::Requirement::Kind::Simple: ok = req.expr && bind(*req.expr, Use::Statement) != nullptr; break;
            case ast::Requirement::Kind::Compound: {
                auto e = req.expr ? bind(*req.expr, Use::Statement) : nullptr;
                ok = e != nullptr;
                if (ok && req.constraint) {
                    ok = ctx_.templates->concept_holds(*req.constraint, e->type, req.range).value_or(false);
                }
                break;
            }
            case ast::Requirement::Kind::Type:
                ok = req.type && ctx_.type_resolver->resolve(*req.type).has_value();
                break;
        }
    }
    ctx_.sink.unmute();
    ctx_.symbols.pop();
    ctx_.statements->drop_temporaries(temps_mark);
    ok = ok && ctx_.sink.dropped_errors() == errors_before;
    return constant(TypeTable::kBool, ok ? 1 : 0, range);
}

bool ExpressionBinder::is_function_object(std::uint32_t record) const {
    return members_.find(record, "operator()").kind == MemberResult::Kind::Methods ||
           ctx_.templates->has_member_template(record, "operator()");
}

BExprPtr ExpressionBinder::call_function_object(BExprPtr object, std::vector<BExprPtr> args, SourceRange range,
                                                SourceRange callee_range) {
    const std::uint32_t record = ctx_.types.info(object->type).decl;
    std::vector<Callee> instances;
    if (ctx_.templates->has_member_template(record, "operator()")) {  // generic lambdas
        instances = ctx_.templates->member_candidates(record, "operator()", {}, args, range);
    }
    return call_candidates("operator()", {}, std::move(args), range, callee_range, std::move(object), true, instances);
}

BExprPtr ExpressionBinder::construct_temporary(TypeRef type, std::vector<BExprPtr> args, SourceRange range) {
    const TypeTable& types = ctx_.types;
    BTemp temp;
    temp.cells = types.cells(type);
    temp.offset = ctx_.allocate_local(temp.cells);
    BVar slot;
    slot.offset = temp.offset;
    slot.cells = temp.cells;
    auto target = make(type, true, range, slot);
    std::vector<BStmt> init;
    if (!ctx_.initializers->construct(std::move(target), type, std::move(args), range, init)) {
        return nullptr;
    }
    temp.init_stmts = std::move(init);
    if (ctx_.initializers->needs_destruction(type)) {
        ctx_.fn->temporaries.push_back(FunctionContext::Temporary{temp.offset, type});
    }
    auto e = make(type, true, range, std::move(temp));
    e->expiring = true;
    return e;
}

// =============================================================================
// Operators
// =============================================================================

std::vector<Callee> ExpressionBinder::free_operators(std::string_view name, const BExpr& lhs, const BExpr* rhs) {
    TypeTable& types = ctx_.types;
    std::vector<Callee> out;
    auto add = [&](const Symbol* s) {
        if (s == nullptr || s->kind != SymbolKind::Functions) {
            return;
        }
        for (const Callee& c : s->functions) {
            if (std::find(out.begin(), out.end(), c) == out.end()) {
                out.push_back(c);
            }
        }
    };
    add(ctx_.symbols.lookup(name));
    // Argument-dependent lookup: operators declared next to the operands' classes.
    for (const BExpr* operand : {&lhs, rhs}) {
        if (operand != nullptr && types.is_record(operand->type) && types.record(operand->type).home != nullptr) {
            add(types.record(operand->type).home->find(name));
        }
    }
    return out;
}

bool ExpressionBinder::has_operator(std::string_view name, const BExpr& lhs, const BExpr* rhs) {
    const TypeTable& types = ctx_.types;
    const bool lhs_record = types.is_record(lhs.type);
    const bool rhs_record = rhs != nullptr && types.is_record(rhs->type);
    if (!lhs_record && !rhs_record) {
        return false;
    }
    if (lhs_record && members_.find(types.info(lhs.type).decl, name).kind == MemberResult::Kind::Methods) {
        return true;
    }
    // Only an operator that takes these operands counts: std::string's
    // operator< is visible inside the library, but says nothing about a pair.
    const auto viable = [&](const Callee& c) {
        const std::vector<TypeRef> params = ctx_.overloads->param_types(c);
        // A unary operator may be a postfix `operator++(T&, int)`: its int is not an operand.
        const std::size_t count = rhs != nullptr ? 2 : 1;
        if (params.size() < count || params.size() - ctx_.overloads->defaults(c) > 2) {
            return false;
        }
        return ctx_.overloads->rank(lhs, params[0]) != ConversionRank::None &&
               (rhs == nullptr || ctx_.overloads->rank(*rhs, params[1]) != ConversionRank::None);
    };
    const std::vector<Callee> candidates = free_operators(name, lhs, rhs);
    return std::any_of(candidates.begin(), candidates.end(), viable);
}

BExprPtr ExpressionBinder::call_operator(const std::string& name, BExprPtr lhs, BExprPtr rhs, SourceRange range) {
    const TypeTable& types = ctx_.types;
    if (types.is_record(lhs->type) &&
        members_.find(types.info(lhs->type).decl, name).kind == MemberResult::Kind::Methods) {
        std::vector<BExprPtr> args;
        if (rhs) {
            args.push_back(std::move(rhs));
        }
        return call_candidates(name, {}, std::move(args), range, range, std::move(lhs), true);
    }
    const std::vector<Callee> candidates = free_operators(name, *lhs, rhs.get());
    std::vector<BExprPtr> args;
    args.push_back(std::move(lhs));
    if (rhs) {
        args.push_back(std::move(rhs));
    }
    return call_candidates(name, candidates, std::move(args), range, range, nullptr, false);
}

void ExpressionBinder::report_invalid_operands(std::string_view op, const BExpr& lhs, const BExpr* rhs,
                                               SourceRange range) {
    // A class without the operator: say so, rather than which operands are wrong.
    // Not for pointer arithmetic with a class (`ptr + p`), nor for unary & (taking
    // the address of something that is not an lvalue): the operator is not what is missing.
    const bool pointer = ctx_.types.is_pointer(lhs.type) || (rhs != nullptr && ctx_.types.is_pointer(rhs->type));
    const bool overloadable =
        op != "?:" && op != "->" && op != "delete" && op != "delete[]" && !pointer && !(op == "&" && rhs == nullptr);
    const BExpr* record = ctx_.types.is_record(lhs.type)                      ? &lhs
                          : rhs != nullptr && ctx_.types.is_record(rhs->type) ? rhs
                                                                              : nullptr;
    if (overloadable && record != nullptr) {
        ctx_.report(DiagnosticFactory::no_operator(range, op, ctx_.types.name(record->type)));
        if (ctx_.library_depth > 0 && !ctx_.library_entry.empty() && ctx_.library_site.begin.line != 0) {
            ctx_.report(DiagnosticFactory::instantiated_from(ctx_.library_site, ctx_.library_entry));
        }
        return;
    }
    ctx_.report(DiagnosticFactory::invalid_operands(
        range, op, ctx_.types.name(lhs.type),
        rhs != nullptr ? std::optional<std::string>(ctx_.types.name(rhs->type)) : std::nullopt));
}

bool ExpressionBinder::check_modifiable(const BExpr& target, const ast::Expr& source) {
    std::optional<std::string> name;
    if (const auto* id = std::get_if<ast::Identifier>(&source.node); id != nullptr && target.is_const) {
        name = id->name;
    }
    if (!target.lvalue || target.is_const || ctx_.types.is_array(target.type)) {
        ctx_.report(DiagnosticFactory::not_assignable(source.range, name));
        return false;
    }
    return true;
}

BExprPtr ExpressionBinder::unary(const ast::UnaryExpr& u, SourceRange range) {
    TypeTable& types = ctx_.types;
    if (u.op == ast::UnaryOp::AddressOf) {
        auto operand = bind(*u.operand);
        if (!operand) {
            return nullptr;
        }
        if (!operand->lvalue) {
            report_invalid_operands("&", *operand, nullptr, range);
            return nullptr;
        }
        const TypeRef pointer = types.pointer_to(operand->type, operand->is_const);
        return make(pointer, false, range, BAddressOf{std::move(operand)});
    }
    auto operand = bind(*u.operand);
    if (!operand) {
        return nullptr;
    }
    if (types.is_record(operand->type)) {
        const std::string name = "operator" + std::string(unary_text(u.op));
        if (has_operator(name, *operand, nullptr)) {
            return call_operator(name, std::move(operand), nullptr, range);
        }
        report_invalid_operands(unary_text(u.op), *operand, nullptr, range);
        return nullptr;
    }
    operand = to_rvalue(std::move(operand));
    if (u.op == ast::UnaryOp::Deref) {
        if (!types.is_pointer(operand->type) || types.is_void(types.info(operand->type).target)) {
            report_invalid_operands("*", *operand, nullptr, range);
            return nullptr;
        }
        const TypeInfo info = types.info(operand->type);
        BDeref deref;
        deref.cells = types.cells(info.target);
        deref.pointer = std::move(operand);
        auto e = make(info.target, true, range, std::move(deref));
        e->is_const = info.target_const;
        return e;
    }
    if (u.op == ast::UnaryOp::Not) {
        auto cond = to_condition(std::move(operand));
        if (!cond) {
            return nullptr;
        }
        if (auto* c = std::get_if<BConst>(&cond->node)) {
            return constant(TypeTable::kBool, c->bits == 0 ? 1 : 0, range);
        }
        return make(TypeTable::kBool, false, range, BUnary{UnOp::Not, ScalarKind::Int32, std::move(cond)});
    }
    const bool integral_only = u.op == ast::UnaryOp::BitNot;
    if (!(integral_only ? types.is_integral(operand->type) : types.is_arithmetic(operand->type))) {
        report_invalid_operands(unary_text(u.op), *operand, nullptr, range);
        return nullptr;
    }
    const TypeRef type = ctx_.conversions.promoted(operand->type);
    operand = ctx_.conversions.convert(std::move(operand), type);
    if (u.op == ast::UnaryOp::Plus) {
        operand->range = range;
        return operand;
    }
    const ScalarKind kind = ImplicitConversions::scalar_kind(types, type);
    if (auto* c = std::get_if<BConst>(&operand->node); c != nullptr && u.op == ast::UnaryOp::Minus) {
        if (kind == ScalarKind::Double) {
            return constant(type, std::bit_cast<std::int64_t>(-std::bit_cast<double>(c->bits)), range);
        }
        if (kind == ScalarKind::UInt32 || kind == ScalarKind::UInt64) {  // wraps modulo 2^bits
            const std::uint64_t wrapped = 0 - static_cast<std::uint64_t>(c->bits);
            return constant(
                type, static_cast<std::int64_t>(kind == ScalarKind::UInt32 ? wrapped & 0xFFFFFFFFU : wrapped), range);
        }
        const std::int64_t negated = c->bits == std::numeric_limits<std::int64_t>::min() ? c->bits : -c->bits;
        if (kind == ScalarKind::Int64 || detail::CheckedArithmetic::fits_int32(negated)) {
            if (c->bits != std::numeric_limits<std::int64_t>::min()) {
                return constant(type, negated, range);
            }
        }
    }
    const UnOp op = u.op == ast::UnaryOp::Minus ? UnOp::Neg : UnOp::BitNot;
    return make(type, false, range, BUnary{op, kind, std::move(operand)});
}

BExprPtr ExpressionBinder::binary(const ast::BinaryExpr& b, SourceRange range) {
    const TypeTable& types = ctx_.types;
    if (b.op == ast::BinaryOp::Comma) {
        auto lhs = bind(*b.lhs, Use::Statement);
        auto rhs = bind(*b.rhs);
        if (!lhs || !rhs) {
            return nullptr;
        }
        const TypeRef type = rhs->type;
        const bool lvalue = rhs->lvalue;
        const bool is_const = rhs->is_const;
        auto e = make(type, lvalue, range, BComma{std::move(lhs), std::move(rhs)});
        e->is_const = is_const;
        return e;
    }
    if (b.op == ast::BinaryOp::LogicalAnd || b.op == ast::BinaryOp::LogicalOr) {
        auto lhs = bind_condition(*b.lhs);
        auto rhs = bind_condition(*b.rhs);
        if (!lhs || !rhs) {
            return nullptr;
        }
        return make(TypeTable::kBool, false, range,
                    BLogical{b.op == ast::BinaryOp::LogicalAnd, std::move(lhs), std::move(rhs)});
    }
    auto lhs = bind(*b.lhs);
    auto rhs = bind(*b.rhs);
    if (!lhs || !rhs) {
        return nullptr;
    }
    const std::string name = "operator" + std::string(op_text(b.op));
    if ((types.is_record(lhs->type) || types.is_record(rhs->type)) && has_operator(name, *lhs, rhs.get())) {
        return call_operator(name, std::move(lhs), std::move(rhs), range);
    }
    // Built-in operators accept classes that convert to a number.
    lhs = to_number(std::move(lhs));
    rhs = to_number(std::move(rhs));
    if (!lhs || !rhs) {
        return nullptr;
    }
    return arithmetic(b.op, to_rvalue(std::move(lhs)), to_rvalue(std::move(rhs)), range);
}

BExprPtr ExpressionBinder::arithmetic(ast::BinaryOp op, BExprPtr lhs, BExprPtr rhs, SourceRange range) {
    const TypeTable& types = ctx_.types;
    const bool lp = types.is_pointer(lhs->type);
    const bool rp = types.is_pointer(rhs->type);

    // Pointer arithmetic.
    if ((op == ast::BinaryOp::Add || op == ast::BinaryOp::Sub) && (lp || rp)) {
        if (lp && rp && op == ast::BinaryOp::Sub && types.info(lhs->type).target == types.info(rhs->type).target) {
            const std::uint32_t elem = std::max<std::uint32_t>(1, types.cells(types.info(lhs->type).target));
            return make(TypeTable::kLong, false, range,
                        BBinary{BinOp::PtrDiff, ScalarKind::Pointer, std::move(lhs), std::move(rhs), elem});
        }
        if (rp && !lp && op == ast::BinaryOp::Add) {
            std::swap(lhs, rhs);
        }
        if (types.is_pointer(lhs->type) && types.is_integral(rhs->type)) {
            const TypeRef type = lhs->type;
            const std::uint32_t elem = std::max<std::uint32_t>(1, types.cells(types.info(type).target));
            rhs = ctx_.conversions.convert(std::move(rhs), TypeTable::kLong);
            return make(type, false, range,
                        BBinary{op == ast::BinaryOp::Add ? BinOp::PtrAdd : BinOp::PtrSub, ScalarKind::Pointer,
                                std::move(lhs), std::move(rhs), elem});
        }
        report_invalid_operands(op_text(op), *lhs, rhs.get(), range);
        return nullptr;
    }

    if (is_comparison(op)) {
        const bool lnull = ImplicitConversions::is_null_pointer_constant(*lhs, types);
        const bool rnull = ImplicitConversions::is_null_pointer_constant(*rhs, types);
        const bool pointers =
            (lp || types.kind(lhs->type) == TypeKind::Nullptr) && (rp || types.kind(rhs->type) == TypeKind::Nullptr);
        if ((lp && (rp || rnull)) || (rp && lnull) || pointers) {
            if (lp && rp && types.info(lhs->type).target != types.info(rhs->type).target &&
                ctx_.conversions.rank(*rhs, lhs->type) == ConversionRank::None &&
                ctx_.conversions.rank(*lhs, rhs->type) == ConversionRank::None) {
                report_invalid_operands(op_text(op), *lhs, rhs.get(), range);
                return nullptr;
            }
            if (lp && rp && lhs->type != rhs->type) {
                if (ctx_.conversions.rank(*rhs, lhs->type) != ConversionRank::None) {
                    rhs = ctx_.conversions.convert(std::move(rhs), lhs->type);
                } else {
                    lhs = ctx_.conversions.convert(std::move(lhs), rhs->type);
                }
            }
            return make(TypeTable::kBool, false, range,
                        BBinary{to_binop(op), ScalarKind::Pointer, std::move(lhs), std::move(rhs), 1});
        }
        if (types.is_scoped_enum(lhs->type) && lhs->type == rhs->type) {
            return make(TypeTable::kBool, false, range,
                        BBinary{to_binop(op), ScalarKind::Int32, std::move(lhs), std::move(rhs), 1});
        }
    }

    const bool integral_only = op == ast::BinaryOp::Mod || op == ast::BinaryOp::Shl || op == ast::BinaryOp::Shr ||
                               op == ast::BinaryOp::BitAnd || op == ast::BinaryOp::BitOr || op == ast::BinaryOp::BitXor;
    const bool ok = integral_only ? types.is_integral(lhs->type) && types.is_integral(rhs->type)
                                  : types.is_arithmetic(lhs->type) && types.is_arithmetic(rhs->type);
    if (!ok) {
        report_invalid_operands(op_text(op), *lhs, rhs.get(), range);
        return nullptr;
    }
    TypeRef common = 0;
    if (op == ast::BinaryOp::Shl || op == ast::BinaryOp::Shr) {
        common = ctx_.conversions.promoted(lhs->type);
        lhs = ctx_.conversions.convert(std::move(lhs), common);
        const TypeRef promoted_rhs = ctx_.conversions.promoted(rhs->type);
        rhs = ctx_.conversions.convert(std::move(rhs), promoted_rhs);
        if (types.kind(rhs->type) != types.kind(common)) {
            rhs = ctx_.conversions.convert(std::move(rhs), common);
        }
    } else {
        common = ctx_.conversions.common_arithmetic(lhs->type, rhs->type);
        lhs = ctx_.conversions.convert(std::move(lhs), common);
        rhs = ctx_.conversions.convert(std::move(rhs), common);
    }
    const ScalarKind kind = ImplicitConversions::scalar_kind(types, common);
    const TypeRef result = is_comparison(op) ? TypeTable::kBool : common;
    return make(result, false, range, BBinary{to_binop(op), kind, std::move(lhs), std::move(rhs), 1});
}

BExprPtr ExpressionBinder::assign(const ast::AssignExpr& a, SourceRange range) {
    const TypeTable& types = ctx_.types;
    auto target = bind(*a.lhs);
    if (!target) {
        return nullptr;
    }
    if (!check_modifiable(*target, *a.lhs)) {
        return nullptr;
    }
    const TypeRef type = target->type;

    if (types.is_record(type)) {
        const std::string name = "operator" + (a.op ? std::string(op_text(*a.op)) : std::string()) + "=";
        if (members_.find(types.info(type).decl, name).kind == MemberResult::Kind::Methods) {
            auto value = bind(*a.rhs);
            if (!value) {
                return nullptr;
            }
            return call_operator(name, std::move(target), std::move(value), range);
        }
        if (a.op) {
            auto value = bind(*a.rhs);
            if (value && has_operator(name, *target, value.get())) {
                return call_operator(name, std::move(target), std::move(value), range);
            }
            report_invalid_operands(name.substr(8), *target, value.get(), range);
            return nullptr;
        }
        auto value = bind(*a.rhs);
        if (!value) {
            return nullptr;
        }
        if (value->type != type) {
            if (!types.is_record(value->type) ||
                !ctx_.hierarchy.find_base(types.info(value->type).decl, types.info(type).decl)) {
                ctx_.report(DiagnosticFactory::cannot_convert(a.rhs->range, types.name(value->type), types.name(type)));
                return nullptr;
            }
            value = base_subobject(std::move(value), types.info(type).decl);
        }
        BAssign assign;
        assign.target = std::move(target);
        assign.value = std::move(value);
        assign.record_cells = types.cells(type);
        return make(type, true, range, std::move(assign));
    }

    auto value = bind_rvalue(*a.rhs);
    if (!value) {
        return nullptr;
    }
    BAssign assign;
    if (!a.op) {
        value = convert_to(std::move(value), type);
        if (!value) {
            return nullptr;
        }
        assign.kind = ImplicitConversions::scalar_kind(types, type);
        assign.target = std::move(target);
        assign.value = std::move(value);
        return make(type, true, range, std::move(assign));
    }

    const ast::BinaryOp op = *a.op;
    if (types.is_pointer(type) && (op == ast::BinaryOp::Add || op == ast::BinaryOp::Sub) &&
        types.is_integral(value->type)) {
        assign.op = op == ast::BinaryOp::Add ? BinOp::PtrAdd : BinOp::PtrSub;
        assign.kind = ScalarKind::Pointer;
        assign.elem_cells = std::max<std::uint32_t>(1, types.cells(types.info(type).target));
        assign.value = ctx_.conversions.convert(std::move(value), TypeTable::kLong);
        assign.target = std::move(target);
        return make(type, true, range, std::move(assign));
    }
    const bool integral_only = op == ast::BinaryOp::Mod || op == ast::BinaryOp::Shl || op == ast::BinaryOp::Shr ||
                               op == ast::BinaryOp::BitAnd || op == ast::BinaryOp::BitOr || op == ast::BinaryOp::BitXor;
    const bool ok = integral_only ? types.is_integral(type) && types.is_integral(value->type)
                                  : types.is_arithmetic(type) && types.is_arithmetic(value->type);
    if (!ok) {
        report_invalid_operands(std::string(op_text(op)) + "=", *target, value.get(), range);
        return nullptr;
    }
    const TypeRef common = (op == ast::BinaryOp::Shl || op == ast::BinaryOp::Shr)
                               ? ctx_.conversions.promoted(type)
                               : ctx_.conversions.common_arithmetic(type, value->type);
    // Conversions of the target's value into the computation type and back.
    assign.load_conv = ctx_.conversions.steps(type, common);
    assign.store_conv = ctx_.conversions.steps(common, type);
    assign.op = to_binop(op);
    assign.kind = ImplicitConversions::scalar_kind(types, common);
    assign.value = ctx_.conversions.convert(std::move(value), common);
    assign.target = std::move(target);
    return make(type, true, range, std::move(assign));
}

BExprPtr ExpressionBinder::inc_dec(const ast::IncDecExpr& e, SourceRange range) {
    const TypeTable& types = ctx_.types;
    auto target = bind(*e.operand);
    if (!target) {
        return nullptr;
    }
    if (types.is_record(target->type)) {
        const std::string name = e.increment ? "operator++" : "operator--";
        if (!has_operator(name, *target, nullptr)) {
            report_invalid_operands(e.increment ? "++" : "--", *target, nullptr, range);
            return nullptr;
        }
        // Postfix forms take a dummy int argument.
        BExprPtr dummy = e.prefix ? nullptr : constant(TypeTable::kInt, 0, range);
        return call_operator(name, std::move(target), std::move(dummy), range);
    }
    if (!check_modifiable(*target, *e.operand)) {
        return nullptr;
    }
    const TypeRef type = target->type;
    if (types.kind(type) == TypeKind::Bool || !(types.is_arithmetic(type) || types.is_pointer(type))) {
        report_invalid_operands(e.increment ? "++" : "--", *target, nullptr, range);
        return nullptr;
    }
    BIncDec inc;
    inc.increment = e.increment;
    inc.prefix = e.prefix;
    inc.kind = ImplicitConversions::scalar_kind(types, type);
    if (types.is_pointer(type)) {
        inc.elem_cells = std::max<std::uint32_t>(1, types.cells(types.info(type).target));
    }
    if (types.is_integral(type) && TypeTable::integer_bits(types.kind(type)) < 32) {
        inc.store_conv = ctx_.conversions.steps(TypeTable::kInt, type);  // char, short: computed as int
    }
    inc.target = std::move(target);
    return make(type, e.prefix, range, std::move(inc));
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks): owned by the variant

BExprPtr ExpressionBinder::conditional(const ast::ConditionalExpr& c, SourceRange range) {
    const TypeTable& types = ctx_.types;
    auto cond = bind_condition(*c.condition);
    auto a = bind(*c.then_expr);
    auto b = bind(*c.else_expr);
    if (!cond || !a || !b) {
        return nullptr;
    }
    if (a->lvalue && b->lvalue && a->type == b->type && !types.is_array(a->type)) {
        const TypeRef type = a->type;
        const bool is_const = a->is_const || b->is_const;
        auto e = make(type, true, range, BConditional{std::move(cond), std::move(a), std::move(b)});
        e->is_const = is_const;
        return e;
    }
    a = to_rvalue(std::move(a));
    b = to_rvalue(std::move(b));
    TypeRef type = 0;
    const bool arithmetic = types.is_arithmetic(a->type) && types.is_arithmetic(b->type);
    if (a->type == b->type ||
        (!arithmetic && ctx_.conversions.rank(*b, a->type) != ConversionRank::None && !types.is_record(a->type))) {
        type = a->type;
    } else if (arithmetic) {
        type = ctx_.conversions.common_arithmetic(a->type, b->type);
    } else if (ctx_.conversions.rank(*a, b->type) != ConversionRank::None && !types.is_record(b->type)) {
        type = b->type;
    } else {
        report_invalid_operands("?:", *a, b.get(), range);
        return nullptr;
    }
    if (types.is_record(type)) {
        ctx_.report(DiagnosticFactory::invalid_operands(range, "?:", types.name(type), types.name(type)));
        return nullptr;
    }
    a = ctx_.conversions.convert(std::move(a), type);
    b = ctx_.conversions.convert(std::move(b), type);
    return make(type, false, range, BConditional{std::move(cond), std::move(a), std::move(b)});
}

BExprPtr ExpressionBinder::subscript(const ast::SubscriptExpr& s, SourceRange range) {
    const TypeTable& types = ctx_.types;
    auto base = bind(*s.base);
    auto index = bind(*s.index);
    if (!base || !index) {
        return nullptr;
    }
    if (!types.is_record(base->type)) {
        index = to_rvalue(std::move(index));
    }
    if (types.is_record(base->type)) {
        if (has_operator("operator[]", *base, index.get())) {
            return call_operator("operator[]", std::move(base), std::move(index), range);
        }
        ctx_.report(DiagnosticFactory::not_subscriptable(s.base->range, types.name(base->type)));
        return nullptr;
    }
    if (!types.is_array(base->type) && !types.is_pointer(base->type)) {
        ctx_.report(DiagnosticFactory::not_subscriptable(s.base->range, types.name(base->type)));
        return nullptr;
    }
    if (!types.is_integral(index->type)) {
        report_invalid_operands("[]", *base, index.get(), range);
        return nullptr;
    }
    index = ctx_.conversions.convert(std::move(index), TypeTable::kLong);
    BIndex idx;
    TypeRef element = 0;
    bool is_const = false;
    if (types.is_array(base->type) && base->lvalue) {
        element = types.info(base->type).target;
        idx.bound = types.info(base->type).count;
        is_const = base->is_const;
        idx.base = std::move(base);
    } else {
        base = to_rvalue(std::move(base));
        element = types.info(base->type).target;
        is_const = types.info(base->type).target_const;
        idx.base_is_pointer = true;
        idx.base = std::move(base);
    }
    idx.index = std::move(index);
    idx.elem_cells = types.cells(element);
    auto e = make(element, true, range, std::move(idx));
    e->is_const = is_const;
    return e;
}

BExprPtr ExpressionBinder::member(const ast::MemberExpr& m, SourceRange range) {
    const TypeTable& types = ctx_.types;
    auto base = bind(*m.base);
    if (!base) {
        return nullptr;
    }
    if (m.arrow) {
        base = through_arrow(std::move(base), m.base->range);
        if (!base) {
            return nullptr;
        }
        base = to_rvalue(std::move(base));
        if (!types.is_pointer(base->type) || !types.is_record(types.info(base->type).target)) {
            report_invalid_operands("->", *base, nullptr, m.base->range);
            return nullptr;
        }
        const TypeInfo info = types.info(base->type);
        BDeref deref;
        deref.cells = types.cells(info.target);
        deref.pointer = std::move(base);
        base = make(info.target, true, m.base->range, std::move(deref));
        base->is_const = info.target_const;
    }
    if (!types.is_record(base->type)) {
        ctx_.report(DiagnosticFactory::no_member(m.member_range, types.name(base->type), m.member, std::nullopt));
        return nullptr;
    }
    return member_of(std::move(base), m.member, m.member_range, range, false);
}

BExprPtr ExpressionBinder::member_of(BExprPtr object, const std::string& name, SourceRange name_range,
                                     SourceRange range, bool want_methods) {
    TypeTable& types = ctx_.types;
    const std::uint32_t record = types.info(object->type).decl;
    const MemberResult m = members_.find(record, name);
    switch (m.kind) {
        case MemberResult::Kind::None:
            if (Symbol* member = static_member(record, name)) {
                return variable(*member, name, range);  // `object.count`
            }
            ctx_.report(DiagnosticFactory::no_member(name_range, types.name(object->type), name,
                                                     NameSuggester::closest(name, members_.names(record))));
            return nullptr;
        case MemberResult::Kind::Ambiguous:
            ctx_.report(DiagnosticFactory::ambiguous_name(name_range, name));
            return nullptr;
        case MemberResult::Kind::Methods:
            if (!want_methods) {
                ctx_.report(DiagnosticFactory::function_not_called(name_range, name, Severity::Error));
            }
            return nullptr;
        case MemberResult::Kind::Field: break;
    }
    if (!members_.accessible(m.access, m.owner, ctx_.fn != nullptr ? ctx_.fn->record : std::nullopt)) {
        ctx_.report(DiagnosticFactory::inaccessible_member(name_range, name, types.record_at(m.owner).name,
                                                           m.access == ast::Access::Private ? "private" : "protected"));
        return nullptr;
    }
    auto sub = base_subobject(std::move(object), m.owner);
    const bool is_const = sub->is_const || m.field->is_const;
    BMember mem;
    mem.base = std::move(sub);
    mem.offset = m.field->offset;
    mem.cells = types.cells(m.field->type);
    auto e = make(m.field->type, true, range, std::move(mem));
    e->is_const = is_const;
    if (m.field->deref) {
        // A lambda's by-reference capture: the member points to the variable.
        const TypeInfo pointer = types.info(m.field->type);
        auto loaded = make(m.field->type, false, range, BLoad{std::move(e)});
        BDeref deref;
        deref.cells = types.cells(pointer.target);
        deref.pointer = std::move(loaded);
        auto target = make(pointer.target, true, range, std::move(deref));
        target->is_const = pointer.target_const;
        return target;
    }
    return e;
}

BExprPtr ExpressionBinder::through_arrow(BExprPtr base, SourceRange range) {
    // `p->x` on a class (a smart pointer): apply its operator-> until a pointer comes out.
    for (int depth = 0; depth < 8 && base && ctx_.types.is_record(base->type); ++depth) {
        if (!has_operator("operator->", *base, nullptr)) {
            break;
        }
        base = call_operator("operator->", std::move(base), nullptr, range);
    }
    return base;
}

BExprPtr ExpressionBinder::cast(const ast::CastExpr& c, SourceRange range) {
    const TypeTable& types = ctx_.types;
    auto target = ctx_.type_resolver->resolve(c.type);
    if (!target) {
        return nullptr;
    }
    if (target->is_auto) {
        ctx_.report(DiagnosticFactory::unsupported_type(range, "auto"));
        return nullptr;
    }
    if (types.is_void(target->type)) {
        auto operand = bind(*c.operand, Use::Statement);
        if (!operand) {
            return nullptr;
        }
        operand->type = TypeTable::kVoid;
        operand->lvalue = false;
        return operand;
    }
    auto operand = bind_rvalue(*c.operand);
    if (!operand) {
        return nullptr;
    }
    if (types.is_record(operand->type) && !types.is_record(target->type)) {
        if (auto op = ctx_.overloads->conversion_operator(operand->type, target->type, true)) {
            operand = to_rvalue(call_conversion(std::move(operand), *op));
            if (!operand) {
                return nullptr;
            }
        }
    }
    if (types.is_record(target->type) || types.is_record(operand->type) ||
        !ctx_.conversions.can_cast(*operand, target->type)) {
        ctx_.report(DiagnosticFactory::cannot_convert(range, types.name(operand->type), types.name(target->type)));
        return nullptr;
    }
    auto out = ctx_.conversions.cast(std::move(operand), target->type);
    out->range = range;
    return out;
}

BExprPtr ExpressionBinder::size_of(const ast::SizeofExpr& s, SourceRange range) {
    const TypeTable& types = ctx_.types;
    TypeRef type = 0;
    if (s.type) {
        auto resolved = ctx_.type_resolver->resolve(*s.type);
        if (!resolved || resolved->is_auto) {
            return nullptr;
        }
        type = resolved->type;
    } else {
        auto operand = bind(*s.operand);
        if (!operand) {
            return nullptr;
        }
        type = operand->type;
    }
    if (types.is_reference(type)) {
        type = types.info(type).target;
    }
    return constant(TypeTable::kLong, types.byte_size(type), range);
}

BExprPtr ExpressionBinder::new_expr(const ast::NewExpr& n, SourceRange range) {
    TypeTable& types = ctx_.types;
    auto resolved = ctx_.type_resolver->resolve(n.type);
    if (!resolved || resolved->is_auto || types.is_void(resolved->type) || types.is_reference(resolved->type)) {
        if (resolved) {
            ctx_.report(DiagnosticFactory::unsupported_type(range, types.name(resolved->type)));
        }
        return nullptr;
    }
    const TypeRef type = resolved->type;
    BNew node;
    node.elem_cells = std::max<std::uint32_t>(1, types.cells(type));
    if (n.array_size) {
        node.count = bind_converted(*n.array_size, TypeTable::kLong);
        if (!node.count) {
            return nullptr;
        }
        node.zero = n.has_parens;
        if (!n.args.empty()) {
            ctx_.report(DiagnosticFactory::unsupported_syntax(range, "array new with initializers"));
            return nullptr;
        }
    }
    if (types.is_record(type)) {
        const RecordInfo& info = types.record(type);
        node.record = types.info(type).decl;
        if (info.is_abstract) {
            ctx_.report(DiagnosticFactory::abstract_class(range, info.name, std::nullopt));
            return nullptr;
        }
        std::vector<BExprPtr> args;
        for (const auto& a : n.args) {
            auto bound = bind(a);
            if (!bound) {
                return nullptr;
            }
            args.push_back(std::move(bound));
        }
        // `new Point(other)` for a class without constructors: a plain copy of the cells.
        if (!info.user_constructors && info.constructors.empty() && args.size() == 1 && !n.array_size &&
            args.front()->type == type) {
            return heap_copy(std::move(node), std::move(args.front()), type, resolved->is_const, range);
        }
        if (info.user_constructors || (!args.empty() && !info.constructors.empty())) {
            std::vector<Callee> ctors;
            ctors.reserve(info.constructors.size());
            for (const std::uint32_t id : info.constructors) {
                ctors.push_back(Callee{false, id});
            }
            if (n.array_size) {
                if (!args.empty()) {
                    ctx_.report(DiagnosticFactory::unsupported_syntax(range, "array new with initializers"));
                    return nullptr;
                }
                std::optional<std::uint32_t> default_ctor;
                for (const std::uint32_t id : info.constructors) {
                    if (ctx_.overloads->defaults(Callee{false, id}) == ctx_.functions[id].params.size()) {
                        default_ctor = id;
                    }
                }
                if (!default_ctor) {
                    ctx_.report(DiagnosticFactory::no_default_constructor(range, info.name));
                    return nullptr;
                }
                const std::uint32_t ctor = *default_ctor;  // NOLINT(bugprone-unchecked-optional-access): checked above
                node.constructor = ctor;
                if (!ctx_.functions[ctor].params.empty()) {
                    ctx_.report(DiagnosticFactory::unsupported_syntax(range, "default arguments in array new"));
                    return nullptr;
                }
                node.loop_temps = ctx_.allocate_local(3);
                return make(types.pointer_to(type, resolved->is_const), false, range, std::move(node));
            }
            auto chosen = ctx_.overloads->resolve(info.name, ctors, args, range);
            if (!chosen) {
                return nullptr;
            }
            auto prepared = ctx_.overloads->prepare(*chosen, std::move(args), range);
            if (!prepared) {
                return nullptr;
            }
            node.constructor = chosen->id;
            node.ctor_args = std::move(*prepared);
        } else if (!args.empty()) {
            ctx_.report(DiagnosticFactory::no_matching_function(range, info.name, ctx_.overloads->describe(args)));
            return nullptr;
        } else {
            node.zero = n.has_parens;
            // Members with constructors or default initializers: the implicit
            // default constructor runs on every object.
            const bool has_header = info.has_header;  // binding the constructor may add records
            if (!ctx_.initializers->trivial_default_initialization(type)) {
                node.constructor = ctx_.declarations->implicit_default_constructor(*node.record, range);
            }
            if (n.array_size && (has_header || node.constructor)) {
                node.loop_temps = ctx_.allocate_local(3);  // headers and constructors for every element
            }
        }
    } else if (!n.args.empty()) {
        if (n.args.size() != 1) {
            ctx_.report(DiagnosticFactory::too_many_initializers(range, 1, n.args.size()));
            return nullptr;
        }
        node.scalar_init = bind_converted(n.args.front(), type);
        if (!node.scalar_init) {
            return nullptr;
        }
    } else {
        node.zero = n.has_parens;
    }
    return make(types.pointer_to(type, resolved->is_const), false, range, std::move(node));
}

BExprPtr ExpressionBinder::lambda(const ast::LambdaExpr& l, SourceRange range) {
    TypeTable& types = ctx_.types;
    if (l.captures_this) {
        ctx_.report(DiagnosticFactory::unsupported_syntax(range, "capturing this"));
        return nullptr;
    }
    std::vector<DeclarationBinder::Capture> captures;
    std::vector<BExprPtr> sources;  // what each capture is initialized from
    std::set<std::string> taken;
    for (const auto& p : l.function->params) {
        taken.insert(p.declarator.name);
    }
    auto capture_variable = [&](const std::string& name, bool by_reference, SourceRange at, bool explicit_capture) {
        // Script mode: top-level variables are globals, and naming one in the
        // capture list copies it like a local (C++ would reject the capture).
        Symbol* symbol = explicit_capture ? ctx_.symbols.lookup(name) : ctx_.symbols.lookup_local(name);
        if (symbol == nullptr || symbol->kind != SymbolKind::Variable) {
            if (explicit_capture) {
                ctx_.report(DiagnosticFactory::unknown_identifier(at, name, std::nullopt));
                return false;
            }
            return true;  // globals and functions are used directly
        }
        captures.push_back(DeclarationBinder::Capture{name, by_reference, symbol->type, symbol->is_const});
        sources.push_back(variable(*symbol, name, at));
        taken.insert(name);
        return true;
    };
    for (const auto& c : l.captures) {
        if (c.init) {
            auto value = bind(*c.init);
            if (!value) {
                return nullptr;
            }
            if (!c.by_reference && !types.is_record(value->type)) {
                value = to_rvalue(std::move(value));
            }
            if (c.by_reference && !value->lvalue) {
                ctx_.report(DiagnosticFactory::reference_needs_lvalue(c.range, types.name(value->type)));
                return nullptr;
            }
            captures.push_back(DeclarationBinder::Capture{c.name, c.by_reference, value->type, value->is_const});
            sources.push_back(std::move(value));
            taken.insert(c.name);
            continue;
        }
        if (!capture_variable(c.name, c.by_reference, c.range, true)) {
            return nullptr;
        }
    }
    if (l.default_capture != 0 && l.function->body) {
        for (const std::string& name : CaptureAnalysis::names(*l.function->body)) {
            if (taken.find(name) == taken.end()) {
                (void)capture_variable(name, l.default_capture == '&', range, false);
            }
        }
    }

    auto closure = ctx_.declarations->bind_lambda(l, captures, range);
    if (!closure) {
        return nullptr;
    }
    // The lambda expression is a temporary closure object holding the captures.
    BTemp temp;
    temp.cells = types.cells(*closure);
    temp.offset = ctx_.allocate_local(temp.cells);
    BVar slot;
    slot.offset = temp.offset;
    slot.cells = temp.cells;
    const RecordInfo& info = types.record(*closure);
    for (std::size_t i = 0; i < captures.size(); ++i) {
        const FieldInfo& field = info.fields[i];
        BMember m;
        m.base = make(*closure, true, range, slot);
        m.offset = field.offset;
        m.cells = types.cells(field.type);
        auto target = make(field.type, true, range, std::move(m));
        if (captures[i].by_reference) {
            const TypeRef pointer = field.type;
            BStmt store;
            store.range = range;
            store.node = BStore{std::move(target), make(pointer, false, range, BAddressOf{std::move(sources[i])})};
            temp.init_stmts.push_back(std::move(store));
        } else if (!ctx_.initializers->initialize_from(std::move(target), field.type, std::move(sources[i]),
                                                       temp.init_stmts)) {
            return nullptr;
        }
    }
    if (ctx_.initializers->needs_destruction(*closure)) {
        ctx_.fn->temporaries.push_back(FunctionContext::Temporary{temp.offset, *closure});
    }
    auto e = make(*closure, true, range, std::move(temp));
    e->expiring = true;
    return e;
}

BExprPtr ExpressionBinder::delete_expr(const ast::DeleteExpr& d, SourceRange range) {
    TypeTable& types = ctx_.types;
    auto pointer = bind_rvalue(*d.operand);
    if (!pointer) {
        return nullptr;
    }
    if (!types.is_pointer(pointer->type)) {
        report_invalid_operands(d.array ? "delete[]" : "delete", *pointer, nullptr, range);
        return nullptr;
    }
    const TypeRef target = types.info(pointer->type).target;
    BDelete node;
    node.array = d.array;
    node.elem_cells = std::max<std::uint32_t>(1, types.cells(target));
    if (types.is_record(target)) {
        const RecordInfo& info = types.record(target);
        node.destructor = info.destructor;
        if (info.destructor) {
            node.virtual_destructor = ctx_.functions[*info.destructor].is_virtual && !d.array;
            if (d.array) {
                node.loop_temps = ctx_.allocate_local(2);
            }
        }
    }
    node.pointer = std::move(pointer);
    return make(TypeTable::kVoid, false, range, std::move(node));
}

}  // namespace cppi::sema
