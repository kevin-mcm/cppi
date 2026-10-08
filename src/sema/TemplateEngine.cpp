#include "sema/TemplateEngine.hpp"

#include "sema/DeclarationBinder.hpp"
#include "sema/ExpressionBinder.hpp"
#include "sema/LibraryScope.hpp"
#include "sema/OverloadResolver.hpp"
#include "sema/StatementBinder.hpp"
#include "sema/TypeResolver.hpp"
#include "support/DiagnosticFactory.hpp"

#include <algorithm>
#include <utility>

namespace cppi::sema {

using detail::DiagnosticFactory;

template <typename Body>
auto TemplateEngine::in_template_scope(const TemplateInfo& t, const std::vector<TypeRef>& args, Body body) {
    auto previous = ctx_.symbols.isolate(t.home);
    ctx_.symbols.push(Scope::Kind::Block);
    for (std::size_t i = 0; i < t.params.size() && i < args.size(); ++i) {
        Symbol param;
        param.kind = SymbolKind::Type;
        param.type = args[i];
        ctx_.symbols.current().add(t.params[i], param);
    }
    auto result = body();
    ctx_.symbols.pop();
    ctx_.symbols.restore(std::move(previous));
    return result;
}

std::string TemplateEngine::display(const TemplateInfo& t, const std::vector<TypeRef>& args) const {
    std::string out = t.name + "<";
    for (std::size_t i = 0; i < args.size(); ++i) {
        out += (i > 0 ? ", " : "") + ctx_.types.name(args[i]);
    }
    return out + ">";
}

std::optional<std::pair<std::uint32_t, std::vector<TypeRef>>> TemplateEngine::origin(TypeRef type) const {
    if (auto it = origins_.find(type); it != origins_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool TemplateEngine::declare(const ast::TemplateDecl& decl, SourceRange range) {
    TemplateInfo info;
    for (const auto& p : decl.params) {
        info.params.push_back(p.name);
        info.defaults.push_back(p.default_type ? &*p.default_type : nullptr);
    }
    info.inner = decl.inner.get();
    if (const auto* fn = std::get_if<ast::FunctionDef>(&decl.inner->node)) {
        if (!fn->decl.scope.empty()) {
            return define_member(decl, fn->decl, range);
        }
        info.name = fn->decl.declarator.name;
    } else if (const auto* record = std::get_if<ast::RecordDef>(&decl.inner->node)) {
        info.name = record->name;
        info.is_class = true;
    } else if (const auto* concept_def = std::get_if<ast::ConceptDef>(&decl.inner->node)) {
        info.name = concept_def->name;
        info.is_concept = true;
        info.concept_expr = concept_def->constraint.get();
    } else {
        ctx_.report(DiagnosticFactory::feature_not_implemented(range, Feature::Templates));
        return false;
    }
    info.home = ctx_.symbols.snapshot();
    info.library = ctx_.library_depth > 0;
    const auto* fn_def = std::get_if<ast::FunctionDef>(&decl.inner->node);
    if (!collect_constraints(info, decl.params, decl.requires_clause.get(),
                             fn_def != nullptr ? fn_def->decl.requires_clause.get() : nullptr, range)) {
        return false;
    }

    Scope& scope = ctx_.symbols.current();
    Symbol* existing = scope.find(info.name);
    if (existing != nullptr && existing->kind == SymbolKind::Template &&
        (info.is_concept || templates_[existing->templates.front()].is_concept)) {
        ctx_.report(DiagnosticFactory::redefinition(range, info.name));
        return false;
    }
    if (existing != nullptr && existing->kind != SymbolKind::Template) {
        if (existing->kind != SymbolKind::Poisoned) {
            ctx_.report(DiagnosticFactory::redefinition(range, info.name));
        }
        return false;
    }
    if (existing != nullptr && info.is_class) {
        TemplateInfo& previous = templates_[existing->templates.front()];
        const auto& prev_def = std::get<ast::RecordDef>(previous.inner->node);
        if (!previous.is_class || prev_def.has_body) {
            ctx_.report(DiagnosticFactory::redefinition(range, info.name));
            return false;
        }
        previous.inner = info.inner;  // the definition after a forward declaration
        return true;
    }
    templates_.push_back(std::move(info));
    const auto id = static_cast<std::uint32_t>(templates_.size() - 1);
    if (existing == nullptr) {
        Symbol t;
        t.kind = SymbolKind::Template;
        t.range = range;
        scope.add(templates_.back().name, t);
        existing = scope.find(templates_.back().name);
    }
    existing->templates.push_back(id);
    return true;
}

std::optional<std::uint32_t> TemplateEngine::instantiate_function(std::uint32_t id, const std::vector<TypeRef>& args,
                                                                  SourceRange range) {
    TemplateInfo& t = templates_[id];
    if (auto it = t.functions.find(args); it != t.functions.end()) {
        return it->second;
    }
    const auto& decl = std::get<ast::FunctionDef>(t.inner->node).decl;
    std::optional<LibraryScope> library;
    if (t.library) {
        library.emplace(ctx_, range, "std::" + display(t, args));
    }
    const std::size_t errors_before = ctx_.sink.dropped_errors();
    auto result = in_template_scope(t, args, [&] {
        return ctx_.declarations->instantiate_function(decl, display(t, args),
                                                       [&](std::uint32_t fid) { t.functions[args] = fid; });
    });
    forget_if_failed(t.functions, args, errors_before);
    return result;
}

std::optional<TypeRef> TemplateEngine::instantiate_class(std::uint32_t id, const std::vector<TypeRef>& args,
                                                         SourceRange range) {
    TemplateInfo& t = templates_[id];
    if (auto it = t.classes.find(args); it != t.classes.end()) {
        return it->second;
    }
    const auto& def = std::get<ast::RecordDef>(t.inner->node);
    if (!def.has_body) {
        ctx_.report(DiagnosticFactory::unknown_type(range, display(t, args), std::nullopt));
        return std::nullopt;
    }
    std::optional<LibraryScope> library;
    if (t.library) {
        library.emplace(ctx_, range, "std::" + display(t, args));
    }
    const std::size_t errors_before = ctx_.sink.dropped_errors();
    auto result = in_template_scope(t, args, [&]() -> std::optional<TypeRef> {
        RecordInfo info;
        info.name = display(t, args);
        info.is_class = def.is_class;
        info.range = def.name_range;
        const TypeRef type = ctx_.types.add_record(std::move(info));
        t.classes[args] = type;
        origins_[type] = {id, args};
        // The injected class name: inside the template, `Box` means this instance.
        Symbol self;
        self.kind = SymbolKind::Type;
        self.type = type;
        ctx_.symbols.current().add(def.name, self);
        ctx_.declarations->bind_record(def, def.name_range);
        return type;
    });
    forget_if_failed(t.classes, args, errors_before);
    return result;
}

std::optional<TypeRef> TemplateEngine::instantiate_type(const ast::TypeSpec& spec) {
    Symbol* symbol =
        spec.scope.empty() ? ctx_.symbols.lookup(spec.name) : ctx_.symbols.lookup_qualified(spec.scope, spec.name);
    return instantiate_named(symbol, spec.template_args, spec.name, spec.range);
}

std::optional<TypeRef> TemplateEngine::instantiate_named(Symbol* symbol, const std::vector<ast::TypeDesc>& descs,
                                                         const std::string& name, SourceRange range) {
    std::optional<std::uint32_t> template_id;
    if (symbol != nullptr && symbol->kind == SymbolKind::Template && templates_[symbol->templates.front()].is_class) {
        template_id = symbol->templates.front();
    } else if (symbol != nullptr && symbol->kind == SymbolKind::Type) {
        // Inside a class template, its name refers to the current instance:
        // `Box<T>` names the template again.
        if (auto from = origin(symbol->type)) {
            template_id = from->first;
        }
    }
    if (!template_id) {
        if (symbol == nullptr || symbol->kind != SymbolKind::Poisoned) {
            ctx_.report(DiagnosticFactory::unknown_type(range, name, std::nullopt));
        }
        return std::nullopt;
    }
    const std::uint32_t id = *template_id;
    std::vector<TypeRef> args;
    for (const auto& desc : descs) {
        auto resolved = ctx_.type_resolver->resolve(desc);
        if (!resolved || resolved->is_auto) {
            return std::nullopt;
        }
        args.push_back(resolved->type);
    }
    while (args.size() < templates_[id].params.size()) {
        auto fallback = default_arg(templates_[id], args);
        if (!fallback) {
            break;
        }
        args.push_back(*fallback);
    }
    if (args.size() != templates_[id].params.size()) {
        ctx_.report(DiagnosticFactory::template_deduction(range, name));
        return std::nullopt;
    }
    if (auto why = unsatisfied(templates_[id], args, range)) {
        ctx_.report(DiagnosticFactory::constraints_not_satisfied(range, name, why));
        return std::nullopt;
    }
    return instantiate_class(id, args, range);
}

bool TemplateEngine::unify(const TemplateInfo& t, const ast::TypeSpec& spec, TypeRef actual, Bindings& bindings) const {
    const bool is_param = spec.scope.empty() && spec.template_args.empty() &&
                          std::find(t.params.begin(), t.params.end(), spec.name) != t.params.end();
    if (is_param) {
        auto [it, inserted] = bindings.emplace(spec.name, actual);
        return inserted || it->second == actual;
    }
    if (spec.template_args.empty()) {
        return true;  // not dependent
    }
    auto from = origin(actual);
    if (!from) {
        return false;
    }
    // The name means what it meant where the template was declared (`variant` inside namespace std).
    const Symbol* symbol = nullptr;
    if (spec.scope.empty()) {
        for (auto it = t.home.rbegin(); it != t.home.rend() && symbol == nullptr; ++it) {
            symbol = (*it)->find(spec.name);
        }
    }
    if (symbol == nullptr) {
        symbol =
            spec.scope.empty() ? ctx_.symbols.lookup(spec.name) : ctx_.symbols.lookup_qualified(spec.scope, spec.name);
    }
    if (symbol == nullptr || symbol->kind != SymbolKind::Template || symbol->templates.front() != from->first) {
        return false;
    }
    for (std::size_t i = 0; i < spec.template_args.size() && i < from->second.size(); ++i) {
        if (!spec.template_args[i].declarator.parts.empty()) {
            continue;
        }
        if (!unify(t, spec.template_args[i].spec, from->second[i], bindings)) {
            return false;
        }
    }
    return true;
}

bool TemplateEngine::deduce(const TemplateInfo& t, const ast::TypeSpec& spec, const ast::Declarator& declarator,
                            const BExpr& arg, Bindings& bindings) const {
    const TypeTable& types = ctx_.types;
    TypeRef actual = arg.type;
    const auto& parts = declarator.parts;
    if (parts.empty()) {
        if (types.is_array(actual)) {
            actual = ctx_.types.pointer_to(types.info(actual).target, arg.is_const);
        }
    } else if (parts.size() == 1 && parts[0].kind == ast::DeclaratorKind::Reference) {
        // T& and const T&: the argument's own type.
    } else if (parts.size() == 1 &&
               (parts[0].kind == ast::DeclaratorKind::Pointer || parts[0].kind == ast::DeclaratorKind::Array)) {
        if (types.is_pointer(actual) || types.is_array(actual)) {
            actual = types.info(actual).target;
        } else {
            return false;
        }
    } else {
        return true;  // too complex to deduce from: overload resolution will check it
    }
    return unify(t, spec, actual, bindings);
}

std::optional<TypeRef> TemplateEngine::default_arg(const TemplateInfo& t, const std::vector<TypeRef>& previous) {
    const std::size_t i = previous.size();
    if (i >= t.defaults.size() || t.defaults[i] == nullptr) {
        return std::nullopt;
    }
    // Resolved where the template was declared, after the parameters before it.
    const ast::TypeDesc& desc = *t.defaults[i];
    return in_template_scope(t, previous, [&]() -> std::optional<TypeRef> {
        auto resolved = ctx_.type_resolver->resolve(desc);
        if (!resolved || resolved->is_auto) {
            return std::nullopt;
        }
        return resolved->type;
    });
}

std::optional<std::vector<TypeRef>> TemplateEngine::deduce_all(const TemplateInfo& t, const ast::FunctionDecl& decl,
                                                               const std::vector<TypeRef>& explicit_args,
                                                               const std::vector<BExprPtr>& args) {
    if (explicit_args.size() > t.params.size() || args.size() > decl.params.size()) {
        return std::nullopt;
    }
    Bindings bindings;
    for (std::size_t i = 0; i < explicit_args.size(); ++i) {
        bindings[t.params[i]] = explicit_args[i];
    }
    std::size_t next_auto = 0;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const ast::TypeSpec& spec = decl.params[i].type;
        if (t.generic_lambda && spec.name == "auto") {
            ast::TypeSpec named;  // `auto x` deduces auto#k like `T x` deduces T
            named.name = "auto#" + std::to_string(next_auto++);
            if (!deduce(t, named, decl.params[i].declarator, *args[i], bindings)) {
                return std::nullopt;
            }
            continue;
        }
        const auto explicit_end = t.params.begin() + static_cast<std::ptrdiff_t>(explicit_args.size());
        if (spec.scope.empty() && spec.template_args.empty() &&
            std::find(t.params.begin(), explicit_end, spec.name) != explicit_end) {
            continue;  // given explicitly: the argument just converts
        }
        if (!deduce(t, spec, decl.params[i].declarator, *args[i], bindings)) {
            return std::nullopt;
        }
    }
    std::vector<TypeRef> type_args;
    for (const auto& p : t.params) {
        auto it = bindings.find(p);
        if (it != bindings.end()) {
            type_args.push_back(it->second);
            continue;
        }
        auto fallback = default_arg(t, type_args);
        if (!fallback) {
            return std::nullopt;
        }
        type_args.push_back(*fallback);
    }
    return type_args;
}

bool TemplateEngine::define_member(const ast::TemplateDecl& decl, const ast::FunctionDecl& fn, SourceRange range) {
    // `template <class T> T Box::get(T x) { ... }`: the body of a member template declared in the class.
    const Symbol* scope = fn.scope.size() == 1 ? ctx_.symbols.lookup(fn.scope.front()) : nullptr;
    if (scope == nullptr || scope->kind != SymbolKind::Type || !ctx_.types.is_record(scope->type)) {
        ctx_.report(DiagnosticFactory::unknown_type(range, fn.scope.empty() ? fn.declarator.name : fn.scope.front(),
                                                    std::nullopt));
        return false;
    }
    const std::uint32_t record = ctx_.types.info(scope->type).decl;
    const RecordInfo& info = ctx_.types.record_at(record);
    if (auto it = info.method_templates.find(fn.declarator.name); it != info.method_templates.end()) {
        for (const std::uint32_t id : it->second) {
            TemplateInfo& t = templates_[id];
            if (t.method->body == nullptr && t.params.size() == decl.params.size() &&
                t.method->params.size() == fn.params.size() && t.functions.empty()) {
                t.method = &fn;
                t.params.clear();
                t.defaults.clear();
                for (const auto& p : decl.params) {
                    t.params.push_back(p.name);
                    t.defaults.push_back(p.default_type ? &*p.default_type : nullptr);
                }
                return true;
            }
        }
    }
    ctx_.report(DiagnosticFactory::no_member(fn.declarator.range, info.name, fn.declarator.name, std::nullopt));
    return false;
}

void TemplateEngine::declare_generic_lambda(std::uint32_t record, const ast::FunctionDecl& decl, std::size_t autos,
                                            std::vector<Scope*> home, bool is_const, bool deduce_return) {
    TemplateInfo info;
    info.name = "operator()";
    for (std::size_t i = 0; i < autos; ++i) {
        info.params.push_back("auto#" + std::to_string(i));
        info.defaults.push_back(nullptr);
    }
    info.method = &decl;
    info.record = record;
    info.home = std::move(home);
    info.library = ctx_.library_depth > 0;
    info.generic_lambda = true;
    std::size_t next_auto = 0;
    for (const auto& p : decl.params) {
        if (p.type.name != "auto") {
            continue;
        }
        if (p.type.auto_constraint) {  // `[](std::integral auto x)`
            if (!find_concept(p.type.auto_constraint->scope, p.type.auto_constraint->name)) {
                ctx_.report(DiagnosticFactory::unknown_type(p.range, p.type.auto_constraint->name, std::nullopt));
                return;
            }
            info.constrained.emplace_back(next_auto, p.type.auto_constraint.get());
        }
        ++next_auto;
    }
    info.lambda_const = is_const;
    info.lambda_deduce = deduce_return;
    templates_.push_back(std::move(info));
    const auto id = static_cast<std::uint32_t>(templates_.size() - 1);
    ctx_.types.record_at(record).method_templates["operator()"].push_back(id);
}

void TemplateEngine::declare_member(std::uint32_t record, const ast::MethodTemplate& method) {
    TemplateInfo info;
    info.name = method.decl.declarator.name;
    for (const auto& p : method.params) {
        info.params.push_back(p.name);
        info.defaults.push_back(p.default_type ? &*p.default_type : nullptr);
    }
    info.method = &method.decl;
    info.record = record;
    if (!collect_constraints(info, method.params, method.requires_clause.get(), method.decl.requires_clause.get(),
                             method.decl.declarator.range)) {
        return;
    }
    // Function and block scopes die with the code being bound (a class template's
    // parameters live in one): keep their type names in a scope that lasts.
    for (Scope* scope : ctx_.symbols.snapshot()) {
        if (scope->kind() == Scope::Kind::Block || scope->kind() == Scope::Kind::Function) {
            Scope* kept = ctx_.symbols.new_class_scope();
            for (const auto& [name, symbol] : scope->symbols()) {
                if (symbol.kind == SymbolKind::Type || symbol.kind == SymbolKind::Template) {
                    kept->add(name, symbol);
                }
            }
            scope = kept;
        }
        info.home.push_back(scope);
    }
    info.library = ctx_.library_depth > 0;
    templates_.push_back(std::move(info));
    const auto id = static_cast<std::uint32_t>(templates_.size() - 1);
    ctx_.types.record_at(record).method_templates[templates_.back().name].push_back(id);
}

const std::vector<std::uint32_t>* TemplateEngine::member_templates(std::uint32_t record, std::string_view name) const {
    const RecordInfo& info = ctx_.types.record_at(record);
    if (auto it = info.method_templates.find(name); it != info.method_templates.end()) {
        return &it->second;
    }
    if (info.methods.find(name) != info.methods.end()) {
        return nullptr;  // hidden by an ordinary member function of the same name
    }
    for (const BaseInfo& base : info.bases) {
        if (const auto* found = member_templates(base.record, name)) {
            return found;
        }
    }
    return nullptr;
}

bool TemplateEngine::has_member_template(std::uint32_t record, std::string_view name) const {
    return member_templates(record, name) != nullptr;
}

std::vector<Callee> TemplateEngine::member_candidates(std::uint32_t record, const std::string& name,
                                                      const std::vector<ast::TypeDesc>& explicit_descs,
                                                      const std::vector<BExprPtr>& args, SourceRange range) {
    std::vector<Callee> out;
    const auto* ids = member_templates(record, name);
    if (ids == nullptr) {
        return out;
    }
    std::vector<TypeRef> explicit_args;
    for (const auto& desc : explicit_descs) {
        auto resolved = ctx_.type_resolver->resolve(desc);
        if (!resolved || resolved->is_auto) {
            return out;
        }
        explicit_args.push_back(resolved->type);
    }
    const std::vector<std::uint32_t> candidates = *ids;  // copy: instantiating may add templates
    for (const std::uint32_t id : candidates) {
        auto type_args = deduce_all(templates_[id], *templates_[id].method, explicit_args, args);
        if (!type_args || unsatisfied(templates_[id], *type_args, range)) {
            continue;
        }
        if (auto fid = instantiate_method(id, *type_args, range)) {
            out.push_back(Callee{false, *fid});
        }
    }
    return out;
}

std::optional<std::uint32_t> TemplateEngine::instantiate_method(std::uint32_t id, const std::vector<TypeRef>& args,
                                                                SourceRange range) {
    TemplateInfo& t = templates_[id];
    if (auto it = t.functions.find(args); it != t.functions.end()) {
        return it->second;
    }
    std::optional<LibraryScope> library;
    if (t.library) {
        library.emplace(ctx_, range);
    }
    const std::uint32_t record = t.record.value_or(0);
    DeclarationBinder::GenericLambda lambda{&args, t.lambda_const, t.lambda_deduce};
    const std::size_t errors_before = ctx_.sink.dropped_errors();
    auto result = in_template_scope(t, args, [&] {
        return ctx_.declarations->instantiate_method(
            record, *t.method,
            t.generic_lambda ? ctx_.types.record_at(record).name
                             : ctx_.types.record_at(record).name + "::" + display(t, args),
            [&](std::uint32_t fid) { t.functions[args] = fid; }, t.generic_lambda ? &lambda : nullptr);
    });
    forget_if_failed(t.functions, args, errors_before);
    return result;
}

std::optional<bool> TemplateEngine::type_trait(const std::string& name, const std::vector<TypeRef>& types) const {
    const TypeTable& t = ctx_.types;
    if (name == "__cppi_is_same" && types.size() == 2) {
        return types[0] == types[1];
    }
    if (name == "__cppi_is_base_of" && types.size() == 2) {  // <Base, Derived>
        return t.is_record(types[0]) && t.is_record(types[1]) &&
               (types[0] == types[1] ||
                ctx_.hierarchy.find_base(t.info(types[1]).decl, t.info(types[0]).decl).has_value());
    }
    if (name == "__cppi_is_convertible" && types.size() == 2) {  // <From, To>
        if (t.is_void(types[0]) || t.is_void(types[1])) {
            return t.is_void(types[0]) && t.is_void(types[1]);
        }
        BExpr probe;
        probe.type = types[0];
        probe.node = BLoad{};
        return ctx_.overloads->rank(probe, types[1]) != ConversionRank::None;
    }
    if (types.size() != 1) {
        return std::nullopt;
    }
    const TypeRef type = types[0];
    const TypeKind kind = t.kind(type);
    if (name == "__cppi_is_integral") return t.is_integral(type) && kind != TypeKind::Enum;
    if (name == "__cppi_is_floating_point") return kind == TypeKind::Double;
    if (name == "__cppi_is_arithmetic") return t.is_arithmetic(type) && kind != TypeKind::Enum;
    if (name == "__cppi_is_signed") return t.is_arithmetic(type) && !t.is_unsigned(type) && kind != TypeKind::Bool;
    if (name == "__cppi_is_unsigned") return t.is_unsigned(type) || kind == TypeKind::Bool;
    if (name == "__cppi_is_class") return t.is_record(type);
    if (name == "__cppi_is_pointer") return t.is_pointer(type);
    if (name == "__cppi_is_enum") return t.is_enum(type);
    return std::nullopt;
}

// =============================================================================
// Concepts (C++20)
// =============================================================================

template <class Cache, class Key>
void TemplateEngine::forget_if_failed(Cache& cache, const Key& key, std::size_t errors_before) const {
    if (ctx_.sink.muted() && ctx_.sink.dropped_errors() != errors_before) {
        cache.erase(key);
    }
}

bool TemplateEngine::collect_constraints(TemplateInfo& info, const std::vector<ast::TemplateParam>& params,
                                         const ast::Expr* requires_clause, const ast::Expr* trailing,
                                         SourceRange range) {
    for (std::size_t i = 0; i < params.size(); ++i) {
        const auto& constraint = params[i].constraint;
        if (!constraint) {
            continue;
        }
        const ast::TypeSpec& spec = *constraint;
        if (!find_concept(spec.scope, spec.name)) {
            // `template <int N>` looks like `template <Number T>`.
            ctx_.report(DiagnosticFactory::unsupported_syntax(params[i].range, "non-type template parameter"));
            return false;
        }
        info.constrained.emplace_back(i, &spec);
    }
    for (const ast::Expr* clause : {requires_clause, trailing}) {
        if (clause != nullptr) {
            if (!ctx_.features.allow(Feature::Concepts, clause->range)) {
                return false;
            }
            info.requirements.push_back(clause);
        }
    }
    (void)range;
    return true;
}

std::optional<std::uint32_t> TemplateEngine::find_concept(const std::vector<std::string>& scope,
                                                          const std::string& name) {
    const Symbol* symbol = scope.empty() ? ctx_.symbols.lookup(name) : ctx_.symbols.lookup_qualified(scope, name);
    if (symbol == nullptr || symbol->kind != SymbolKind::Template || symbol->templates.empty() ||
        !templates_[symbol->templates.front()].is_concept) {
        return std::nullopt;
    }
    return symbol->templates.front();
}

bool TemplateEngine::holds(const ast::Expr& expr) {
    if (ctx_.fn == nullptr) {
        return false;
    }
    const std::size_t errors_before = ctx_.sink.dropped_errors();
    const std::size_t temps_mark = ctx_.fn->temporaries.size();
    ctx_.sink.mute();
    auto bound = ctx_.expressions->bind_condition(expr);
    ctx_.sink.unmute();
    ctx_.statements->drop_temporaries(temps_mark);
    if (!bound || ctx_.sink.dropped_errors() != errors_before) {
        return false;  // not even valid for these types: not satisfied
    }
    auto value = ctx_.constants.integer(*bound);
    return value && *value != 0;
}

bool TemplateEngine::evaluate_concept(std::uint32_t id, const std::vector<TypeRef>& args) {
    TemplateInfo& t = templates_[id];
    if (auto it = t.satisfied.find(args); it != t.satisfied.end()) {
        return it->second;
    }
    if (args.size() != t.params.size() || t.concept_expr == nullptr || concept_depth_ > 64) {
        return false;
    }
    ++concept_depth_;
    std::optional<LibraryScope> library;
    if (t.library) {
        library.emplace(ctx_, t.concept_expr->range);
    }
    const ast::Expr& expr = *t.concept_expr;
    const bool result = in_template_scope(t, args, [&] { return holds(expr); });
    --concept_depth_;
    templates_[id].satisfied[args] = result;
    return result;
}

std::optional<bool> TemplateEngine::concept_holds(const ast::TypeSpec& spec, TypeRef first, SourceRange range) {
    auto id = find_concept(spec.scope, spec.name);
    if (!id) {
        ctx_.report(DiagnosticFactory::unknown_type(range, spec.name, std::nullopt));
        return std::nullopt;
    }
    std::vector<TypeRef> args{first};
    for (const auto& desc : spec.template_args) {
        auto resolved = ctx_.type_resolver->resolve(desc);
        if (!resolved || resolved->is_auto) {
            return std::nullopt;
        }
        args.push_back(resolved->type);
    }
    return evaluate_concept(*id, args);
}

BExprPtr TemplateEngine::concept_value(const ast::CallExpr& id, SourceRange range) {
    auto concept_id = find_concept(id.scope, id.callee);
    if (!concept_id) {
        const Symbol* symbol =
            id.scope.empty() ? ctx_.symbols.lookup(id.callee) : ctx_.symbols.lookup_qualified(id.scope, id.callee);
        if (symbol == nullptr) {
            ctx_.report(DiagnosticFactory::unknown_identifier(id.callee_range, id.callee, std::nullopt));
        } else if (symbol->kind != SymbolKind::Poisoned) {
            ctx_.report(DiagnosticFactory::unsupported_syntax(range, "type name"));
        }
        return nullptr;
    }
    if (!ctx_.features.allow(Feature::Concepts, range)) {
        return nullptr;
    }
    std::vector<TypeRef> args;
    for (const auto& desc : id.template_args) {
        auto resolved = ctx_.type_resolver->resolve(desc);
        if (!resolved || resolved->is_auto) {
            return nullptr;
        }
        args.push_back(resolved->type);
    }
    if (args.size() != templates_[*concept_id].params.size()) {
        ctx_.report(DiagnosticFactory::template_deduction(id.callee_range, id.callee));
        return nullptr;
    }
    return ExpressionBinder::constant(TypeTable::kBool, evaluate_concept(*concept_id, args) ? 1 : 0, range);
}

std::optional<std::string> TemplateEngine::unsatisfied(const TemplateInfo& t, const std::vector<TypeRef>& args,
                                                       SourceRange range) {
    if (t.constrained.empty() && t.requirements.empty()) {
        return std::nullopt;
    }
    const auto constrained = t.constrained;  // copies: evaluating may add templates
    const auto requirements = t.requirements;
    for (const auto& entry : constrained) {
        const std::size_t index = entry.first;
        const ast::TypeSpec* spec = entry.second;
        if (index >= args.size()) {
            continue;
        }
        // Resolved where the template was declared, with its parameters bound.
        const auto ok = in_template_scope(t, args, [&] { return concept_holds(*spec, args[index], range); });
        if (!ok.value_or(false)) {
            return spec->name + "<" + ctx_.types.name(args[index]) + ">";
        }
    }
    for (const ast::Expr* clause : requirements) {
        if (!in_template_scope(t, args, [&] { return holds(*clause); })) {
            return std::string("requires-clause");
        }
    }
    return std::nullopt;
}

BExprPtr TemplateEngine::call(const ast::CallExpr& call, std::vector<BExprPtr> args, SourceRange range) {
    // Compile-time type questions the standard library is built on: `__cppi_is_same<A, B>()`.
    if (call.callee.starts_with("__cppi_is_") && call.scope.empty() && args.empty()) {
        std::vector<TypeRef> types;
        for (const auto& desc : call.template_args) {
            auto resolved = ctx_.type_resolver->resolve(desc);
            if (!resolved || resolved->is_auto) {
                return nullptr;
            }
            types.push_back(resolved->type);
        }
        if (auto answer = type_trait(call.callee, types)) {
            return ExpressionBinder::constant(TypeTable::kBool, *answer ? 1 : 0, range);
        }
    }
    Symbol* symbol =
        call.scope.empty() ? ctx_.symbols.lookup(call.callee) : ctx_.symbols.lookup_qualified(call.scope, call.callee);
    // Member function templates: `Factory::make<int>()`, or `get<int>(x)` inside the class.
    std::optional<std::uint32_t> member_record;
    if (call.scope.size() == 1 && symbol == nullptr) {
        const Symbol* scope = ctx_.symbols.lookup(call.scope.front());
        if (scope != nullptr && scope->kind == SymbolKind::Type && ctx_.types.is_record(scope->type)) {
            member_record = ctx_.types.info(scope->type).decl;
        }
    } else if (call.scope.empty() && symbol == nullptr && ctx_.fn != nullptr && ctx_.fn->record) {
        member_record = ctx_.fn->record;
    }
    if (member_record && has_member_template(*member_record, call.callee)) {
        auto instances = member_candidates(*member_record, call.callee, call.template_args, args, range);
        const bool with_this = call.scope.empty() && ctx_.fn->has_this;
        if (!with_this) {
            std::erase_if(instances, [&](const Callee& i) { return !ctx_.functions[i.id].is_static; });
            if (instances.empty()) {
                ctx_.report(DiagnosticFactory::template_deduction(call.callee_range, call.callee));
                return nullptr;
            }
            return ctx_.expressions->call_candidates(call.callee, instances, std::move(args), range, call.callee_range,
                                                     nullptr, false);
        }
        auto self = ctx_.expressions->this_object(range);
        if (!self) {
            return nullptr;
        }
        return ctx_.expressions->call_candidates(call.callee, {}, std::move(args), range, call.callee_range,
                                                 std::move(self), true, instances, true);
    }
    const bool class_name =
        symbol != nullptr && ((symbol->kind == SymbolKind::Type && origin(symbol->type)) ||
                              (symbol->kind == SymbolKind::Template && templates_[symbol->templates.front()].is_class));
    if (class_name) {
        // `Box<int>(3)`: a temporary of a class template instance.
        auto type = instantiate_named(symbol, call.template_args, call.callee, call.callee_range);
        if (!type) {
            return nullptr;
        }
        return ctx_.expressions->construct_temporary(*type, std::move(args), range);
    }
    if (symbol == nullptr || symbol->kind != SymbolKind::Template) {
        if (symbol == nullptr || symbol->kind != SymbolKind::Poisoned) {
            ctx_.report(DiagnosticFactory::unknown_function(call.callee_range, call.callee, std::nullopt));
        }
        return nullptr;
    }
    std::vector<TypeRef> explicit_args;
    for (const auto& desc : call.template_args) {
        auto resolved = ctx_.type_resolver->resolve(desc);
        if (!resolved || resolved->is_auto) {
            return nullptr;
        }
        explicit_args.push_back(resolved->type);
    }

    std::vector<Callee> candidates;
    std::optional<std::string> rejected;  // a constraint some candidate did not meet
    std::vector<bool> constrained;        // per candidate: its template has constraints
    for (const std::uint32_t id : symbol->templates) {
        const TemplateInfo& t = templates_[id];
        if (t.is_class || t.is_concept) {
            continue;
        }
        const auto& decl = std::get<ast::FunctionDef>(t.inner->node).decl;
        auto type_args = deduce_all(t, decl, explicit_args, args);
        if (!type_args) {
            continue;
        }
        if (auto why = unsatisfied(templates_[id], *type_args, range)) {
            rejected = std::move(why);
            continue;
        }
        if (auto fid = instantiate_function(id, *type_args, range)) {
            candidates.push_back(Callee{false, *fid});
            constrained.push_back(!t.constrained.empty() || !t.requirements.empty());
        }
    }
    if (candidates.empty()) {
        ctx_.report(rejected ? DiagnosticFactory::constraints_not_satisfied(call.callee_range, call.callee, rejected)
                             : DiagnosticFactory::template_deduction(call.callee_range, call.callee));
        return nullptr;
    }
    // More constrained beats less constrained: an unconstrained candidate with
    // the same parameters as a satisfied constrained one is dropped (simplified
    // partial ordering by constraints).
    std::vector<Callee> preferred;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        bool shadowed = false;
        for (std::size_t j = 0; j < candidates.size() && !constrained[i] && !shadowed; ++j) {
            shadowed = constrained[j] &&
                       ctx_.overloads->param_types(candidates[j]) == ctx_.overloads->param_types(candidates[i]);
        }
        if (!shadowed) {
            preferred.push_back(candidates[i]);
        }
    }
    candidates = std::move(preferred);
    auto chosen = ctx_.overloads->resolve(call.callee, candidates, args, range);
    if (!chosen) {
        return nullptr;
    }
    auto prepared = ctx_.overloads->prepare(*chosen, std::move(args), range);
    if (!prepared) {
        return nullptr;
    }
    return ctx_.expressions->make_call(*chosen, std::move(*prepared), range);
}

BExprPtr TemplateEngine::string_literal(const ast::StringLiteral& literal, SourceRange range) {
    TypeTable& types = ctx_.types;
    const auto cells = static_cast<std::uint32_t>(literal.value.size() + 1);
    const TypeRef type = types.array_of(TypeTable::kChar, cells);
    auto it = literals_.find(literal.value);
    if (it == literals_.end()) {
        BVar storage;
        storage.offset = ctx_.program.global_cells;
        storage.global = true;
        storage.cells = cells;
        ctx_.program.global_cells += cells;
        it = literals_.emplace(literal.value, storage).first;
        // Static storage, filled in before the program starts (no code runs for it).
        ctx_.program.strings.emplace_back(storage.offset, literal.value);
    }
    auto e = ExpressionBinder::make(type, true, range, it->second);
    e->is_const = true;
    return e;
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks): owned by the variant

}  // namespace cppi::sema
