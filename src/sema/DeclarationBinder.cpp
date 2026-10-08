#include "sema/DeclarationBinder.hpp"

#include "sema/ExpressionBinder.hpp"
#include "sema/InitializerBinder.hpp"
#include "sema/Intrinsics.hpp"
#include "sema/LvalueCloner.hpp"
#include "sema/NameSuggester.hpp"
#include "sema/OverloadResolver.hpp"
#include "sema/StatementBinder.hpp"
#include "sema/TemplateEngine.hpp"
#include "support/DiagnosticFactory.hpp"
#include "support/Overloaded.hpp"

#include <algorithm>
#include <utility>
#include <variant>

namespace cppi::sema {

using detail::DiagnosticFactory;

namespace {

BStmt wrap(SourceRange range, auto node) {
    BStmt s;
    s.range = range;
    s.node = std::move(node);
    return s;
}

std::uint32_t align_to(std::uint32_t value, std::uint32_t alignment) {
    return alignment <= 1 ? value : (value + alignment - 1) / alignment * alignment;
}

}  // namespace

// =============================================================================
// Host
// =============================================================================

void DeclarationBinder::register_host() {
    TypeTable& types = ctx_.types;
    Scope& global = ctx_.symbols.global();
    for (std::uint16_t id = types::kFirstUserType;; ++id) {
        const auto* e = ctx_.host.find_enum(TypeId{id});
        if (e == nullptr) {
            break;
        }
        EnumInfo info;
        info.name = e->name;
        info.host_id = TypeId{id};
        for (std::size_t i = 0; i < e->enumerators.size(); ++i) {
            info.enumerators.emplace_back(e->enumerators[i], static_cast<std::int64_t>(i));
        }
        const TypeRef type = types.add_enum(std::move(info));
        types.map_host_enum(TypeId{id}, type);
        Symbol t;
        t.kind = SymbolKind::Type;
        t.type = type;
        global.add(e->name, t);
        for (std::size_t i = 0; i < e->enumerators.size(); ++i) {
            Symbol c;
            c.kind = SymbolKind::Constant;
            c.type = type;
            c.value = static_cast<std::int64_t>(i);
            global.add(e->enumerators[i], c);
        }
    }
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(Intrinsic::Count); ++i) {
        Symbol f;
        f.kind = SymbolKind::Functions;
        f.functions.push_back(Callee{false, i, true});
        global.add(std::string(Intrinsics::info(static_cast<Intrinsic>(i), types).name), f);
    }
    for (std::uint32_t i = 0; i < ctx_.host.function_count(); ++i) {
        Symbol f;
        f.kind = SymbolKind::Functions;
        f.functions.push_back(Callee{true, i});
        global.add(ctx_.host.function(FunctionId{i}).name, f);
    }
}

// =============================================================================
// Variables
// =============================================================================

void DeclarationBinder::bind_variables(const ast::DeclStmt& decl, SourceRange range, std::vector<BStmt>*& out) {
    for (const auto& var : decl.vars) {
        declare_variable(decl.type, var, range, out, nullptr);
    }
}

bool DeclarationBinder::bind_loop_variable(const ast::DeclStmt& decl, BExprPtr element, SourceRange range,
                                           std::vector<BStmt>*& out) {
    if (decl.vars.size() != 1) {
        return false;
    }
    return declare_variable(decl.type, decl.vars.front(), range, out, std::move(element));
}

bool DeclarationBinder::declare_variable(const ast::TypeSpec& spec, const ast::VarDeclarator& var, SourceRange range,
                                         std::vector<BStmt>*& out, BExprPtr loop_element) {
    TypeTable& types = ctx_.types;
    // `auto [x, y] = e;` declares a hidden variable holding e; x and y refer into it.
    const std::string hidden = var.declarator.bindings.empty()
                                   ? std::string()
                                   : "__binding@" + std::to_string(var.declarator.range.begin.line) + ":" +
                                         std::to_string(var.declarator.range.begin.column);
    const std::string& name = var.declarator.bindings.empty() ? var.declarator.name : hidden;
    // Temporaries of the initializer die at the end of the declaration.
    const std::size_t temps_mark = ctx_.fn->temporaries.size();
    if (!var.declarator.bindings.empty() && spec.name != "auto") {
        ctx_.report(
            DiagnosticFactory::declaration_not_allowed(var.declarator.range, "structured binding without auto"));
        return false;
    }
    if (!var.declarator.scope.empty()) {
        return define_static_member(spec, var, range, out);
    }
    const bool global = ctx_.symbols.at_global_scope() && ctx_.fn->function == 0;
    auto poison_name = [&]() {
        Symbol p;
        p.kind = SymbolKind::Poisoned;
        ctx_.symbols.current().add(name, p);
    };
    // Note: every early exit below reports why.

    if (spec.is_static && !global) {
        ctx_.report(DiagnosticFactory::declaration_not_allowed(range, "static local variable"));
        poison_name();
        return false;
    }
    auto resolved = ctx_.type_resolver->resolve(spec, var.declarator);
    if (!resolved) {
        poison_name();
        return false;
    }

    // `auto`: the initializer decides the type.
    BExprPtr bound_init = std::move(loop_element);
    if (resolved->is_auto) {
        if (!bound_init) {
            if (var.style == ast::InitStyle::None || (var.style == ast::InitStyle::Direct && var.args.size() != 1)) {
                ctx_.report(DiagnosticFactory::uninitialized_const(var.declarator.range, name));
                poison_name();
                return false;
            }
            const ast::Expr* init = var.style == ast::InitStyle::Direct ? &var.args.front() : var.init.get();
            if (const auto* list = std::get_if<ast::InitList>(&init->node)) {
                if (list->elements.size() != 1) {
                    ctx_.report(DiagnosticFactory::too_many_initializers(init->range, 1, list->elements.size()));
                    poison_name();
                    return false;
                }
                init = &list->elements.front();
            }
            bound_init = ctx_.expressions->bind(*init);
            if (!bound_init) {
                poison_name();
                return false;
            }
        }
        const auto& parts = var.declarator.parts;
        const bool auto_reference = parts.size() == 1 && parts[0].kind == ast::DeclaratorKind::Reference;
        const bool auto_pointer = parts.size() == 1 && parts[0].kind == ast::DeclaratorKind::Pointer;
        const bool keep_array = !var.declarator.bindings.empty() && types.is_array(bound_init->type);
        if (!auto_reference && !keep_array &&
            (types.is_array(bound_init->type) || !types.is_record(bound_init->type))) {
            bound_init = ctx_.expressions->to_rvalue(std::move(bound_init));
        }
        if (types.is_void(bound_init->type) || types.kind(bound_init->type) == TypeKind::Nullptr) {
            ctx_.report(DiagnosticFactory::cannot_convert(bound_init->range, types.name(bound_init->type), "auto"));
            poison_name();
            return false;
        }
        if (auto_reference) {
            // auto& binds to an lvalue of the initializer's own type.
            const bool target_const = resolved->is_const || bound_init->is_const;
            resolved->type = types.reference_to(bound_init->type, target_const);
            resolved->is_const = false;
        } else if (auto_pointer && !types.is_pointer(bound_init->type)) {
            ctx_.report(DiagnosticFactory::cannot_convert(bound_init->range, types.name(bound_init->type), "auto*"));
            poison_name();
            return false;
        } else {
            resolved->type = bound_init->type;
        }
        resolved->is_auto = false;
        if (keep_array) {
            // `auto [a, b] = arr;` copies the array.
            const TypeRef array = bound_init->type;
            const std::uint32_t cells = types.cells(array);
            std::uint32_t offset = 0;
            const bool at_top = ctx_.symbols.at_global_scope() && ctx_.fn->function == 0;
            if (at_top) {
                offset = ctx_.program.global_cells;
                ctx_.program.global_cells += cells;
            } else {
                offset = ctx_.allocate_local(cells);
            }
            Symbol whole;
            whole.kind = SymbolKind::Variable;
            whole.type = array;
            whole.global = at_top;
            whole.offset = offset;
            BVar slot;
            slot.offset = offset;
            slot.global = at_top;
            slot.cells = cells;
            out->push_back(
                wrap(range, BCopy{ExpressionBinder::make(array, true, range, slot), std::move(bound_init), cells}));
            return bind_structured(whole, var, range, *out);
        }
    }

    TypeRef type = resolved->type;
    if (types.is_void(type)) {
        ctx_.report(DiagnosticFactory::declaration_not_allowed(var.declarator.range, "variable of type void"));
        poison_name();
        return false;
    }
    // `int a[] = {1, 2, 3};` and `char s[] = "hi";`
    if (types.is_array(type) && types.info(type).count == 0) {
        std::uint32_t count = 0;
        const ast::Expr* init = var.init.get();
        if (init != nullptr) {
            if (const auto* list = std::get_if<ast::InitList>(&init->node)) {
                count = static_cast<std::uint32_t>(list->elements.size());
            } else if (const auto* str = std::get_if<ast::StringLiteral>(&init->node)) {
                count = static_cast<std::uint32_t>(str->value.size() + 1);
            }
        }
        if (count == 0) {
            ctx_.report(DiagnosticFactory::invalid_array_size(var.declarator.range, std::nullopt));
            poison_name();
            return false;
        }
        type = types.array_of(types.info(type).target, count);
    }
    if (types.is_record(type) && !types.record(type).complete) {
        ctx_.report(DiagnosticFactory::unknown_type(var.declarator.range, types.name(type), std::nullopt));
        poison_name();
        return false;
    }
    const bool reference = types.is_reference(type);
    if (!bound_init && var.style == ast::InitStyle::None &&
        (reference || (resolved->is_const && (types.is_scalar(type) || types.is_array(type))))) {
        ctx_.report(DiagnosticFactory::uninitialized_const(var.declarator.range, name));
        poison_name();
        return false;
    }

    // Copy elision: `T x = T(...)` or `T x = make()` builds the object right
    // in x's place; the temporary's slot becomes the variable.
    if (!bound_init && !reference && types.is_record(type) && var.style == ast::InitStyle::Copy && var.init &&
        !std::holds_alternative<ast::InitList>(var.init->node)) {
        bound_init = ctx_.expressions->bind(*var.init);
        if (!bound_init) {
            poison_name();
            return false;
        }
    }
    std::optional<std::uint32_t> adopted;
    std::vector<BStmt> adopted_init;
    bool built_in_place = false;  // a global built directly by the call that returns it
    if (bound_init && !global && !reference && types.is_record(type) && bound_init->type == type &&
        bound_init->expiring) {
        if (auto* temp = std::get_if<BTemp>(&bound_init->node); temp != nullptr && !temp->init) {
            adopted = temp->offset;
            adopted_init = std::move(temp->init_stmts);
        } else if (auto* call = std::get_if<BCall>(&bound_init->node); call != nullptr && call->result_temp) {
            adopted = call->result_temp;
            BStmt run;
            run.range = range;
            run.node = BExprStmt{std::move(*bound_init)};
            adopted_init.push_back(std::move(run));
        }
        if (adopted) {
            auto& temps = ctx_.fn->temporaries;
            for (auto it = temps.begin(); it != temps.end(); ++it) {
                if (it->offset == *adopted) {
                    temps.erase(it);  // destroyed with the variable instead
                    break;
                }
            }
        }
    }

    const std::uint32_t cells = reference ? 1 : std::max<std::uint32_t>(1, types.cells(type));
    std::uint32_t offset = 0;
    if (adopted) {
        offset = *adopted;
    } else if (global) {
        offset = ctx_.program.global_cells;
        ctx_.program.global_cells += cells;
    } else {
        offset = ctx_.allocate_local(cells);
    }
    if (!adopted && global && bound_init && types.is_record(type) && bound_init->type == type && bound_init->expiring) {
        if (auto* call = std::get_if<BCall>(&bound_init->node); call != nullptr && call->result_temp) {
            // The function writes its result straight into the global.
            auto& temps = ctx_.fn->temporaries;
            for (auto it = temps.begin(); it != temps.end(); ++it) {
                if (it->offset == *call->result_temp) {
                    temps.erase(it);
                    break;
                }
            }
            BVar target;
            target.offset = offset;
            target.global = true;
            target.cells = cells;
            call->result_temp.reset();
            call->result_target = ExpressionBinder::make(type, true, range, target);
            BStmt run;
            run.range = range;
            run.node = BExprStmt{std::move(*bound_init)};
            adopted_init.push_back(std::move(run));
            built_in_place = true;
        }
    }

    Symbol symbol;
    symbol.kind = SymbolKind::Variable;
    symbol.range = var.declarator.range;
    symbol.global = global;
    symbol.reference = reference;
    symbol.offset = offset;
    symbol.type = reference ? types.info(type).target : type;
    symbol.is_const = reference ? types.info(type).target_const : resolved->is_const;
    Symbol* declared = ctx_.symbols.current().add(name, symbol);
    if (declared == nullptr) {
        Symbol* existing = ctx_.symbols.current().find(name);
        if (existing == nullptr || existing->kind != SymbolKind::Poisoned) {
            ctx_.report(DiagnosticFactory::redefinition(var.declarator.range, name));
        }
        return false;
    }
    if (global) {
        ctx_.program.globals.push_back(GlobalDebug{name, offset, symbol.type});
    } else {
        ctx_.program.functions[ctx_.fn->function].locals.push_back(
            LocalDebug{name, offset, symbol.type, reference,
                       SourceRange{var.declarator.range.begin, ctx_.symbols.current().end()}});
    }

    // The slot itself (for references: the cell holding the address).
    BVar slot;
    slot.offset = offset;
    slot.global = global;
    slot.cells = cells;
    slot.name = name;
    auto target = ExpressionBinder::make(reference ? types.pointer_to(symbol.type, symbol.is_const) : type, true,
                                         var.declarator.range, slot);

    const std::size_t first = out->size();
    bool ok = true;
    if (adopted || built_in_place) {
        for (auto& stmt : adopted_init) {
            out->push_back(std::move(stmt));
        }
    } else if (reference) {
        if (bound_init) {
            const TypeInfo ref = types.info(type);
            if (!bound_init->lvalue || bound_init->type != ref.target || (bound_init->is_const && !ref.target_const)) {
                ctx_.report(DiagnosticFactory::reference_needs_lvalue(var.declarator.range, types.name(type)));
                ok = false;
            } else {
                auto address = ExpressionBinder::make(types.pointer_to(ref.target, ref.target_const), false, range,
                                                      BAddressOf{std::move(bound_init)});
                out->push_back(wrap(range, BStore{std::move(target), std::move(address)}));
            }
        } else {
            const ast::Expr* init = var.style == ast::InitStyle::Direct ? &var.args.front() : var.init.get();
            ok = init != nullptr && ctx_.initializers->bind_reference(std::move(target), type, *init, *out);
        }
    } else if (bound_init) {
        ok = ctx_.initializers->initialize_from(std::move(target), type, std::move(bound_init), *out);
    } else {
        ok = ctx_.initializers->initialize(std::move(target), type, var, !global, *out);
    }
    if (!ok) {
        declared->kind = SymbolKind::Poisoned;
        ctx_.statements->drop_temporaries(temps_mark);
        return false;
    }
    std::vector<BStmt> temps_cleanup = ctx_.statements->release_temporaries(temps_mark);
    if (reference && !temps_cleanup.empty()) {
        // A temporary bound to a reference lives as long as the reference.
        BBlock nested;
        nested.cleanup = std::move(temps_cleanup);
        out->push_back(wrap(range, std::move(nested)));
        out = &std::get<BBlock>(out->back().node).statements;
    } else {
        for (auto& cleanup : temps_cleanup) {
            out->push_back(std::move(cleanup));
        }
    }

    // A const scalar with a constant initializer is a constant expression.
    if (symbol.is_const && !reference && types.is_scalar(type) && out->size() == first + 1 &&
        std::holds_alternative<BStore>(out->back().node)) {
        if (const auto* store = std::get_if<BStore>(&out->back().node)) {
            if (auto value = ctx_.constants.integer(*store->value)) {
                if (types.kind(type) == TypeKind::Double) {
                    if (const auto* c = std::get_if<BConst>(&store->value->node)) {
                        declared->constant = c->bits;
                    }
                } else {
                    declared->constant = value;
                }
            }
        }
    }

    if (!reference && ctx_.initializers->needs_destruction(type)) {
        auto object = ExpressionBinder::make(type, true, range, slot);
        if (global) {
            std::vector<BStmt> cleanup;
            ctx_.initializers->destroy(std::move(object), type, cleanup);
            global_cleanup_.insert(global_cleanup_.begin(), std::make_move_iterator(cleanup.begin()),
                                   std::make_move_iterator(cleanup.end()));
        } else {
            BBlock nested;
            ctx_.initializers->destroy(std::move(object), type, nested.cleanup);
            out->push_back(wrap(range, std::move(nested)));
            out = &std::get<BBlock>(out->back().node).statements;
        }
    }
    if (!var.declarator.bindings.empty()) {
        return bind_structured(*declared, var, range, *out);
    }
    return true;
}

// =============================================================================
// Functions
// =============================================================================

bool DeclarationBinder::bind_structured(const Symbol& whole, const ast::VarDeclarator& var, SourceRange range,
                                        std::vector<BStmt>& out) {
    TypeTable& types = ctx_.types;
    const auto& names = var.declarator.bindings;
    auto object = [&]() { return ctx_.expressions->variable(whole, "", range); };
    std::vector<BExprPtr> parts;
    const TypeRef type = whole.type;
    if (types.is_array(type)) {
        const TypeInfo array = types.info(type);
        if (array.count != names.size()) {
            ctx_.report(DiagnosticFactory::too_many_initializers(var.declarator.range, array.count, names.size()));
            return false;
        }
        for (std::uint32_t i = 0; i < array.count; ++i) {
            BIndex idx;
            idx.base = object();
            idx.index = ExpressionBinder::constant(TypeTable::kLong, i, range);
            idx.elem_cells = types.cells(array.target);
            idx.bound = array.count;
            auto e = ExpressionBinder::make(array.target, true, range, std::move(idx));
            e->is_const = whole.is_const;
            parts.push_back(std::move(e));
        }
    } else if (types.is_record(type) && types.record(type).bases.empty()) {
        const RecordInfo& info = types.record(type);
        if (info.fields.size() != names.size()) {
            ctx_.report(
                DiagnosticFactory::too_many_initializers(var.declarator.range, info.fields.size(), names.size()));
            return false;
        }
        for (const FieldInfo& f : info.fields) {
            BMember m;
            m.base = object();
            m.offset = f.offset;
            m.cells = types.cells(f.type);
            auto e = ExpressionBinder::make(f.type, true, range, std::move(m));
            e->is_const = whole.is_const || f.is_const;
            parts.push_back(std::move(e));
        }
    } else {
        ctx_.report(DiagnosticFactory::cannot_convert(var.declarator.range, types.name(type), "[...]"));
        return false;
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        const TypeRef part_type = parts[i]->type;
        const bool is_const = parts[i]->is_const;
        Symbol s;
        s.kind = SymbolKind::Variable;
        s.range = var.declarator.range;
        s.type = part_type;
        s.is_const = is_const;
        s.reference = true;
        s.global = whole.global;
        if (s.global) {
            s.offset = ctx_.program.global_cells++;
        } else {
            s.offset = ctx_.allocate_local(1);
        }
        BVar slot;
        slot.offset = s.offset;
        slot.global = s.global;
        const TypeRef pointer = types.pointer_to(part_type, is_const);
        out.push_back(
            wrap(range, BStore{ExpressionBinder::make(pointer, true, range, slot),
                               ExpressionBinder::make(pointer, false, range, BAddressOf{std::move(parts[i])})}));
        if (ctx_.symbols.current().add(names[i], s) == nullptr) {
            ctx_.report(DiagnosticFactory::redefinition(var.declarator.range, names[i]));
            return false;
        }
        if (!s.global) {
            ctx_.program.functions[ctx_.fn->function].locals.push_back(
                LocalDebug{names[i], s.offset, part_type, true,
                           SourceRange{var.declarator.range.begin, ctx_.symbols.current().end()}});
        }
    }
    return true;
}

std::optional<TypeRef> DeclarationBinder::bind_lambda(const ast::LambdaExpr& lambda,
                                                      const std::vector<Capture>& captures, SourceRange range) {
    TypeTable& types = ctx_.types;
    RecordInfo info;
    info.name = "lambda at " + std::to_string(range.begin.line) + ":" + std::to_string(range.begin.column);
    info.range = range;
    info.home = &ctx_.symbols.enclosing_namespace();
    const TypeRef type = types.add_record(std::move(info));
    const std::uint32_t record = types.info(type).decl;
    for (const Capture& c : captures) {
        FieldInfo f;
        f.name = c.name;
        f.type = c.by_reference ? types.pointer_to(c.type, c.is_const) : c.type;
        f.deref = c.by_reference;
        f.range = range;
        types.record_at(record).fields.push_back(std::move(f));
    }

    const ast::FunctionDecl& decl = *lambda.function;
    const auto autos = std::count_if(decl.params.begin(), decl.params.end(),
                                     [](const ast::Param& p) { return p.type.name == "auto"; });
    if (autos > 0) {
        // A generic lambda: its call operator is a member function template.
        layout_record(record);
        types.record_at(record).complete = true;
        build_dispatch(record);
        std::vector<std::pair<std::uint32_t, const ast::FunctionDecl*>> bodies;
        ast::RecordDef none;
        none.name = types.record_at(record).name;
        none.name_range = range;
        synthesize_copies(record, none, bodies);
        std::vector<Scope*> chain;
        for (Scope* scope : ctx_.symbols.snapshot()) {
            if (scope->kind() == Scope::Kind::Global || scope->kind() == Scope::Kind::Namespace) {
                chain.push_back(scope);
            }
        }
        for (const auto& [body_id, body_decl] : bodies) {
            define_body(body_id, body_decl);
        }
        ctx_.templates->declare_generic_lambda(record, decl, static_cast<std::size_t>(autos), std::move(chain),
                                               !lambda.is_mutable, !lambda.has_return_type);
        return type;
    }
    auto sig = resolve_signature(decl, false, !lambda.has_return_type);
    if (!sig) {
        return std::nullopt;
    }
    FunctionInfo fn;
    fn.name = "operator()";
    fn.display = types.record_at(record).name;
    fn.return_type = sig->return_type;
    fn.params = std::move(sig->params);
    fn.range = range;
    fn.decl = &decl;
    fn.record = record;
    fn.is_const = !lambda.is_mutable;
    const std::uint32_t id = add_function(std::move(fn));
    types.record_at(record).methods["operator()"].push_back(id);
    layout_record(record);
    types.record_at(record).complete = true;
    build_dispatch(record);
    std::vector<std::pair<std::uint32_t, const ast::FunctionDecl*>> bodies;
    ast::RecordDef none;
    none.name = types.record_at(record).name;
    none.name_range = range;
    synthesize_copies(record, none, bodies);

    // The body sees the namespaces around it and its captures (as members),
    // not the enclosing function's other variables.
    std::vector<Scope*> chain;
    for (Scope* scope : ctx_.symbols.snapshot()) {
        if (scope->kind() == Scope::Kind::Global || scope->kind() == Scope::Kind::Namespace) {
            chain.push_back(scope);
        }
    }
    auto previous = ctx_.symbols.isolate(std::move(chain));
    deduce_next_ = !lambda.has_return_type;
    define_body(id, &decl);
    for (const auto& [body_id, body_decl] : bodies) {
        define_body(body_id, body_decl);
    }
    ctx_.symbols.restore(std::move(previous));
    return type;
}

std::uint32_t DeclarationBinder::add_function(FunctionInfo info) {
    const auto id = static_cast<std::uint32_t>(ctx_.functions.size());
    info.library = ctx_.library_depth > 0;
    BoundFunction bound;
    bound.name = info.display;
    bound.range = info.range;
    bound.record = info.record;
    bound.library = info.library;
    ctx_.functions.push_back(std::move(info));
    ctx_.program.functions.push_back(std::move(bound));
    return id;
}

bool DeclarationBinder::same_params(const std::vector<ParamInfo>& a, const std::vector<ParamInfo>& b) const {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].type != b[i].type) {
            return false;
        }
    }
    return true;
}

std::optional<DeclarationBinder::Signature> DeclarationBinder::resolve_signature(const ast::FunctionDecl& decl,
                                                                                 bool constructor_like,
                                                                                 bool deduced_return) {
    const TypeTable& types = ctx_.types;
    Signature sig;
    // `auto f() { return 1; }` (C++14): the body decides.
    deduced_return = deduced_return || (decl.return_type.name == "auto" && decl.declarator.parts.empty());
    if (constructor_like || deduced_return) {
        sig.return_type = TypeTable::kVoid;  // deduced returns are decided by the body
    } else {
        ast::Declarator ret_declarator;
        for (const auto& part : decl.declarator.parts) {
            ast::DeclaratorPart copy;
            copy.kind = part.kind;
            copy.is_const = part.is_const;
            copy.range = part.range;
            ret_declarator.parts.push_back(std::move(copy));
        }
        auto ret = ctx_.type_resolver->resolve(decl.return_type, ret_declarator);
        if (!ret) {
            return std::nullopt;
        }
        if (ret->is_auto) {
            ctx_.report(DiagnosticFactory::unsupported_type(decl.return_type.range, "auto"));
            return std::nullopt;
        }
        if (types.is_array(ret->type)) {
            ctx_.report(
                DiagnosticFactory::declaration_not_allowed(decl.declarator.range, "function returning an array"));
            return std::nullopt;
        }
        sig.return_type = ret->type;
    }
    bool ok = true;
    bool seen_default = false;
    std::size_t next_auto = 0;
    for (const auto& p : decl.params) {
        std::optional<ResolvedType> resolved;
        if (generic_autos_ != nullptr && p.type.name == "auto" && next_auto < generic_autos_->size()) {
            // A generic lambda's `auto x`, `const auto& x`: the deduced type.
            ResolvedType base;
            base.type = (*generic_autos_)[next_auto++];
            base.is_const = p.type.is_const;
            resolved = ctx_.type_resolver->apply(base, p.declarator);
        } else {
            resolved = ctx_.type_resolver->resolve(p.type, p.declarator);
        }
        if (!resolved) {
            ok = false;
            continue;
        }
        if (resolved->is_auto || types.is_void(resolved->type)) {
            ctx_.report(DiagnosticFactory::unsupported_type(p.range, resolved->is_auto ? "auto" : "void"));
            ok = false;
            continue;
        }
        ParamInfo info;
        info.name = p.declarator.name;
        info.type = ctx_.type_resolver->adjust_parameter(*resolved);
        info.default_value = p.default_value.get();
        info.range = p.range;
        if (info.default_value != nullptr) {
            seen_default = true;
        } else if (seen_default) {
            ctx_.report(DiagnosticFactory::declaration_not_allowed(
                p.range, "parameter without a default after one with a default"));
            ok = false;
        }
        sig.params.push_back(std::move(info));
    }
    if (!ok) {
        return std::nullopt;
    }
    return sig;
}

void DeclarationBinder::bind_function(const ast::FunctionDecl& decl, SourceRange range) {
    if (!decl.scope.empty()) {
        define_out_of_class(decl, range);
        return;
    }
    const std::string& name = decl.declarator.name;
    if (decl.is_virtual || decl.is_const || decl.is_override || decl.is_pure || decl.is_constructor) {
        ctx_.report(DiagnosticFactory::declaration_not_allowed(
            decl.declarator.range,
            decl.is_constructor ? "function without a return type" : "member specifier outside a class"));
        return;
    }
    auto sig = resolve_signature(decl, false);
    Scope& global = ctx_.symbols.current();
    Symbol* existing = global.find(name);
    if (!sig) {
        if (existing == nullptr) {
            Symbol p;
            p.kind = SymbolKind::Poisoned;
            global.add(name, p);
        }
        return;
    }
    if (existing != nullptr && existing->kind != SymbolKind::Functions) {
        if (existing->kind != SymbolKind::Poisoned) {
            ctx_.report(DiagnosticFactory::redefinition(decl.declarator.range, name));
        }
        return;
    }
    std::optional<std::uint32_t> id;
    if (existing != nullptr) {
        for (const Callee& c : existing->functions) {
            if (c.host) {
                if (same_params(sig->params, [&] {
                        std::vector<ParamInfo> host_params;
                        for (const TypeRef t : ctx_.overloads->param_types(c)) {
                            ParamInfo p;
                            p.type = t;
                            host_params.push_back(p);
                        }
                        return host_params;
                    }())) {
                    ctx_.report(DiagnosticFactory::redefinition(decl.declarator.range, name));
                    return;
                }
                continue;
            }
            FunctionInfo& other = ctx_.functions[c.id];
            if (!same_params(other.params, sig->params)) {
                continue;
            }
            if (other.return_type != sig->return_type || (other.defined && decl.body)) {
                ctx_.report(DiagnosticFactory::redefinition(decl.declarator.range, name));
                return;
            }
            id = c.id;
            // A definition may name parameters the prototype left unnamed;
            // default arguments come from the first declaration.
            for (std::size_t i = 0; i < other.params.size(); ++i) {
                other.params[i].name = sig->params[i].name;
                if (other.params[i].default_value == nullptr) {
                    other.params[i].default_value = sig->params[i].default_value;
                }
            }
        }
    }
    if (!id) {
        FunctionInfo info;
        info.name = name;
        info.display = name;
        info.return_type = sig->return_type;
        info.params = std::move(sig->params);
        info.range = decl.declarator.range;
        info.decl = &decl;
        info.is_constexpr = decl.return_type.is_constexpr;
        id = add_function(std::move(info));
        if (existing == nullptr) {
            Symbol f;
            f.kind = SymbolKind::Functions;
            f.range = decl.declarator.range;
            global.add(name, f);
            existing = global.find(name);
        }
        existing->functions.push_back(Callee{false, *id});
    }
    if (name == "main") {
        if (!ctx_.functions[*id].params.empty()) {
            ctx_.report(DiagnosticFactory::declaration_not_allowed(decl.declarator.range, "main with parameters"));
            return;
        }
        main_ = id;
    }
    if (decl.body) {
        define_body(*id, &decl);
    }
}

std::optional<std::uint32_t> DeclarationBinder::instantiate_function(
    const ast::FunctionDecl& decl, std::string display, const std::function<void(std::uint32_t)>& registered) {
    auto sig = resolve_signature(decl, false);
    if (!sig) {
        return std::nullopt;
    }
    FunctionInfo info;
    info.name = decl.declarator.name;
    info.display = std::move(display);
    info.return_type = sig->return_type;
    info.params = std::move(sig->params);
    info.range = decl.declarator.range;
    info.decl = &decl;
    info.is_constexpr = decl.return_type.is_constexpr;
    const std::uint32_t id = add_function(std::move(info));
    registered(id);  // before the body: recursive calls find this instance
    if (decl.body) {
        define_body(id, &decl);
    }
    return id;
}

std::optional<std::uint32_t> DeclarationBinder::instantiate_method(std::uint32_t record, const ast::FunctionDecl& decl,
                                                                   std::string display,
                                                                   const std::function<void(std::uint32_t)>& registered,
                                                                   const GenericLambda* lambda) {
    generic_autos_ = lambda != nullptr ? lambda->autos : nullptr;
    auto sig = resolve_signature(decl, false, lambda != nullptr && lambda->deduce_return);
    generic_autos_ = nullptr;
    if (!sig) {
        return std::nullopt;
    }
    FunctionInfo info;
    info.name = decl.declarator.name;
    info.display = std::move(display);
    info.return_type = sig->return_type;
    info.params = std::move(sig->params);
    info.range = decl.declarator.range;
    info.decl = &decl;
    info.record = record;
    info.is_const = lambda != nullptr ? lambda->is_const : decl.is_const;
    info.is_static = decl.is_static;
    info.access = decl.access;
    info.is_constexpr = decl.return_type.is_constexpr;
    const std::uint32_t id = add_function(std::move(info));
    registered(id);  // before the body: recursive calls find this instance
    if (decl.body) {
        deduce_next_ = lambda != nullptr && lambda->deduce_return;
        define_body(id, &decl);
    }
    return id;
}

void DeclarationBinder::define_out_of_class(const ast::FunctionDecl& decl, SourceRange range) {
    TypeTable& types = ctx_.types;
    Symbol* scope = nullptr;
    if (decl.scope.size() == 1) {
        scope = ctx_.symbols.lookup(decl.scope.front());
    } else {
        const std::vector<std::string> outer(decl.scope.begin(), decl.scope.end() - 1);
        scope = ctx_.symbols.lookup_qualified(outer, decl.scope.back());
    }
    if (scope == nullptr || scope->kind != SymbolKind::Type || !types.is_record(scope->type)) {
        ctx_.report(DiagnosticFactory::unknown_type(range, decl.scope.front(), std::nullopt));
        return;
    }
    const std::uint32_t record = types.info(scope->type).decl;
    RecordInfo& info = types.record_at(record);
    const std::string& name = decl.declarator.name;
    const bool ctor = name == info.name;
    const bool dtor = name == "~" + info.name;
    auto sig = resolve_signature(decl, ctor || dtor);
    if (!sig) {
        return;
    }
    std::vector<std::uint32_t> candidates;
    if (ctor) {
        candidates = info.constructors;
    } else if (dtor) {
        if (info.destructor) {
            candidates.push_back(*info.destructor);
        }
    } else if (auto it = info.methods.find(name); it != info.methods.end()) {
        candidates = it->second;
    }
    for (const std::uint32_t id : candidates) {
        FunctionInfo& fn = ctx_.functions[id];
        if (!same_params(fn.params, sig->params) || fn.is_const != decl.is_const) {
            continue;
        }
        if (fn.defined) {
            ctx_.report(DiagnosticFactory::redefinition(decl.declarator.range, fn.display));
            return;
        }
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            fn.params[i].name = sig->params[i].name;
        }
        if (decl.body) {
            define_body(id, &decl);
        }
        return;
    }
    ctx_.report(DiagnosticFactory::no_member(decl.declarator.range, info.name, name,
                                             NameSuggester::closest(name, ctx_.expressions->members().names(record))));
}

void DeclarationBinder::define_body(std::uint32_t id, const ast::FunctionDecl* decl) {
    const TypeTable& types = ctx_.types;
    FunctionInfo& info = ctx_.functions[id];
    info.defined = true;
    if (decl != nullptr) {
        info.decl = decl;
    }

    FunctionContext fc;
    fc.function = id;
    fc.return_type = info.return_type;
    fc.record = info.record;
    fc.const_this = info.is_const;
    fc.has_this = !info.is_static;
    fc.deduce_return =
        deduce_next_ || (decl != nullptr && decl->return_type.name == "auto" && decl->declarator.parts.empty());
    deduce_next_ = false;
    FunctionContext* previous = ctx_.fn;
    ctx_.fn = &fc;
    ctx_.symbols.push(Scope::Kind::Function, decl != nullptr ? decl->range.end : info.range.end);

    std::vector<ParamSlot> params;
    // `this`, the result slot and the declared parameters. Reserving up front
    // also avoids a GCC 13 -Wstringop-overflow false positive in Release.
    params.reserve(info.params.size() + 2);
    if (info.record && !info.is_static) {
        params.push_back(ParamSlot{fc.frame.allocate(1), 1, false});  // this
    }
    if (types.is_record(info.return_type)) {
        fc.result_slot = fc.frame.allocate(1);
        params.push_back(ParamSlot{*fc.result_slot, 1, false});
    }
    const auto param_infos = info.params;  // copy: binding may add functions
    for (const ParamInfo& p : param_infos) {
        // Records passed by value arrive as a pointer to a copy the caller
        // made (and destroys): the parameter refers to that copy.
        const bool by_value_record = types.is_record(p.type);
        const bool reference = types.is_reference(p.type) || by_value_record;
        const TypeRef object = types.is_reference(p.type) ? types.info(p.type).target : p.type;
        const std::uint32_t cells = reference ? 1 : std::max<std::uint32_t>(1, types.cells(p.type));
        const std::uint32_t offset = fc.frame.allocate(cells);
        params.push_back(ParamSlot{offset, cells, false});
        if (p.name.empty()) {
            continue;
        }
        Symbol s;
        s.kind = SymbolKind::Variable;
        s.range = p.range;
        s.type = object;
        s.reference = reference;
        s.is_const = types.is_reference(p.type) ? types.info(p.type).target_const : false;
        s.offset = offset;
        if (ctx_.symbols.current().add(p.name, s) == nullptr) {
            ctx_.report(DiagnosticFactory::redefinition(p.range, p.name));
        }
        ctx_.program.functions[id].locals.push_back(
            LocalDebug{p.name, offset, object, reference, SourceRange{p.range.begin, ctx_.symbols.current().end()}});
    }

    BBlock body;
    if (info.implicit == FunctionInfo::Implicit::DefaultConstructor && info.record) {
        const TypeRef type = types.record_type(*info.record);
        if (auto self = ctx_.expressions->this_object(SourceRange{})) {
            ctx_.initializers->default_initialize(std::move(self), type, false, SourceRange{}, body.statements);
        }
        for (auto& cleanup : ctx_.statements->release_temporaries(0)) {
            body.statements.push_back(std::move(cleanup));
        }
    } else if (info.implicit != FunctionInfo::Implicit::None && info.record) {
        Symbol* other_symbol = ctx_.symbols.current().find("__other");
        auto other = ctx_.expressions->variable(*other_symbol, "__other", info.range);
        if (info.implicit == FunctionInfo::Implicit::CopyConstructor) {
            ctx_.initializers->copy_members(*info.record, std::move(other), body.statements);
        } else {
            ctx_.initializers->assign_members(*info.record, std::move(other), body.statements);
            BReturn r;
            auto self = ctx_.expressions->this_pointer(info.range);
            r.value = std::move(*self);
            body.statements.push_back(wrap(info.range, std::move(r)));
        }
    } else if (info.is_constructor && info.record) {
        ctx_.initializers->construct_members(*info.record, decl, body.statements);
        for (auto& cleanup : ctx_.statements->release_temporaries(0)) {
            body.statements.push_back(std::move(cleanup));
        }
    }
    if (decl != nullptr && decl->body) {
        if (const auto* block = std::get_if<ast::Block>(&decl->body->node)) {
            ctx_.statements->bind_statements(block->statements, body.statements);
        }
    }
    if (info.is_destructor && info.record) {
        ctx_.initializers->destroy_members(*info.record, body.cleanup);
    }

    if (!types.is_void(ctx_.functions[id].return_type)) {
        bool returns = false;
        for (const auto& s : body.statements) {
            returns = returns || StatementBinder::always_returns(s);
        }
        if (!returns) {
            if (info.name == "main" && !info.record) {
                BReturn r;
                r.value = std::move(*ExpressionBinder::constant(info.return_type, 0, info.range));
                body.statements.push_back(wrap(info.range, std::move(r)));
            } else {
                ctx_.report(DiagnosticFactory::missing_return(info.range, info.display));
            }
        }
    }

    ctx_.symbols.pop();
    ctx_.fn = previous;

    BoundFunction& bound = ctx_.program.functions[id];
    bound.params = std::move(params);
    bound.frame_cells = fc.frame.size();
    bound.returns_value = !types.is_void(info.return_type) && !types.is_record(info.return_type);
    bound.defined = true;
    bound.body = std::move(body);
}

// =============================================================================
// Static data members
// =============================================================================

template <class F>
auto DeclarationBinder::in_class_scope(std::uint32_t record, F body) {
    FunctionContext& fc = *ctx_.fn;
    const auto saved_record = fc.record;
    const bool saved_this = fc.has_this;
    fc.record = record;
    fc.has_this = false;
    ctx_.symbols.enter_scope(ctx_.types.record_at(record).statics);
    auto result = body();
    ctx_.symbols.leave_namespace();
    fc.record = saved_record;
    fc.has_this = saved_this;
    return result;
}

bool DeclarationBinder::declare_static_member(std::uint32_t record, const ast::FieldDecl& field,
                                              const ast::VarDeclarator& var, std::vector<BStmt>* out) {
    TypeTable& types = ctx_.types;
    const SourceRange range = var.declarator.range;
    if (out == nullptr || ctx_.fn == nullptr || ctx_.fn->function != 0 || !ctx_.symbols.at_global_scope()) {
        ctx_.report(DiagnosticFactory::declaration_not_allowed(
            range, out == nullptr ? "static data member of a class template" : "static data member of a local class"));
        return false;
    }
    const std::string display = types.record_at(record).name + "::" + var.declarator.name;
    const bool has_init = var.style != ast::InitStyle::None;
    // Only constants and inline variables may be initialized inside the class.
    if (has_init && !field.type.is_inline && !field.type.is_constexpr && !field.type.is_const) {
        ctx_.report(
            DiagnosticFactory::declaration_not_allowed(range, "in-class initializer of a non-const static member"));
        return false;
    }
    auto resolved = ctx_.type_resolver->resolve(field.type, var.declarator);
    if (!resolved) {
        return false;
    }
    // Without an initializer here, the definition outside the class gives
    // the value; objects of class type are built by it (`Clock Game::clock(60);`).
    const bool needs_construction = !has_init && types.is_record(resolved->type);
    Symbol* declared = nullptr;
    if (!has_init) {
        const TypeRef type = resolved->type;
        if (resolved->is_auto || types.is_void(type) || types.is_reference(type) ||
            (types.is_array(type) && types.info(type).count == 0)) {
            ctx_.report(DiagnosticFactory::declaration_not_allowed(range, "this kind of static data member"));
            return false;
        }
        if (types.is_record(type) && !types.record(type).complete) {
            ctx_.report(DiagnosticFactory::unknown_type(range, types.name(type), std::nullopt));
            return false;
        }
        Symbol symbol;
        symbol.kind = SymbolKind::Variable;
        symbol.range = range;
        symbol.global = true;
        symbol.type = type;
        symbol.is_const = resolved->is_const;
        symbol.offset = ctx_.program.global_cells;
        ctx_.program.global_cells += std::max<std::uint32_t>(1, types.cells(type));
        declared = types.record_at(record).statics->add(var.declarator.name, symbol);
        if (declared == nullptr) {
            ctx_.report(DiagnosticFactory::redefinition(range, display));
            return false;
        }
        ctx_.program.globals.push_back(GlobalDebug{display, symbol.offset, type});
        if (!needs_construction) {  // static storage starts zeroed
            BVar slot;
            slot.offset = symbol.offset;
            slot.global = true;
            slot.cells = std::max<std::uint32_t>(1, types.cells(type));
            out->push_back(wrap(range, BZero{ExpressionBinder::make(type, true, range, slot), slot.cells}));
        }
    } else {
        const std::size_t globals_before = ctx_.program.globals.size();
        const bool ok =
            in_class_scope(record, [&]() { return declare_variable(field.type, var, range, out, nullptr); });
        if (!ok) {
            return false;
        }
        declared = types.record_at(record).statics->find(var.declarator.name);
        if (ctx_.program.globals.size() > globals_before) {
            ctx_.program.globals[globals_before].name = display;
        }
    }
    declared->member_of = record;
    declared->member_access = static_cast<std::uint8_t>(field.access);
    statics_[declared] = StaticMember{record, display, range, has_init, needs_construction};
    return true;
}

bool DeclarationBinder::define_static_member(const ast::TypeSpec& spec, const ast::VarDeclarator& var,
                                             SourceRange range, std::vector<BStmt>*& out) {
    const TypeTable& types = ctx_.types;
    const auto& name = var.declarator.name;
    std::string qualified;
    for (const auto& part : var.declarator.scope) {
        qualified += part + "::";
    }
    qualified += name;
    Symbol* member = ctx_.symbols.lookup_qualified(var.declarator.scope, name);
    const auto it = member != nullptr ? statics_.find(member) : statics_.end();
    if (it == statics_.end()) {
        ctx_.report(DiagnosticFactory::unknown_identifier(var.declarator.range, qualified, std::nullopt));
        return false;
    }
    StaticMember& info = it->second;
    if (ctx_.fn == nullptr || ctx_.fn->function != 0 || !ctx_.symbols.at_global_scope()) {
        ctx_.report(DiagnosticFactory::declaration_not_allowed(range, "static member definition inside a function"));
        return false;
    }
    if (info.defined) {
        ctx_.report(DiagnosticFactory::redefinition(var.declarator.range, info.display));
        return false;
    }
    auto resolved = ctx_.type_resolver->resolve(spec, var.declarator);
    if (!resolved) {
        return false;
    }
    if (resolved->type != member->type || resolved->is_const != member->is_const) {
        ctx_.report(DiagnosticFactory::cannot_convert(var.declarator.range, types.name(resolved->type),
                                                      types.name(member->type)));
        return false;
    }
    info.defined = true;
    const TypeRef type = member->type;
    BVar slot;
    slot.offset = member->offset;
    slot.global = true;
    slot.cells = std::max<std::uint32_t>(1, types.cells(type));
    slot.name = info.display;
    const std::size_t temps_mark = ctx_.fn->temporaries.size();
    const std::size_t first = out->size();
    const bool ok = in_class_scope(info.record, [&]() {
        auto target = ExpressionBinder::make(type, true, var.declarator.range, slot);
        if (var.style == ast::InitStyle::None && !info.needs_construction) {
            return true;  // already zero-initialized, like every static object
        }
        return ctx_.initializers->initialize(std::move(target), type, var, false, *out);
    });
    if (!ok) {
        ctx_.statements->drop_temporaries(temps_mark);
        return false;
    }
    for (auto& cleanup : ctx_.statements->release_temporaries(temps_mark)) {
        out->push_back(std::move(cleanup));
    }
    // `const int Limits::max = 10;` is a constant expression afterwards.
    if (member->is_const && types.is_scalar(type) && types.kind(type) != TypeKind::Double && out->size() == first + 1) {
        if (const auto* store = std::get_if<BStore>(&out->back().node)) {
            member->constant = ctx_.constants.integer(*store->value);
        }
    }
    if (info.needs_construction && ctx_.initializers->needs_destruction(type)) {
        std::vector<BStmt> cleanup;
        ctx_.initializers->destroy(ExpressionBinder::make(type, true, range, slot), type, cleanup);
        global_cleanup_.insert(global_cleanup_.begin(), std::make_move_iterator(cleanup.begin()),
                               std::make_move_iterator(cleanup.end()));
    }
    return true;
}

// =============================================================================
// Records
// =============================================================================

bool DeclarationBinder::overrides(std::uint32_t candidate, std::uint32_t function) const {
    const FunctionInfo& a = ctx_.functions[candidate];
    const FunctionInfo& b = ctx_.functions[function];
    if (a.is_destructor && b.is_destructor) {
        return true;
    }
    return a.name == b.name && !a.is_constructor && !b.is_constructor && same_params(a.params, b.params);
}

void DeclarationBinder::bind_record(const ast::RecordDef& def, SourceRange /*range*/, std::vector<BStmt>* out) {
    TypeTable& types = ctx_.types;
    Scope& scope = ctx_.symbols.current();
    Symbol* existing = scope.find(def.name);
    TypeRef type = 0;
    if (existing != nullptr) {
        if (existing->kind != SymbolKind::Type || !types.is_record(existing->type) ||
            (types.record(existing->type).complete && def.has_body)) {
            if (existing->kind != SymbolKind::Poisoned) {
                ctx_.report(DiagnosticFactory::redefinition(def.name_range, def.name));
            }
            return;
        }
        type = existing->type;
        if (!def.has_body) {
            return;  // repeated forward declaration
        }
    } else {
        RecordInfo info;
        info.name = def.name;
        info.is_class = def.is_class;
        info.range = def.name_range;
        type = types.add_record(std::move(info));
        Symbol t;
        t.kind = SymbolKind::Type;
        t.type = type;
        t.range = def.name_range;
        scope.add(def.name, t);
        if (!def.has_body) {
            return;
        }
    }
    const std::uint32_t record = types.info(type).decl;
    types.record_at(record).is_final = def.is_final;
    types.record_at(record).home = &ctx_.symbols.enclosing_namespace();
    Scope* statics = ctx_.symbols.new_class_scope();
    types.record_at(record).statics = statics;
    if (Symbol* self = scope.find(def.name); self != nullptr && self->kind == SymbolKind::Type) {
        self->scope = statics;  // `Counter::count`
    }
    bool ok = true;

    // Bases.
    for (const auto& base : def.bases) {
        TypeRef base_type = 0;
        if (base.type.scope.empty() && base.type.template_args.empty()) {
            Symbol* b = ctx_.symbols.lookup(base.name);
            if (b == nullptr || b->kind != SymbolKind::Type || !types.is_record(b->type)) {
                if (b == nullptr || b->kind != SymbolKind::Poisoned) {
                    ctx_.report(DiagnosticFactory::unknown_type(base.range, base.name, std::nullopt));
                }
                ok = false;
                continue;
            }
            base_type = b->type;
        } else {
            auto resolved = ctx_.type_resolver->resolve_spec(base.type);  // `std::exception`, `Box<int>`
            if (!resolved || !types.is_record(resolved->type)) {
                if (resolved) {
                    ctx_.report(DiagnosticFactory::unknown_type(base.range, base.name, std::nullopt));
                }
                ok = false;
                continue;
            }
            base_type = resolved->type;
        }
        const RecordInfo& base_info = types.record(base_type);
        if (!base_info.complete || base_type == type) {
            ctx_.report(DiagnosticFactory::unknown_type(base.range, base.name, std::nullopt));
            ok = false;
            continue;
        }
        if (base_info.is_final) {
            ctx_.report(DiagnosticFactory::declaration_not_allowed(base.range, "inheriting from a final class"));
            ok = false;
            continue;
        }
        BaseInfo info;
        info.record = types.info(base_type).decl;
        info.type = base_type;
        info.access = base.access;
        info.is_virtual = base.is_virtual;
        info.range = base.range;
        types.record_at(record).bases.push_back(info);
    }

    // Fields (static constants are visible in the class: `int cells[size];`).
    ctx_.symbols.enter_scope(statics);
    for (const auto& field : def.fields) {
        if (field.type.is_static) {
            for (const auto& var : field.vars) {
                ok = declare_static_member(record, field, var, out) && ok;
            }
            continue;
        }
        for (const auto& var : field.vars) {
            auto resolved = ctx_.type_resolver->resolve(field.type, var.declarator);
            if (!resolved) {
                ok = false;
                continue;
            }
            const TypeRef ft = resolved->type;
            if (resolved->is_auto || types.is_void(ft) || types.is_reference(ft) ||
                (types.is_array(ft) && types.info(ft).count == 0)) {
                ctx_.report(
                    DiagnosticFactory::declaration_not_allowed(var.declarator.range, "this kind of data member"));
                ok = false;
                continue;
            }
            if (types.is_record(ft) && !types.record(ft).complete) {
                ctx_.report(DiagnosticFactory::unknown_type(var.declarator.range, types.name(ft), std::nullopt));
                ok = false;
                continue;
            }
            RecordInfo& info = types.record_at(record);
            const bool duplicate = std::any_of(info.fields.begin(), info.fields.end(),
                                               [&](const FieldInfo& f) { return f.name == var.declarator.name; });
            if (duplicate) {
                ctx_.report(DiagnosticFactory::redefinition(var.declarator.range, var.declarator.name));
                ok = false;
                continue;
            }
            FieldInfo f;
            f.name = var.declarator.name;
            f.type = ft;
            f.is_const = resolved->is_const;
            f.access = field.access;
            f.default_init = var.style == ast::InitStyle::None ? nullptr : &var;
            f.range = var.declarator.range;
            info.fields.push_back(std::move(f));
        }
    }

    // Member functions (declarations; bodies are bound once the class is complete).
    std::vector<std::pair<std::uint32_t, const ast::FunctionDecl*>> bodies;
    for (const auto& m : def.methods) {
        const std::string& name = m.declarator.name;
        const bool ctor = name == def.name;
        const bool dtor = name == "~" + def.name;
        if ((m.is_constructor && !ctor) || (m.is_destructor && !dtor)) {
            ctx_.report(
                DiagnosticFactory::declaration_not_allowed(m.declarator.range, "function without a return type"));
            ok = false;
            continue;
        }
        if (m.is_static && (ctor || dtor || m.is_virtual || m.is_pure || m.is_override || m.is_const)) {
            ctx_.report(DiagnosticFactory::declaration_not_allowed(
                m.declarator.range, "static constructor, destructor or virtual function"));
            ok = false;
            continue;
        }
        if (ctor && m.is_virtual) {
            ctx_.report(DiagnosticFactory::declaration_not_allowed(m.declarator.range, "virtual constructor"));
            ok = false;
            continue;
        }
        auto sig = resolve_signature(m, ctor || dtor);
        if (!sig) {
            ok = false;
            continue;
        }
        FunctionInfo info;
        info.name = name;
        info.display = types.record_at(record).name + "::" + name;
        info.return_type = sig->return_type;
        info.params = std::move(sig->params);
        info.range = m.declarator.range;
        info.decl = &m;
        info.record = record;
        info.is_const = m.is_const;
        info.is_virtual = m.is_virtual || m.is_pure || m.is_override || m.is_final;
        info.is_pure = m.is_pure;
        info.is_constructor = ctor;
        info.is_destructor = dtor;
        info.access = m.access;
        info.deleted = m.is_deleted;
        info.is_explicit = m.is_explicit;
        info.is_static = m.is_static;
        info.is_constexpr = m.return_type.is_constexpr;
        if (m.is_defaulted) {
            // `= default`: the compiler's version of a copy constructor or
            // assignment; other members get an empty body.
            const bool copy_like = info.params.size() == 1 && types.is_reference(info.params[0].type);
            if (ctor && copy_like) {
                info.implicit = FunctionInfo::Implicit::CopyConstructor;
                info.params[0].name = "__other";
            } else if (name == "operator=" && copy_like) {
                info.implicit = FunctionInfo::Implicit::CopyAssignment;
                info.params[0].name = "__other";
            }
        }

        RecordInfo& rec = types.record_at(record);
        bool duplicate = false;
        auto check = [&](const std::vector<std::uint32_t>& ids) {
            for (const std::uint32_t other : ids) {
                if (same_params(ctx_.functions[other].params, info.params) &&
                    ctx_.functions[other].is_const == info.is_const) {
                    duplicate = true;
                }
            }
        };
        if (ctor) {
            check(rec.constructors);
        } else if (dtor) {
            duplicate = rec.destructor.has_value();
        } else if (auto it = rec.methods.find(name); it != rec.methods.end()) {
            check(it->second);
        }
        if (duplicate) {
            ctx_.report(DiagnosticFactory::redefinition(m.declarator.range, info.display));
            ok = false;
            continue;
        }
        const std::uint32_t id = add_function(std::move(info));
        RecordInfo& r = types.record_at(record);
        if (ctor) {
            r.constructors.push_back(id);
            r.user_constructors = true;
        } else if (dtor) {
            r.destructor = id;
        } else {
            r.methods[name].push_back(id);
        }
        if (m.is_static) {  // also callable as `Class::name(...)`
            Symbol functions;
            functions.kind = SymbolKind::Functions;
            functions.range = m.declarator.range;
            Symbol* entry = statics->add(name, functions);
            if (entry == nullptr) {
                entry = statics->find(name);
            }
            if (entry->kind == SymbolKind::Functions) {
                entry->functions.push_back(Callee{false, id});
            }
        }
        if (m.body || m.is_defaulted) {
            bodies.emplace_back(id, &m);
        }
    }

    for (const auto& t : def.method_templates) {
        ctx_.templates->declare_member(record, t);
    }
    ctx_.symbols.leave_namespace();

    // Virtual functions: declared virtual here, or overriding a base's.
    std::vector<std::uint32_t> own;
    {
        RecordInfo& r = types.record_at(record);
        std::vector<std::uint32_t> members;
        for (const auto& [name, ids] : r.methods) {
            members.insert(members.end(), ids.begin(), ids.end());
        }
        if (r.destructor) {
            members.push_back(*r.destructor);
        }
        std::sort(members.begin(), members.end());
        for (const std::uint32_t id : members) {
            FunctionInfo& fn = ctx_.functions[id];
            bool base_virtual = false;
            std::vector<std::uint32_t> pending;
            pending.reserve(r.bases.size());
            for (const BaseInfo& b : r.bases) {
                pending.push_back(b.record);
            }
            while (!pending.empty()) {
                const std::uint32_t current = pending.back();
                pending.pop_back();
                for (const std::uint32_t v : types.record_at(current).virtual_functions) {
                    base_virtual = base_virtual || overrides(id, v);
                }
                for (const BaseInfo& b : types.record_at(current).bases) {
                    pending.push_back(b.record);
                }
            }
            if (fn.decl != nullptr && fn.decl->is_override && !base_virtual) {
                ctx_.report(DiagnosticFactory::nothing_to_override(fn.range, fn.display));
                ok = false;
            }
            fn.is_virtual = fn.is_virtual || base_virtual;
            if (fn.is_virtual) {
                own.push_back(id);
            }
        }
        r.virtual_functions = own;
    }

    {
        RecordInfo& r = types.record_at(record);
        bool header = !own.empty();
        for (const BaseInfo& b : r.bases) {
            header = header || b.is_virtual || types.record_at(b.record).has_header;
        }
        r.has_header = header;
    }
    layout_record(record);

    // An implicit destructor when members or bases need destruction.
    {
        const RecordInfo& r = types.record_at(record);
        bool needs = false;
        bool virtual_base_dtor = false;
        for (const BaseInfo& b : r.bases) {
            const RecordInfo& bi = types.record_at(b.record);
            needs = needs || ctx_.initializers->needs_destruction(b.type);
            if (bi.destructor) {
                virtual_base_dtor = virtual_base_dtor || ctx_.functions[*bi.destructor].is_virtual;
            }
        }
        for (const FieldInfo& f : r.fields) {
            needs = needs || ctx_.initializers->needs_destruction(f.type);
        }
        if (!r.destructor && (needs || virtual_base_dtor)) {
            FunctionInfo info;
            info.name = "~" + def.name;
            info.display = types.record_at(record).name + "::~" + def.name;
            info.return_type = TypeTable::kVoid;
            info.range = def.name_range;
            info.record = record;
            info.is_destructor = true;
            info.is_virtual = virtual_base_dtor;
            const std::uint32_t id = add_function(std::move(info));
            types.record_at(record).destructor = id;
            if (virtual_base_dtor) {
                types.record_at(record).virtual_functions.push_back(id);
            }
            bodies.emplace_back(id, nullptr);
        }
    }

    types.record_at(record).complete = true;
    build_dispatch(record);
    synthesize_copies(record, def, bodies);
    (void)ok;

    for (const auto& [id, decl] : bodies) {
        define_body(id, decl);
    }
}

void DeclarationBinder::synthesize_copies(std::uint32_t record, const ast::RecordDef& def,
                                          std::vector<std::pair<std::uint32_t, const ast::FunctionDecl*>>& bodies) {
    TypeTable& types = ctx_.types;
    const TypeRef type = types.record_type(record);
    RecordInfo& r = types.record_at(record);
    const TypeRef const_ref = types.reference_to(type, true);

    bool user_copy = false;
    for (const std::uint32_t id : r.constructors) {
        const auto& params = ctx_.functions[id].params;
        user_copy = user_copy || (params.size() == 1 && types.is_reference(params[0].type) &&
                                  types.info(params[0].type).target == type);
    }
    const bool user_assign = r.methods.contains("operator=");

    bool members_copy = true;
    bool members_assign = !r.has_header;  // assignment must keep the dynamic type
    auto visit = [&](TypeRef t) {
        while (types.is_array(t)) {
            t = types.info(t).target;
        }
        if (types.is_record(t)) {
            members_copy = members_copy && types.record(t).trivial_copy;
            members_assign = members_assign && types.record(t).trivial_assign;
        }
    };
    for (const BaseInfo& b : r.bases) {
        visit(b.type);
    }
    for (const FieldInfo& f : r.fields) {
        visit(f.type);
    }
    r.trivial_copy = !user_copy && members_copy;
    r.trivial_assign = !user_assign && members_assign;

    auto implicit = [&](std::string name, TypeRef result, FunctionInfo::Implicit kind) {
        FunctionInfo info;
        info.name = std::move(name);
        info.display = types.record_at(record).name + "::" + info.name;
        info.return_type = result;
        ParamInfo other;
        other.name = "__other";
        other.type = const_ref;
        info.params.push_back(other);
        info.range = def.name_range;
        info.record = record;
        info.is_constructor = kind == FunctionInfo::Implicit::CopyConstructor;
        info.implicit = kind;
        info.defined = true;
        return add_function(std::move(info));
    };
    if (!user_copy && !members_copy) {
        const std::uint32_t id = implicit(def.name, TypeTable::kVoid, FunctionInfo::Implicit::CopyConstructor);
        types.record_at(record).constructors.push_back(id);
        bodies.emplace_back(id, nullptr);
    }
    if (!user_assign && !members_assign) {
        const std::uint32_t id =
            implicit("operator=", types.reference_to(type, false), FunctionInfo::Implicit::CopyAssignment);
        types.record_at(record).methods["operator="].push_back(id);
        bodies.emplace_back(id, nullptr);
    }
}

void DeclarationBinder::layout_record(std::uint32_t record) {
    TypeTable& types = ctx_.types;
    RecordInfo& r = types.record_at(record);
    std::uint32_t offset = 0;
    std::uint32_t bytes = 0;
    std::uint32_t align = 1;

    // Every polymorphic subobject starts with its header. A first
    // non-virtual polymorphic base shares its header with us.
    const bool shares_header =
        !r.bases.empty() && !r.bases.front().is_virtual && types.record_at(r.bases.front().record).has_header;
    if (r.has_header && !shares_header) {
        offset = 1;
        bytes = 8;
        align = 8;
    }
    for (BaseInfo& b : r.bases) {
        if (b.is_virtual) {
            continue;
        }
        const RecordInfo& bi = types.record_at(b.record);
        b.offset = offset;
        offset += bi.nv_size;
        bytes = align_to(bytes, bi.byte_align) + bi.byte_size;
        align = std::max(align, bi.byte_align);
    }
    for (FieldInfo& f : r.fields) {
        f.offset = offset;
        offset += types.cells(f.type);
        const std::uint32_t fa = types.byte_align(f.type);
        bytes = align_to(bytes, fa) + types.byte_size(f.type);
        align = std::max(align, fa);
    }
    r.nv_size = offset;
    r.virtual_base_offsets.clear();
    for (const std::uint32_t v : ctx_.hierarchy.virtual_bases(record)) {
        const RecordInfo& vi = types.record_at(v);
        r.virtual_base_offsets.emplace_back(v, offset);
        offset += vi.nv_size;
        bytes = align_to(bytes, vi.byte_align) + vi.byte_size;
        align = std::max(align, vi.byte_align);
    }
    for (BaseInfo& b : r.bases) {
        if (!b.is_virtual) {
            continue;
        }
        for (const auto& [v, voff] : r.virtual_base_offsets) {
            if (v == b.record) {
                b.offset = voff;
            }
        }
    }
    r.size = std::max<std::uint32_t>(1, offset);
    r.byte_align = align;
    r.byte_size = std::max<std::uint32_t>(1, align_to(bytes, align));
}

void DeclarationBinder::collect_subobjects(std::uint32_t complete, std::uint32_t record, std::uint32_t offset,
                                           std::vector<std::pair<std::uint32_t, std::uint32_t>>& out) const {
    const RecordInfo& r = ctx_.types.record_at(record);
    if (r.has_header) {
        const auto entry = std::pair{record, offset};
        if (std::find(out.begin(), out.end(), entry) == out.end()) {
            out.push_back(entry);
        }
    }
    for (const BaseInfo& b : r.bases) {
        if (!b.is_virtual) {
            collect_subobjects(complete, b.record, offset + b.offset, out);
        }
    }
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> DeclarationBinder::final_overrider(
    std::uint32_t record, std::uint32_t offset, std::uint32_t target_record, std::uint32_t target_offset,
    std::uint32_t function) const {
    // Path from the complete object down to the target subobject; the most
    // derived class on that path that declares an overrider wins.
    const RecordInfo& complete = ctx_.types.record_at(record);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> path;
    auto find = [&](auto&& self, std::uint32_t current, std::uint32_t off) -> bool {
        path.emplace_back(current, off);
        if (current == target_record && off == target_offset) {
            return true;
        }
        for (const BaseInfo& b : ctx_.types.record_at(current).bases) {
            std::uint32_t boff = off + b.offset;
            if (b.is_virtual) {
                for (const auto& [v, voff] : complete.virtual_base_offsets) {
                    if (v == b.record) {
                        boff = offset + voff;
                    }
                }
            }
            if (self(self, b.record, boff)) {
                return true;
            }
        }
        path.pop_back();
        return false;
    };
    if (!find(find, record, offset)) {
        return std::nullopt;
    }
    for (const auto& [rec, off] : path) {
        for (const std::uint32_t id : ctx_.types.record_at(rec).virtual_functions) {
            if (id == function || overrides(id, function)) {
                return std::pair{id, off};
            }
        }
    }
    return std::pair{function, target_offset};
}

void DeclarationBinder::build_dispatch(std::uint32_t record) {
    TypeTable& types = ctx_.types;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> subs;
    collect_subobjects(record, record, 0, subs);
    for (const auto& [v, voff] : types.record_at(record).virtual_base_offsets) {
        collect_subobjects(record, v, voff, subs);
    }
    std::vector<RecordInfo::Subobject> out;
    bool abstract = false;
    for (const auto& [sub, off] : subs) {
        RecordInfo::Subobject s;
        s.record = sub;
        s.offset = off;
        for (const std::uint32_t f : types.record_at(sub).virtual_functions) {
            auto overrider = final_overrider(record, 0, sub, off, f);
            VirtualSlot slot;
            slot.name = ctx_.functions[f].display;
            slot.function = overrider ? overrider->first : f;
            slot.this_offset = overrider ? overrider->second : off;
            slot.is_pure = ctx_.functions[slot.function].is_pure;
            abstract = abstract || slot.is_pure;
            s.slots.push_back(std::move(slot));
        }
        out.push_back(std::move(s));
    }
    RecordInfo& r = types.record_at(record);
    r.subobjects = std::move(out);
    r.is_abstract = abstract;
}

// =============================================================================
// Enums and aliases
// =============================================================================

void DeclarationBinder::bind_enum(const ast::EnumDef& def, SourceRange /*range*/) {
    TypeTable& types = ctx_.types;
    Scope& scope = ctx_.symbols.current();
    if (scope.find(def.name) != nullptr) {
        ctx_.report(DiagnosticFactory::redefinition(def.name_range, def.name));
        return;
    }
    EnumInfo info;
    info.name = def.name;
    info.scoped = def.scoped;
    const TypeRef type = types.add_enum(std::move(info));
    Symbol t;
    t.kind = SymbolKind::Type;
    t.type = type;
    t.range = def.name_range;
    scope.add(def.name, t);

    std::int64_t next = 0;
    for (const auto& e : def.enumerators) {
        std::int64_t value = next;
        if (e.value) {
            auto bound = ctx_.expressions->bind_rvalue(*e.value);
            if (!bound) {
                continue;
            }
            auto constant = ctx_.constants.integer(*bound);
            if (!constant || !(types.is_integral(bound->type) || types.is_enum(bound->type))) {
                ctx_.report(DiagnosticFactory::not_constant(e.value->range));
                continue;
            }
            value = *constant;
        }
        next = value + 1;
        types.enum_info(type).enumerators.emplace_back(e.name, value);
        if (!def.scoped) {
            Symbol c;
            c.kind = SymbolKind::Constant;
            c.type = type;
            c.value = value;
            c.range = e.range;
            if (scope.add(e.name, c) == nullptr) {
                ctx_.report(DiagnosticFactory::redefinition(e.range, e.name));
            }
        }
    }
}

void DeclarationBinder::bind_alias(const ast::AliasDecl& alias, SourceRange range) {
    auto resolved = ctx_.type_resolver->resolve(alias.type);
    Scope& scope = ctx_.symbols.current();
    if (!resolved || resolved->is_auto) {
        Symbol p;
        p.kind = SymbolKind::Poisoned;
        scope.add(alias.name, p);
        return;
    }
    Symbol t;
    t.kind = SymbolKind::Type;
    t.type = resolved->type;
    t.range = range;
    if (scope.add(alias.name, t) == nullptr) {
        ctx_.report(DiagnosticFactory::redefinition(range, alias.name));
    }
}

void DeclarationBinder::poison(const ast::Stmt& stmt) {
    Scope& scope = ctx_.symbols.current();
    auto add = [&](const std::string& name) {
        if (!name.empty() && scope.find(name) == nullptr) {
            Symbol p;
            p.kind = SymbolKind::Poisoned;
            scope.add(name, p);
        }
    };
    std::visit(detail::Overloaded{
                   [&](const ast::DeclStmt& d) {
                       for (const auto& v : d.vars) {
                           add(v.declarator.name);
                       }
                   },
                   [&](const ast::FunctionDef& f) {
                       if (f.decl.scope.empty()) {
                           add(f.decl.declarator.name);
                       }
                   },
                   [&](const ast::RecordDef& r) { add(r.name); },
                   [&](const ast::EnumDef& e) {
                       add(e.name);
                       if (!e.scoped) {
                           for (const auto& en : e.enumerators) {
                               add(en.name);
                           }
                       }
                   },
                   [&](const ast::AliasDecl& a) { add(a.name); },
                   [&](const auto&) {},
               },
               stmt.node);
}

const ast::DeclStmt* DeclarationBinder::as_object_declaration(const ast::FunctionDecl& decl) {
    if (!decl.object_reading || decl.body) {
        return nullptr;
    }
    // C++ reads it as a function whenever it can be one: it is an object only
    // if some "parameter" cannot be a type (a literal, a variable, an enumerator...).
    for (const ast::Param& p : decl.params) {
        if (p.type.scope.empty() && (p.type.name == "true" || p.type.name == "false" || p.type.name == "nullptr")) {
            return decl.object_reading.get();
        }
        const Symbol* s = p.type.scope.empty() ? ctx_.symbols.lookup(p.type.name)
                                               : ctx_.symbols.lookup_qualified(p.type.scope, p.type.name);
        if (s != nullptr &&
            (s->kind == SymbolKind::Variable || s->kind == SymbolKind::Constant || s->kind == SymbolKind::Functions)) {
            return decl.object_reading.get();
        }
    }
    return nullptr;
}

std::uint32_t DeclarationBinder::implicit_default_constructor(std::uint32_t record, SourceRange site) {
    TypeTable& types = ctx_.types;
    if (const auto existing = types.record_at(record).implicit_default_constructor) {
        return *existing;
    }
    const RecordInfo& r = types.record_at(record);
    FunctionInfo info;
    info.name = r.name;
    info.display = r.name + "::" + r.name;
    info.return_type = TypeTable::kVoid;
    info.range = r.range;
    info.record = record;
    info.is_constructor = true;
    info.implicit = FunctionInfo::Implicit::DefaultConstructor;
    info.defined = true;
    const std::uint32_t id = add_function(std::move(info));
    types.record_at(record).implicit_default_constructor = id;
    const SourceRange previous = ctx_.implicit_site;
    ctx_.implicit_site = site;
    define_body(id, nullptr);
    ctx_.implicit_site = previous;
    return id;
}

std::uint32_t DeclarationBinder::thrown_index(TypeRef type) {
    auto& thrown = ctx_.program.thrown;
    for (std::size_t i = 0; i < thrown.size(); ++i) {
        if (thrown[i].type == type) {
            return static_cast<std::uint32_t>(i);
        }
    }
    ThrownType t;
    t.type = type;
    t.name = ctx_.types.name(type);
    if (const Symbol* s = ctx_.symbols.lookup_qualified({"std"}, t.name);
        s != nullptr && s->kind == SymbolKind::Type && s->type == type) {
        t.name = "std::" + t.name;
    }
    if (ctx_.types.is_record(type)) {
        t.destructor = ctx_.types.record(type).destructor;
    }
    thrown.push_back(std::move(t));
    return static_cast<std::uint32_t>(thrown.size() - 1);
}

std::optional<std::uint32_t> DeclarationBinder::base_offset(std::uint32_t derived, std::uint32_t base) const {
    if (derived == base) {
        return 0;
    }
    auto path = ctx_.hierarchy.find_base(derived, base);
    if (!path || path->ambiguous) {
        return std::nullopt;
    }
    if (!path->via_virtual) {
        return path->offset;
    }
    for (const auto& [record, offset] : ctx_.types.record_at(derived).virtual_base_offsets) {
        if (record == path->virtual_base) {
            return offset + path->offset_in_virtual;
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> DeclarationBinder::catch_offset(TypeRef thrown, TypeRef caught) const {
    const TypeTable& types = ctx_.types;
    if (thrown == caught) {
        return 0;
    }
    if (types.is_pointer(thrown) && types.is_pointer(caught)) {
        const TypeInfo& t = types.info(thrown);
        const TypeInfo& c = types.info(caught);
        if (t.target == c.target && (c.target_const || !t.target_const)) {
            return 0;  // `catch (const char* s)` takes a thrown `char*`
        }
        return std::nullopt;
    }
    if (types.is_record(thrown) && types.is_record(caught)) {
        return base_offset(types.info(thrown).decl, types.info(caught).decl);  // a base catches derived exceptions
    }
    return std::nullopt;
}

void DeclarationBinder::resolve_catches() {
    TypeTable& types = ctx_.types;
    // std::exception keeps its message in `std::string __text`, whose first cell is the char*.
    std::optional<std::uint32_t> exception_record;
    std::optional<std::uint32_t> text_offset;
    if (const Symbol* s = ctx_.symbols.lookup_qualified({"std"}, "exception");
        s != nullptr && s->kind == SymbolKind::Type && types.is_record(s->type)) {
        exception_record = types.info(s->type).decl;
        for (const FieldInfo& f : types.record_at(*exception_record).fields) {
            if (f.name == "__text") {
                text_offset = f.offset;
            }
        }
    }
    for (ThrownType& t : ctx_.program.thrown) {
        if (exception_record && text_offset && types.is_record(t.type)) {
            if (auto base = base_offset(types.info(t.type).decl, *exception_record)) {
                t.message_offset = *base + *text_offset;
            }
        }
    }
    for (auto& table : ctx_.program.catch_tables) {
        for (CatchClauseInfo& clause : table) {
            if (clause.catch_all) {
                continue;
            }
            for (std::size_t i = 0; i < ctx_.program.thrown.size(); ++i) {
                if (auto offset = catch_offset(ctx_.program.thrown[i].type, clause.type)) {
                    clause.matches.emplace_back(static_cast<std::uint32_t>(i), *offset);
                }
            }
        }
    }
}

void DeclarationBinder::finish(std::vector<BStmt>& script) {
    resolve_catches();
    for (const auto& [symbol, member] : statics_) {
        if (member.needs_construction && !member.defined) {
            ctx_.report(DiagnosticFactory::undefined_function(member.range, member.display));
        }
    }
    for (const std::uint32_t id : ctx_.called) {
        const FunctionInfo& fn = ctx_.functions[id];
        if (!fn.defined && !fn.is_pure && !fn.deleted) {
            ctx_.report(DiagnosticFactory::undefined_function(fn.range, fn.display));
        }
    }
    if (main_ && ctx_.functions[*main_].defined) {
        auto call = ctx_.expressions->make_call(Callee{false, *main_}, {}, ctx_.functions[*main_].range);
        if (call) {
            script.push_back(wrap(call->range, BExprStmt{std::move(*call)}));
        }
    }
}

}  // namespace cppi::sema
