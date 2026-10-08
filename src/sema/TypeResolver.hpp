#pragma once

/// Turns the types written in the source (`const int* p[3]`) into TypeRefs.

#include "sema/AnalysisContext.hpp"
#include "sema/TypeInfo.hpp"

#include "ast/Expr.hpp"

#include <cstdint>
#include <optional>

namespace cppi::sema {

struct ResolvedType {
    TypeRef type = 0;
    bool is_const = false;  ///< the declared object itself is const (for arrays: its elements)
    bool is_auto = false;   ///< `auto`: the caller deduces the type from the initializer
};

class TypeResolver {
public:
    /// Largest object the VM can hold, in cells.
    static constexpr std::uint32_t kMaxObjectCells = 1U << 20;

    explicit TypeResolver(AnalysisContext& ctx) noexcept : ctx_(ctx) {}

    /// The base type named by `spec`, or nullopt after reporting why.
    [[nodiscard]] std::optional<ResolvedType> resolve_spec(const ast::TypeSpec& spec);

    /// Applies a declarator's pointers, references and array bounds.
    /// Arrays without a bound get count 0 (deduced from an initializer).
    [[nodiscard]] std::optional<ResolvedType> apply(ResolvedType base, const ast::Declarator& declarator);

    [[nodiscard]] std::optional<ResolvedType> resolve(const ast::TypeSpec& spec, const ast::Declarator& declarator);
    [[nodiscard]] std::optional<ResolvedType> resolve(const ast::TypeDesc& desc) {
        return resolve(desc.spec, desc.declarator);
    }

    /// A parameter's type: arrays and functions decay to pointers.
    [[nodiscard]] TypeRef adjust_parameter(const ResolvedType& resolved);

private:
    [[nodiscard]] std::optional<TypeRef> builtin(const std::string& name) const;

    AnalysisContext& ctx_;
};

}  // namespace cppi::sema
