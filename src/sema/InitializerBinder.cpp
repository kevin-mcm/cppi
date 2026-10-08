/// @file InitializerBinder.cpp
/// @brief Implementation of InitializerBinder.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/InitializerBinder.hpp"

#include "sema/ExpressionBinder.hpp"
#include "sema/LvalueCloner.hpp"
#include "sema/OverloadResolver.hpp"
#include "sema/TemplateEngine.hpp"
#include "support/DiagnosticFactory.hpp"

#include <algorithm>
#include <utility>
#include <variant>

namespace cppi::sema {

using detail::DiagnosticFactory;

namespace {

bool is_copy_constructor(const AnalysisContext& ctx, std::uint32_t function, TypeRef record) {
    const FunctionInfo& fn = ctx.functions[function];
    if (fn.params.size() != 1 || !ctx.types.is_reference(fn.params.front().type)) {
        return false;
    }
    return ctx.types.info(fn.params.front().type).target == record;
}

}  // namespace

BExprPtr InitializerBinder::element(const BExpr& array, std::uint32_t index) const {
    const TypeInfo info = ctx_.types.info(array.type);
    BIndex idx;
    idx.base = LvalueCloner::clone(array);
    idx.index = ExpressionBinder::constant(TypeTable::kLong, index, array.range);
    idx.elem_cells = ctx_.types.cells(info.target);
    idx.bound = info.count;
    auto e = ExpressionBinder::make(info.target, true, array.range, std::move(idx));
    e->is_const = array.is_const;
    return e;
}

BExprPtr InitializerBinder::field_of(const BExpr& record, const FieldInfo& field) const {
    BMember m;
    m.base = LvalueCloner::clone(record);
    m.offset = field.offset;
    m.cells = ctx_.types.cells(field.type);
    return ExpressionBinder::make(field.type, true, record.range, std::move(m));
}

BExprPtr InitializerBinder::base_of(const BExpr& record, const BaseInfo& base) const {
    return ctx_.expressions->base_subobject(LvalueCloner::clone(record), base.record);
}

// =============================================================================

bool InitializerBinder::initialize(BExprPtr target, TypeRef type, const ast::VarDeclarator& var, bool fresh_local,
                                   std::vector<BStmt>& out) {
    switch (var.style) {
        case ast::InitStyle::None: return default_initialize(std::move(target), type, fresh_local, var.range, out);
        case ast::InitStyle::Copy:
        case ast::InitStyle::List:
            if (const auto* list = std::get_if<ast::InitList>(&var.init->node)) {
                if (fresh_local && ctx_.types.cells(type) > 0) {
                    out.push_back(stmt(var.range, BUninit{LvalueCloner::clone(*target), ctx_.types.cells(type)}));
                }
                return initialize_list(std::move(target), type, *list, var.init->range, out);
            }
            return initialize_expr(std::move(target), type, *var.init, out);
        case ast::InitStyle::Direct: {
            if (ctx_.types.is_record(type)) {
                std::vector<BExprPtr> args;
                for (const auto& a : var.args) {
                    auto bound = ctx_.expressions->bind(a);
                    if (!bound) {
                        return false;
                    }
                    args.push_back(std::move(bound));
                }
                if (fresh_local) {
                    out.push_back(stmt(var.range, BUninit{LvalueCloner::clone(*target), ctx_.types.cells(type)}));
                }
                return construct(std::move(target), type, std::move(args), var.range, out);
            }
            if (var.args.size() != 1) {
                ctx_.report(DiagnosticFactory::too_many_initializers(var.range, 1, var.args.size()));
                return false;
            }
            return initialize_expr(std::move(target), type, var.args.front(), out);
        }
    }
    return false;
}

bool InitializerBinder::initialize_expr(BExprPtr target, TypeRef type, const ast::Expr& init, std::vector<BStmt>& out) {
    if (ctx_.types.is_array(type)) {
        if (std::holds_alternative<ast::StringLiteral>(init.node) &&
            ctx_.types.kind(ctx_.types.info(type).target) == TypeKind::Char) {
            const auto& text = std::get<ast::StringLiteral>(init.node).value;
            const TypeInfo info = ctx_.types.info(type);
            if (text.size() + 1 > info.count) {
                ctx_.report(DiagnosticFactory::too_many_initializers(init.range, info.count, text.size() + 1));
                return false;
            }
            out.push_back(stmt(init.range, BZero{LvalueCloner::clone(*target), info.count}));
            for (std::uint32_t i = 0; i < text.size(); ++i) {
                out.push_back(stmt(
                    init.range,
                    BStore{element(*target, i), ExpressionBinder::constant(
                                                    TypeTable::kChar, static_cast<signed char>(text[i]), init.range)}));
            }
            return true;
        }
        auto value = ctx_.expressions->bind(init);
        if (value) {
            ctx_.report(
                DiagnosticFactory::cannot_convert(init.range, ctx_.types.name(value->type), ctx_.types.name(type)));
        }
        return false;
    }
    auto value = ctx_.expressions->bind(init);
    if (!value) {
        return false;
    }
    return initialize_from(std::move(target), type, std::move(value), out);
}

bool InitializerBinder::initialize_from(BExprPtr target, TypeRef type, BExprPtr value, std::vector<BStmt>& out) {
    const TypeTable& types = ctx_.types;
    const SourceRange range = value->range;
    if (types.is_record(type)) {
        std::vector<BExprPtr> args;
        args.push_back(std::move(value));
        return construct(std::move(target), type, std::move(args), range, out);
    }
    auto converted = ctx_.expressions->convert_to(ctx_.expressions->to_rvalue(std::move(value)), type);
    if (!converted) {
        return false;
    }
    out.push_back(stmt(range, BStore{std::move(target), std::move(converted)}));
    return true;
}

bool InitializerBinder::initialize_list(BExprPtr target, TypeRef type, const ast::InitList& list, SourceRange range,
                                        std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    const auto& items = list.elements;

    if (types.is_scalar(type)) {
        if (items.empty()) {
            out.push_back(stmt(range, BZero{std::move(target), 1}));
            return true;
        }
        if (items.size() > 1) {
            ctx_.report(DiagnosticFactory::too_many_initializers(range, 1, items.size()));
            return false;
        }
        return initialize_expr(std::move(target), type, items.front(), out);
    }

    if (types.is_array(type)) {
        const TypeInfo info = types.info(type);
        if (items.size() > info.count) {
            ctx_.report(DiagnosticFactory::too_many_initializers(range, info.count, items.size()));
            return false;
        }
        const TypeRef elem = info.target;
        const bool record_elements = types.is_record(elem) && types.record(elem).user_constructors;
        if (items.size() < info.count && !record_elements) {
            out.push_back(stmt(range, BZero{LvalueCloner::clone(*target), types.cells(type)}));
        }
        for (std::uint32_t i = 0; i < info.count; ++i) {
            auto slot = element(*target, i);
            if (i >= items.size()) {
                if (record_elements && !construct(std::move(slot), elem, {}, range, out)) {
                    return false;
                }
                continue;
            }
            const ast::Expr& item = items[i];
            bool ok = false;
            if (const auto* sub = std::get_if<ast::InitList>(&item.node)) {
                ok = initialize_list(std::move(slot), elem, *sub, item.range, out);
            } else {
                ok = initialize_expr(std::move(slot), elem, item, out);
            }
            if (!ok) {
                return false;
            }
        }
        return true;
    }

    if (types.is_record(type)) {
        RecordInfo& info = types.record(type);
        if (info.user_constructors || !info.bases.empty() || info.has_header) {
            // A class: the braces hold constructor arguments (C++11).
            std::vector<BExprPtr> args;
            for (const auto& item : items) {
                auto bound = ctx_.expressions->bind(item);
                if (!bound) {
                    return false;
                }
                args.push_back(std::move(bound));
            }
            return construct(std::move(target), type, std::move(args), range, out);
        }
        // An aggregate: one initializer per field, in order.
        if (items.size() > info.fields.size()) {
            ctx_.report(DiagnosticFactory::too_many_initializers(range, info.fields.size(), items.size()));
            return false;
        }
        if (items.size() < info.fields.size()) {
            out.push_back(stmt(range, BZero{LvalueCloner::clone(*target), types.cells(type)}));
        }
        for (std::size_t i = 0; i < info.fields.size(); ++i) {
            const FieldInfo field = info.fields[i];
            auto slot = field_of(*target, field);
            bool ok = true;
            if (i < items.size()) {
                const ast::Expr& item = items[i];
                if (const auto* sub = std::get_if<ast::InitList>(&item.node)) {
                    ok = initialize_list(std::move(slot), field.type, *sub, item.range, out);
                } else {
                    ok = initialize_expr(std::move(slot), field.type, item, out);
                }
            } else if (field.default_init != nullptr) {
                ok = initialize(std::move(slot), field.type, *field.default_init, false, out);
            } else if (types.is_record(field.type)) {
                ok = default_initialize(std::move(slot), field.type, false, range, out);
            }
            if (!ok) {
                return false;
            }
        }
        return true;
    }
    ctx_.report(DiagnosticFactory::cannot_convert(range, "{...}", types.name(type)));
    return false;
}

bool InitializerBinder::construct(BExprPtr target, TypeRef type, std::vector<BExprPtr> args, SourceRange range,
                                  std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    if (!types.is_record(type)) {
        if (args.empty()) {
            out.push_back(stmt(range, BZero{std::move(target), std::max<std::uint32_t>(1, types.cells(type))}));
            return true;
        }
        if (args.size() > 1) {
            ctx_.report(DiagnosticFactory::too_many_initializers(range, 1, args.size()));
            return false;
        }
        auto value = ctx_.expressions->convert_to(ctx_.expressions->to_rvalue(std::move(args.front())), type);
        if (!value) {
            return false;
        }
        out.push_back(stmt(range, BStore{std::move(target), std::move(value)}));
        return true;
    }
    const std::uint32_t record = types.info(type).decl;
    const RecordInfo& info = types.record_at(record);
    if (info.is_abstract) {
        std::optional<std::string> pure;
        for (const auto& sub : info.subobjects) {
            for (const auto& slot : sub.slots) {
                if (slot.is_pure && !pure) {
                    pure = slot.name;
                }
            }
        }
        ctx_.report(DiagnosticFactory::abstract_class(range, info.name, pure));
        return false;
    }

    // Copying from an object of the same class (or a derived one) uses the
    // implicit member-wise copy unless the class declares a copy constructor.
    const bool copy_source = args.size() == 1 && types.is_record(args.front()->type) &&
                             (args.front()->type == type ||
                              ctx_.hierarchy.find_base(types.info(args.front()->type).decl, record).has_value());
    bool user_copy = false;
    for (const std::uint32_t id : info.constructors) {
        user_copy = user_copy || is_copy_constructor(ctx_, id, type);
    }
    if (copy_source && !user_copy) {
        auto source = ctx_.expressions->base_subobject(std::move(args.front()), record);
        out.push_back(stmt(range, BCopy{LvalueCloner::clone(*target), std::move(source), types.cells(type)}));
        if (info.has_header) {
            out.push_back(stmt(range, BInitHeaders{std::move(target), record}));
        }
        return true;
    }

    if (!info.user_constructors && !(copy_source && user_copy)) {
        if (!args.empty()) {
            ctx_.report(DiagnosticFactory::no_matching_function(range, info.name, ctx_.overloads->describe(args)));
            return false;
        }
        // `T()` value-initializes: zero, then default member initializers.
        out.push_back(stmt(range, BZero{LvalueCloner::clone(*target), types.cells(type)}));
        return default_initialize(std::move(target), type, false, range, out);
    }

    std::vector<Callee> ctors;
    ctors.reserve(info.constructors.size());
    for (const std::uint32_t id : info.constructors) {
        ctors.push_back(Callee{false, id});
    }
    if (args.empty()) {
        bool has_default = false;
        for (const std::uint32_t id : info.constructors) {
            has_default =
                has_default || ctx_.overloads->defaults(Callee{false, id}) == ctx_.functions[id].params.size();
        }
        if (!has_default) {
            ctx_.report(DiagnosticFactory::no_default_constructor(range, info.name));
            return false;
        }
    }
    auto chosen = ctx_.overloads->resolve(info.name, ctors, args, range);
    if (!chosen) {
        return false;
    }
    const Callee callee = *chosen;
    auto prepared = ctx_.overloads->prepare(callee, std::move(args), range);
    if (!prepared) {
        return false;
    }
    const FunctionInfo& fn = ctx_.functions[chosen->id];
    if (!ctx_.expressions->members().accessible(fn.access, record,
                                                ctx_.fn != nullptr ? ctx_.fn->record : std::nullopt)) {
        ctx_.report(DiagnosticFactory::inaccessible_member(
            range, info.name, info.name, fn.access == ast::Access::Private ? "private" : "protected"));
        return false;
    }
    if (info.has_header) {
        out.push_back(stmt(range, BInitHeaders{LvalueCloner::clone(*target), record}));
    }
    if (!init_virtual_bases(*target, record, range, out)) {
        return false;
    }
    std::vector<BExprPtr> call_args;
    call_args.push_back(ExpressionBinder::make(types.pointer_to(type), false, range, BAddressOf{std::move(target)}));
    for (auto& a : *prepared) {
        call_args.push_back(std::move(a));
    }
    auto call = ctx_.expressions->make_call(callee, std::move(call_args), range);
    out.push_back(stmt(range, BExprStmt{std::move(*call)}));
    return true;
}

bool InitializerBinder::init_virtual_bases(const BExpr& target, std::uint32_t record, SourceRange range,
                                           std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    for (const auto& [vbase, offset] : types.record_at(record).virtual_base_offsets) {
        auto slot = ctx_.expressions->base_subobject(LvalueCloner::clone(target), vbase);
        const RecordInfo& info = types.record_at(vbase);
        const bool ok = !info.user_constructors
                            ? default_initialize(std::move(slot), types.record_type(vbase), false, range, out, false)
                            : construct_base(std::move(slot), vbase, {}, range, out);
        if (!ok) {
            return false;
        }
        (void)offset;
    }
    return true;
}

bool InitializerBinder::default_initialize(BExprPtr target, TypeRef type, bool fresh_local, SourceRange range,
                                           std::vector<BStmt>& out, bool complete) {
    TypeTable& types = ctx_.types;
    if (types.is_array(type)) {
        const TypeInfo info = types.info(type);
        if (types.is_record(info.target) &&
            (types.record(info.target).user_constructors || types.record(info.target).has_header ||
             !types.record(info.target).trivial_copy || types.record(info.target).destructor)) {
            for (std::uint32_t i = 0; i < info.count; ++i) {
                if (!default_initialize(element(*target, i), info.target, fresh_local, range, out)) {
                    return false;
                }
            }
            return true;
        }
        if (fresh_local) {
            out.push_back(stmt(range, BUninit{std::move(target), types.cells(type)}));
        }
        return true;
    }
    if (!types.is_record(type)) {
        if (fresh_local) {
            out.push_back(stmt(range, BUninit{std::move(target), std::max<std::uint32_t>(1, types.cells(type))}));
        }
        return true;
    }
    const RecordInfo& info = types.record(type);
    if (info.user_constructors) {
        if (fresh_local) {
            out.push_back(stmt(range, BUninit{LvalueCloner::clone(*target), types.cells(type)}));
        }
        return construct(std::move(target), type, {}, range, out);
    }
    if (info.is_abstract) {
        return construct(std::move(target), type, {}, range, out);  // reports
    }
    if (fresh_local) {
        out.push_back(stmt(range, BUninit{LvalueCloner::clone(*target), types.cells(type)}));
    }
    if (complete && info.has_header) {
        out.push_back(stmt(range, BInitHeaders{LvalueCloner::clone(*target), types.info(type).decl}));
    }
    if (complete && !init_virtual_bases(*target, types.info(type).decl, range, out)) {
        return false;
    }
    // Implicit default constructor: bases, then members with initializers or constructors.
    for (const BaseInfo& base : info.bases) {
        if (base.is_virtual) {
            continue;  // initialized once, by the complete object
        }
        const RecordInfo& base_info = types.record_at(base.record);
        const bool ok = !base_info.user_constructors
                            ? default_initialize(base_of(*target, base), base.type, false, range, out, false)
                            : construct_base(base_of(*target, base), base.record, {}, range, out);
        if (!ok) {
            return false;
        }
    }
    const auto fields = info.fields;
    for (const FieldInfo& field : fields) {
        auto slot = field_of(*target, field);
        if (field.default_init != nullptr) {
            if (!initialize(std::move(slot), field.type, *field.default_init, false, out)) {
                return false;
            }
        } else if (types.is_record(field.type) || types.is_array(field.type)) {
            if (!default_initialize(std::move(slot), field.type, false, range, out)) {
                return false;
            }
        }
    }
    return true;
}

bool InitializerBinder::bind_reference(BExprPtr slot, TypeRef reference_type, const ast::Expr& init,
                                       std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    const TypeInfo ref = types.info(reference_type);
    auto value = ctx_.expressions->bind(init);
    if (!value) {
        return false;
    }
    const SourceRange range = value->range;
    if (value->lvalue && types.is_record(value->type) && value->type != ref.target && types.is_record(ref.target) &&
        ctx_.hierarchy.find_base(types.info(value->type).decl, types.info(ref.target).decl)) {
        value = ctx_.expressions->base_subobject(std::move(value), types.info(ref.target).decl);
    }
    if (value->lvalue && value->type == ref.target && (ref.target_const || !value->is_const)) {
        auto address = ExpressionBinder::make(types.pointer_to(ref.target, ref.target_const), false, range,
                                              BAddressOf{std::move(value)});
        out.push_back(stmt(range, BStore{std::move(slot), std::move(address)}));
        return true;
    }
    if (!ref.target_const) {
        ctx_.report(DiagnosticFactory::reference_needs_lvalue(range, types.name(reference_type)));
        return false;
    }
    BExprPtr object;
    if (types.is_record(ref.target)) {
        if (value->type != ref.target) {
            ctx_.report(DiagnosticFactory::cannot_convert(range, types.name(value->type), types.name(ref.target)));
            return false;
        }
        object = std::move(value);  // a temporary record (e.g. a call result)
    } else {
        auto converted = ctx_.expressions->convert_to(ctx_.expressions->to_rvalue(std::move(value)), ref.target);
        if (!converted) {
            return false;
        }
        object = ctx_.expressions->materialize(std::move(converted));
    }
    auto address =
        ExpressionBinder::make(types.pointer_to(ref.target, true), false, range, BAddressOf{std::move(object)});
    out.push_back(stmt(range, BStore{std::move(slot), std::move(address)}));
    return true;
}

bool InitializerBinder::trivial_default_initialization(TypeRef type) const {
    const TypeTable& types = ctx_.types;
    if (types.is_array(type)) {
        return trivial_default_initialization(types.info(type).target);
    }
    if (!types.is_record(type)) {
        return true;
    }
    const RecordInfo& info = types.record(type);
    if (info.user_constructors) {
        return false;
    }
    for (const BaseInfo& base : info.bases) {
        if (base.is_virtual || !trivial_default_initialization(base.type)) {
            return false;
        }
    }
    for (const FieldInfo& field : info.fields) {
        if (field.default_init != nullptr || !trivial_default_initialization(field.type)) {
            return false;
        }
    }
    return true;
}

bool InitializerBinder::needs_destruction(TypeRef type) const {
    const TypeTable& types = ctx_.types;
    if (types.is_array(type)) {
        return needs_destruction(types.info(type).target);
    }
    if (!types.is_record(type)) {
        return false;
    }
    return types.record(type).destructor.has_value();
}

void InitializerBinder::destroy(BExprPtr target, TypeRef type, std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    const SourceRange range = target->range;
    if (types.is_array(type)) {
        const TypeInfo info = types.info(type);
        for (std::uint32_t i = info.count; i-- > 0;) {
            destroy(element(*target, i), info.target, out);
        }
        return;
    }
    if (!types.is_record(type)) {
        return;
    }
    const RecordInfo& info = types.record(type);
    if (!info.destructor) {
        return;
    }
    std::vector<BExprPtr> args;
    args.push_back(ExpressionBinder::make(types.pointer_to(type), false, range, BAddressOf{std::move(target)}));
    auto call = ctx_.expressions->make_call(Callee{false, *info.destructor}, std::move(args), range);
    out.push_back(stmt(range, BExprStmt{std::move(*call)}));
}

bool InitializerBinder::construct_members(std::uint32_t record, const ast::FunctionDecl* decl,
                                          std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    const SourceRange range = decl != nullptr ? decl->range : SourceRange{};
    auto self = ctx_.expressions->this_object(range);
    if (!self) {
        return false;
    }
    std::vector<const ast::MemberInit*> unused;
    if (decl != nullptr) {
        for (const auto& init : decl->inits) {
            unused.push_back(&init);
        }
    }
    auto take = [&](const std::string& name) -> const ast::MemberInit* {
        const std::string short_name = name.substr(0, name.find('<'));  // class template instances
        for (auto it = unused.begin(); it != unused.end(); ++it) {
            if ((*it)->name == name || (*it)->name == short_name) {
                const ast::MemberInit* found = *it;
                unused.erase(it);
                return found;
            }
        }
        return nullptr;
    };
    auto bind_args = [&](const ast::MemberInit& init) -> std::optional<std::vector<BExprPtr>> {
        std::vector<BExprPtr> args;
        for (const auto& a : init.args) {
            auto bound = ctx_.expressions->bind(a);
            if (!bound) {
                return std::nullopt;
            }
            args.push_back(std::move(bound));
        }
        return args;
    };

    const RecordInfo info = types.record_at(record);  // copy: binding may add types
    // `A() : A(0) {}` delegates the whole construction to another constructor.
    const std::string short_name = info.name.substr(0, info.name.find('<'));
    const auto delegation = std::find_if(unused.begin(), unused.end(), [&](const ast::MemberInit* init) {
        return init->name == info.name || init->name == short_name;
    });
    if (delegation != unused.end()) {
        const ast::MemberInit& init = **delegation;
        if (!ctx_.features.allow(Feature::DelegatingConstructors, init.range)) {
            return false;
        }
        if (unused.size() != 1) {
            ctx_.report(
                DiagnosticFactory::unsupported_syntax(init.range, "delegating constructor with other initializers"));
            return false;
        }
        auto args = bind_args(init);
        if (!args) {
            return false;
        }
        return construct_base(std::move(self), record, std::move(*args), init.range, out);
    }
    bool ok = true;
    for (const BaseInfo& base : info.bases) {
        const ast::MemberInit* init = take(types.record_at(base.record).name);
        auto slot = base_of(*self, base);
        if (base.is_virtual) {
            continue;  // constructed with the complete object
        }
        const RecordInfo& base_info = types.record_at(base.record);
        if (init != nullptr) {
            auto args = bind_args(*init);
            if (!args) {
                ok = false;
                continue;
            }
            if (!base_info.user_constructors && args->empty()) {
                ok = default_initialize(std::move(slot), base.type, false, init->range, out, false) && ok;
                continue;
            }
            ok = construct_base(std::move(slot), base.record, std::move(*args), init->range, out) && ok;
        } else if (base_info.user_constructors) {
            ok = construct_base(std::move(slot), base.record, {}, range, out) && ok;
        } else {
            ok = default_initialize(std::move(slot), base.type, false, range, out, false) && ok;
        }
    }
    for (const FieldInfo& field : info.fields) {
        const ast::MemberInit* init = take(field.name);
        auto slot = field_of(*self, field);
        if (init != nullptr) {
            auto args = bind_args(*init);
            if (!args) {
                ok = false;
                continue;
            }
            if (types.is_record(field.type)) {
                ok = construct(std::move(slot), field.type, std::move(*args), init->range, out) && ok;
            } else if (args->size() == 1) {
                ok = initialize_from(std::move(slot), field.type, std::move(args->front()), out) && ok;
            } else if (args->empty()) {
                out.push_back(
                    stmt(init->range, BZero{std::move(slot), std::max<std::uint32_t>(1, types.cells(field.type))}));
            } else {
                ctx_.report(DiagnosticFactory::too_many_initializers(init->range, 1, args->size()));
                ok = false;
            }
        } else if (field.default_init != nullptr) {
            ok = initialize(std::move(slot), field.type, *field.default_init, false, out) && ok;
        } else if (types.is_record(field.type) || types.is_array(field.type)) {
            ok = default_initialize(std::move(slot), field.type, false, range, out) && ok;
        }
    }
    for (const ast::MemberInit* init : unused) {
        ctx_.report(DiagnosticFactory::no_member(init->range, info.name, init->name, std::nullopt));
        ok = false;
    }
    return ok;
}

bool InitializerBinder::construct_base(BExprPtr target, std::uint32_t base_record, std::vector<BExprPtr> args,
                                       SourceRange range, std::vector<BStmt>& out) {
    // Like construct(), but for a base subobject: the complete object
    // already has its headers.
    TypeTable& types = ctx_.types;
    const RecordInfo& info = types.record_at(base_record);
    const TypeRef type = types.record_type(base_record);
    std::vector<Callee> ctors;
    ctors.reserve(info.constructors.size());
    for (const std::uint32_t id : info.constructors) {
        ctors.push_back(Callee{false, id});
    }
    if (ctors.empty()) {
        ctx_.report(DiagnosticFactory::no_matching_function(range, info.name, ctx_.overloads->describe(args)));
        return false;
    }
    if (args.empty()) {
        bool has_default = false;
        for (const std::uint32_t id : info.constructors) {
            has_default =
                has_default || ctx_.overloads->defaults(Callee{false, id}) == ctx_.functions[id].params.size();
        }
        if (!has_default) {
            ctx_.report(DiagnosticFactory::no_default_constructor(range, info.name));
            return false;
        }
    }
    auto chosen = ctx_.overloads->resolve(info.name, ctors, args, range);
    if (!chosen) {
        return false;
    }
    const Callee callee = *chosen;
    auto prepared = ctx_.overloads->prepare(callee, std::move(args), range);
    if (!prepared) {
        return false;
    }
    std::vector<BExprPtr> call_args;
    call_args.push_back(ExpressionBinder::make(types.pointer_to(type), false, range, BAddressOf{std::move(target)}));
    for (auto& a : *prepared) {
        call_args.push_back(std::move(a));
    }
    auto call = ctx_.expressions->make_call(callee, std::move(call_args), range);
    out.push_back(stmt(range, BExprStmt{std::move(*call)}));
    return true;
}

bool InitializerBinder::copy_members(std::uint32_t record, BExprPtr other, std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    auto self = ctx_.expressions->this_object(SourceRange{});
    if (!self) {
        return false;
    }
    const RecordInfo info = types.record_at(record);
    bool ok = true;
    for (const BaseInfo& base : info.bases) {
        if (base.is_virtual) {
            continue;
        }
        const RecordInfo& bi = types.record_at(base.record);
        auto target = base_of(*self, base);
        auto source = base_of(*other, base);
        if (bi.trivial_copy) {
            out.push_back(stmt(SourceRange{}, BCopy{std::move(target), std::move(source), bi.nv_size}));
        } else {
            std::vector<BExprPtr> args;
            args.push_back(std::move(source));
            ok = construct_base(std::move(target), base.record, std::move(args), SourceRange{}, out) && ok;
        }
    }
    for (const FieldInfo& field : info.fields) {
        auto target = field_of(*self, field);
        auto source = field_of(*other, field);
        if (types.is_record(field.type) && !types.record(field.type).trivial_copy) {
            std::vector<BExprPtr> args;
            args.push_back(std::move(source));
            ok = construct(std::move(target), field.type, std::move(args), SourceRange{}, out) && ok;
        } else if (types.is_array(field.type) && types.is_record(types.info(field.type).target) &&
                   !types.record(types.info(field.type).target).trivial_copy) {
            const TypeInfo array = types.info(field.type);
            for (std::uint32_t i = 0; i < array.count; ++i) {
                std::vector<BExprPtr> args;
                args.push_back(element(*source, i));
                ok = construct(element(*target, i), array.target, std::move(args), SourceRange{}, out) && ok;
            }
        } else {
            out.push_back(stmt(SourceRange{}, BCopy{std::move(target), std::move(source), types.cells(field.type)}));
        }
    }
    return ok;
}

bool InitializerBinder::assign_members(std::uint32_t record, BExprPtr other, std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    auto self = ctx_.expressions->this_object(SourceRange{});
    if (!self) {
        return false;
    }
    auto assign = [&](BExprPtr target, BExprPtr source, TypeRef type) {
        if (types.is_record(type) && !types.record(type).trivial_assign) {
            std::vector<BExprPtr> args;
            args.push_back(std::move(source));
            auto call = ctx_.expressions->call_method(std::move(target), "operator=", std::move(args), SourceRange{});
            if (!call) {
                return false;
            }
            out.push_back(stmt(SourceRange{}, BExprStmt{std::move(*call)}));
            return true;
        }
        out.push_back(stmt(SourceRange{}, BCopy{std::move(target), std::move(source), types.cells(type)}));
        return true;
    };
    const RecordInfo info = types.record_at(record);
    bool ok = true;
    for (const BaseInfo& base : info.bases) {
        if (!base.is_virtual) {
            ok = assign(base_of(*self, base), base_of(*other, base), base.type) && ok;
        }
    }
    for (const FieldInfo& field : info.fields) {
        ok = assign(field_of(*self, field), field_of(*other, field), field.type) && ok;
    }
    return ok;
}

void InitializerBinder::destroy_members(std::uint32_t record, std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    auto self = ctx_.expressions->this_object(SourceRange{});
    if (!self) {
        return;
    }
    const RecordInfo info = types.record_at(record);
    for (auto field = info.fields.rbegin(); field != info.fields.rend(); ++field) {
        if (needs_destruction(field->type)) {
            destroy(field_of(*self, *field), field->type, out);
        }
    }
    for (auto base = info.bases.rbegin(); base != info.bases.rend(); ++base) {
        if (!base->is_virtual && needs_destruction(base->type)) {
            destroy(base_of(*self, *base), base->type, out);
        }
    }
}

}  // namespace cppi::sema
