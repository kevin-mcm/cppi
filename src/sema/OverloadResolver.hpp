#pragma once

/// Picks which function a call runs, the way C++ does (simplified): every
/// candidate with the right number of arguments is ranked argument by
/// argument (exact match > promotion > conversion); the best one wins, and a
/// tie is an ambiguous call.

#include "sema/AnalysisContext.hpp"
#include "sema/BoundTree.hpp"
#include "sema/ImplicitConversions.hpp"
#include "sema/Symbol.hpp"

#include <cppi/SourceRange.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

class OverloadResolver {
public:
    explicit OverloadResolver(AnalysisContext& ctx) noexcept : ctx_(ctx) {}

    /// The parameter types of a callee, as declared.
    [[nodiscard]] std::vector<TypeRef> param_types(const Callee& callee) const;
    [[nodiscard]] TypeRef return_type(const Callee& callee) const;
    [[nodiscard]] std::string display_name(const Callee& callee) const;
    /// Number of trailing parameters with default values.
    [[nodiscard]] std::size_t defaults(const Callee& callee) const;

    /// Chooses among `candidates` for the bound `args` (not yet converted).
    /// Reports and returns nullopt if no single best candidate exists.
    [[nodiscard]] std::optional<Callee> resolve(const std::string& name, const std::vector<Callee>& candidates,
                                                const std::vector<BExprPtr>& args, SourceRange call_range);

    /// Turns bound arguments into what the callee receives: converted values
    /// for scalars, addresses for references and records. Missing trailing
    /// arguments are filled from default values.
    [[nodiscard]] std::optional<std::vector<BExprPtr>> prepare(const Callee& callee, std::vector<BExprPtr> args,
                                                               SourceRange call_range);

    /// Conversion rank of one argument for one parameter type.
    [[nodiscard]] ConversionRank rank(const BExpr& arg, TypeRef param) const;
    /// The conversion operator (`operator bool()`...) of `record` that yields
    /// something convertible to `target`.
    [[nodiscard]] std::optional<std::uint32_t> conversion_operator(TypeRef record, TypeRef target,
                                                                   bool allow_explicit) const;
    [[nodiscard]] bool converts_by_operator(TypeRef record, TypeRef target, bool allow_explicit) const;

    /// Can a constructor of `record` build one from `arg` (without another
    /// user-defined conversion)?
    [[nodiscard]] bool converts_by_constructor(const BExpr& arg, TypeRef record) const;

    /// "int, double, Point".
    [[nodiscard]] std::string describe(const std::vector<BExprPtr>& args) const;

private:
    [[nodiscard]] std::optional<Callee> choose(const std::string& name, const std::vector<Callee>& candidates,
                                               const std::vector<BExprPtr>& args, SourceRange call_range);
    void report_single(const std::string& name, const Callee& callee, const std::vector<BExprPtr>& args,
                       SourceRange call_range);

    AnalysisContext& ctx_;
};

}  // namespace cppi::sema
