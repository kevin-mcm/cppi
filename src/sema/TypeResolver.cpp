#include "sema/TypeResolver.hpp"

#include "sema/ExpressionBinder.hpp"
#include "sema/NameSuggester.hpp"
#include "sema/TemplateEngine.hpp"
#include "support/DiagnosticFactory.hpp"

#include <string_view>

namespace cppi::sema {

using detail::DiagnosticFactory;

std::optional<TypeRef> TypeResolver::builtin(const std::string& name) const {
    if (name == "bool") return TypeTable::kBool;
    if (name == "void") return TypeTable::kVoid;
    if (name == "double" || name == "float" || name == "long double") return TypeTable::kDouble;
    // Integer types: the words may come in any order ("long unsigned int").
    int is_unsigned = 0;
    int is_signed = 0;
    int shorts = 0;
    int longs = 0;
    int ints = 0;
    int chars = 0;
    std::size_t start = 0;
    while (start < name.size()) {
        std::size_t end = name.find(' ', start);
        if (end == std::string::npos) {
            end = name.size();
        }
        const std::string_view word(name.data() + start, end - start);
        if (word == "unsigned") {
            ++is_unsigned;
        } else if (word == "signed") {
            ++is_signed;
        } else if (word == "short") {
            ++shorts;
        } else if (word == "long") {
            ++longs;
        } else if (word == "int") {
            ++ints;
        } else if (word == "char") {
            ++chars;
        } else {
            return std::nullopt;
        }
        start = end + 1;
    }
    if (is_unsigned + is_signed > 1 || ints > 1 || chars > 1 || shorts > 1 || longs > 2 || (shorts > 0 && longs > 0) ||
        (chars > 0 && (ints + shorts + longs) > 0)) {
        return std::nullopt;
    }
    const bool u = is_unsigned != 0;
    if (chars > 0) {
        return u ? TypeTable::kUChar : TypeTable::kChar;  // plain char is signed here
    }
    if (shorts > 0) {
        return u ? TypeTable::kUShort : TypeTable::kShort;
    }
    if (longs > 0) {
        return u ? TypeTable::kULong : TypeTable::kLong;  // long and long long are both 64 bits
    }
    if (ints + is_unsigned + is_signed > 0) {
        return u ? TypeTable::kUInt : TypeTable::kInt;
    }
    return std::nullopt;
}

std::optional<ResolvedType> TypeResolver::resolve_spec(const ast::TypeSpec& spec) {
    ResolvedType out;
    out.is_const = spec.is_const;
    if (spec.name == "auto") {
        out.is_auto = true;
        out.type = TypeTable::kError;
        return out;
    }
    if (spec.scope.empty()) {
        if (auto b = builtin(spec.name)) {
            out.type = *b;
            return out;
        }
    }
    if (spec.name == "wchar_t" || spec.name == "char16_t" || spec.name == "char32_t" || spec.name == "char8_t") {
        ctx_.report(DiagnosticFactory::unsupported_type(spec.range, spec.name));
        return std::nullopt;
    }
    if (!spec.template_args.empty()) {
        if (auto t = ctx_.templates->instantiate_type(spec)) {
            out.type = *t;
            return out;
        }
        return std::nullopt;
    }
    Symbol* symbol = nullptr;
    if (!spec.scope.empty()) {
        symbol = ctx_.symbols.lookup_qualified(spec.scope, spec.name);
        if (symbol == nullptr) {
            // `Outer::Inner` for enums nested in nothing else: not supported.
            ctx_.report(
                DiagnosticFactory::unknown_type(spec.range, spec.scope.back() + "::" + spec.name, std::nullopt));
            return std::nullopt;
        }
    } else {
        symbol = ctx_.symbols.lookup(spec.name);
    }
    if (symbol != nullptr && symbol->kind == SymbolKind::Type) {
        out.type = symbol->type;
        return out;
    }
    if (symbol != nullptr && symbol->kind == SymbolKind::Poisoned) {
        return std::nullopt;
    }
    ctx_.report(DiagnosticFactory::unknown_type(
        spec.range, spec.name, NameSuggester::closest(spec.name, ctx_.symbols.visible_names([](const Symbol& s) {
            return s.kind == SymbolKind::Type;
        }))));
    return std::nullopt;
}

std::optional<ResolvedType> TypeResolver::apply(ResolvedType base, const ast::Declarator& declarator) {
    ResolvedType out = base;
    for (const auto& part : declarator.parts) {
        if (ctx_.types.is_reference(out.type) && part.kind != ast::DeclaratorKind::Reference) {
            ctx_.report(DiagnosticFactory::declaration_not_allowed(part.range, "pointer or array of references"));
            return std::nullopt;
        }
        switch (part.kind) {
            case ast::DeclaratorKind::Pointer:
                out.type = ctx_.types.pointer_to(out.type, out.is_const);
                out.is_const = part.is_const;
                break;
            case ast::DeclaratorKind::Reference:
            case ast::DeclaratorKind::RvalueReference:
                if (ctx_.types.is_reference(out.type)) {
                    ctx_.report(DiagnosticFactory::declaration_not_allowed(part.range, "reference to reference"));
                    return std::nullopt;
                }
                out.type =
                    ctx_.types.reference_to(out.type, out.is_const, part.kind == ast::DeclaratorKind::RvalueReference);
                out.is_const = false;
                break;
            case ast::DeclaratorKind::Array: {
                std::uint32_t count = 0;
                if (part.size) {
                    auto size = ctx_.expressions->bind_rvalue(*part.size);
                    if (!size) {
                        return std::nullopt;
                    }
                    auto value = ctx_.constants.integer(*size);
                    if (!value || !ctx_.types.is_integral(size->type)) {
                        ctx_.report(DiagnosticFactory::not_constant(part.size->range));
                        return std::nullopt;
                    }
                    const std::uint64_t element = std::max<std::uint32_t>(1, ctx_.types.cells(out.type));
                    if (*value <= 0 || static_cast<std::uint64_t>(*value) * element > kMaxObjectCells) {
                        ctx_.report(DiagnosticFactory::invalid_array_size(part.size->range, value));
                        return std::nullopt;
                    }
                    count = static_cast<std::uint32_t>(*value);
                }
                if (ctx_.types.is_void(out.type)) {
                    ctx_.report(DiagnosticFactory::declaration_not_allowed(part.range, "array of void"));
                    return std::nullopt;
                }
                out.type = ctx_.types.array_of(out.type, count);
                break;
            }
        }
    }
    return out;
}

std::optional<ResolvedType> TypeResolver::resolve(const ast::TypeSpec& spec, const ast::Declarator& declarator) {
    auto base = resolve_spec(spec);
    if (!base) {
        return std::nullopt;
    }
    if (base->is_auto) {
        // `auto`, `auto&`, `const auto&` and `auto*` are deduced by the caller.
        const bool simple = declarator.parts.empty() ||
                            (declarator.parts.size() == 1 && declarator.parts[0].kind != ast::DeclaratorKind::Array);
        if (!simple) {
            ctx_.report(DiagnosticFactory::unsupported_type(spec.range, "auto with [] or several * &"));
            return std::nullopt;
        }
        return base;
    }
    return apply(*base, declarator);
}

TypeRef TypeResolver::adjust_parameter(const ResolvedType& resolved) {
    if (ctx_.types.is_array(resolved.type)) {
        return ctx_.types.pointer_to(ctx_.types.info(resolved.type).target, resolved.is_const);
    }
    return resolved.type;
}

}  // namespace cppi::sema
