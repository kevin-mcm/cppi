#pragma once

/// @file OverloadResolver.hpp
/// @brief Picks which function a call runs, the way C++ does (simplified):
/// every candidate with the right number of arguments is ranked argument by
/// argument (exact match > promotion > conversion); the best one wins, and a
/// tie is an ambiguous call.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisContext.hpp"
#include "sema/BoundTree.hpp"
#include "sema/ImplicitConversions.hpp"
#include "sema/Symbol.hpp"

#include <cppi/SourceRange.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

/// Overload resolution and argument preparation for calls.
class OverloadResolver {
public:
    /// @param ctx The shared analysis state.
    explicit OverloadResolver(AnalysisContext& ctx) noexcept : ctx_(ctx) {}

    /// The parameter types of a callee, as declared.
    [[nodiscard]] std::vector<TypeRef> param_types(const Callee& callee) const;
    /// The return type of a callee.
    [[nodiscard]] TypeRef return_type(const Callee& callee) const;
    /// The callee's name for messages ("Robot::move").
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
    /// True if conversion_operator() finds one.
    [[nodiscard]] bool converts_by_operator(TypeRef record, TypeRef target, bool allow_explicit) const;

    /// Can a constructor of `record` build one from `arg` (without another
    /// user-defined conversion)?
    [[nodiscard]] bool converts_by_constructor(const BExpr& arg, TypeRef record) const;

    /// "int, double, Point".
    [[nodiscard]] std::string describe(const std::vector<BExprPtr>& args) const;

private:
    /// The best viable candidate; reports why none (or several) fit.
    [[nodiscard]] std::optional<Callee> choose(const std::string& name, const std::vector<Callee>& candidates,
                                               const std::vector<BExprPtr>& args, SourceRange call_range);
    /// Explains why the only candidate does not accept `args` (count or type).
    void report_single(const std::string& name, const Callee& callee, const std::vector<BExprPtr>& args,
                       SourceRange call_range);

    /// The shared analysis state.
    AnalysisContext& ctx_;
};

}  // namespace cppi::sema
