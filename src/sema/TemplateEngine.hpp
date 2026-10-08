#pragma once

/// @file TemplateEngine.hpp
/// @brief Templates: function templates (with argument deduction) and class
/// templates, instantiated on demand where they were declared, with the
/// template parameters bound to concrete types.
///
/// Each instance is an ordinary function or class afterwards.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisContext.hpp"
#include "sema/BoundTree.hpp"
#include "sema/TemplateInfo.hpp"

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

/// Declares templates and instantiates them on demand (functions, classes,
/// member function templates, generic lambdas), and evaluates concepts.
class TemplateEngine {
public:
    /// @param ctx The shared analysis state.
    explicit TemplateEngine(AnalysisContext& ctx) noexcept : ctx_(ctx) {}

    /// Registers `template <...> ...` (a function or class template).
    bool declare(const ast::TemplateDecl& decl, SourceRange range);
    /// The definition outside the class of a member function template.
    bool define_member(const ast::TemplateDecl& decl, const ast::FunctionDecl& fn, SourceRange range);
    /// The call operator of the generic lambda class `record`.
    void declare_generic_lambda(std::uint32_t record, const ast::FunctionDecl& decl, std::size_t autos,
                                std::vector<Scope*> home, bool is_const, bool deduce_return);
    /// Registers a member function template of `record`.
    void declare_member(std::uint32_t record, const ast::MethodTemplate& method);
    /// Instances of the member function templates `name` of `record` (or of
    /// its closest base that has them) that can take `args`.
    [[nodiscard]] std::vector<Callee> member_candidates(std::uint32_t record, const std::string& name,
                                                        const std::vector<ast::TypeDesc>& explicit_descs,
                                                        const std::vector<BExprPtr>& args, SourceRange range);
    /// Does `record` (or a base) have member function templates called `name`?
    [[nodiscard]] bool has_member_template(std::uint32_t record, std::string_view name) const;

    /// `Box<int>`, `std::vector<int>`.
    [[nodiscard]] std::optional<TypeRef> instantiate_type(const ast::TypeSpec& spec);
    /// `max<int>(a, b)` or `max(a, b)` for a function template.
    [[nodiscard]] BExprPtr call(const ast::CallExpr& call, std::vector<BExprPtr> args, SourceRange range);
    /// "text": an array of chars in static storage.
    [[nodiscard]] BExprPtr string_literal(const ast::StringLiteral& literal, SourceRange range);

    // --- C++20 concepts -------------------------------------------------------
    /// `Number<int>` as a value: true or false, known at compile time.
    [[nodiscard]] BExprPtr concept_value(const ast::CallExpr& id, SourceRange range);
    /// Does `first` (followed by `spec`'s own template arguments) satisfy the
    /// concept named by `spec`? `{ e } -> std::convertible_to<T>` asks this.
    [[nodiscard]] std::optional<bool> concept_holds(const ast::TypeSpec& spec, TypeRef first, SourceRange range);
    /// Binds `expr` as a condition without reporting anything: true only if it
    /// is valid and constantly true.
    [[nodiscard]] bool holds(const ast::Expr& expr);

    /// `__cppi_is_same<A, B>()`, `__cppi_is_integral<T>()`...: the answer, or nullopt if `name` is not one.
    [[nodiscard]] std::optional<bool> type_trait(const std::string& name, const std::vector<TypeRef>& types) const;

    /// The class template and arguments `type` was instantiated from.
    [[nodiscard]] std::optional<std::pair<std::uint32_t, std::vector<TypeRef>>> origin(TypeRef type) const;

private:
    /// Template parameter name -> deduced type.
    using Bindings = std::map<std::string, TypeRef, std::less<>>;

    /// The instance of function template `id` for `args` (made on first use).
    [[nodiscard]] std::optional<std::uint32_t> instantiate_function(std::uint32_t id, const std::vector<TypeRef>& args,
                                                                    SourceRange range);
    /// The class type `name<descs...>` names, through `symbol`.
    [[nodiscard]] std::optional<TypeRef> instantiate_named(Symbol* symbol, const std::vector<ast::TypeDesc>& descs,
                                                           const std::string& name, SourceRange range);
    /// The instance of class template `id` for `args` (made on first use).
    [[nodiscard]] std::optional<TypeRef> instantiate_class(std::uint32_t id, const std::vector<TypeRef>& args,
                                                           SourceRange range);
    /// The instance of member function template `id` for `args`.
    [[nodiscard]] std::optional<std::uint32_t> instantiate_method(std::uint32_t id, const std::vector<TypeRef>& args,
                                                                  SourceRange range);
    /// Template ids of the member function templates `name` visible in `record`.
    [[nodiscard]] const std::vector<std::uint32_t>* member_templates(std::uint32_t record, std::string_view name) const;
    /// Deduces the template arguments of a function template for `args`.
    [[nodiscard]] std::optional<std::vector<TypeRef>> deduce_all(const TemplateInfo& t, const ast::FunctionDecl& decl,
                                                                 const std::vector<TypeRef>& explicit_args,
                                                                 const std::vector<BExprPtr>& args);
    /// The concept template a name refers to, or nullopt.
    [[nodiscard]] std::optional<std::uint32_t> find_concept(const std::vector<std::string>& scope,
                                                            const std::string& name);
    /// True if concept `id` holds for `args` (memoized).
    [[nodiscard]] bool evaluate_concept(std::uint32_t id, const std::vector<TypeRef>& args);
    /// Why `args` do not meet `t`'s constraints ("Number", "requires-clause"), or nullopt if they do.
    [[nodiscard]] std::optional<std::string> unsatisfied(const TemplateInfo& t, const std::vector<TypeRef>& args,
                                                         SourceRange range);
    /// Collects a template's constraints; false (after reporting) if one does not name a concept.
    bool collect_constraints(TemplateInfo& info, const std::vector<ast::TemplateParam>& params,
                             const ast::Expr* requires_clause, const ast::Expr* trailing, SourceRange range);
    /// While muted (a trial), an instance whose code had errors is forgotten,
    /// so a real use later instantiates it again and reports them.
    template <class Cache, class Key>
    void forget_if_failed(Cache& cache, const Key& key, std::size_t errors_before) const;
    /// The default of the template parameter after `previous` (`class U = int`).
    [[nodiscard]] std::optional<TypeRef> default_arg(const TemplateInfo& t, const std::vector<TypeRef>& previous);
    /// Deduces template arguments from one parameter and its argument.
    bool deduce(const TemplateInfo& t, const ast::TypeSpec& spec, const ast::Declarator& declarator, const BExpr& arg,
                Bindings& bindings) const;
    /// Matches the written type `spec` against `actual`, binding template parameters.
    bool unify(const TemplateInfo& t, const ast::TypeSpec& spec, TypeRef actual, Bindings& bindings) const;
    /// "name<int, double>", for messages.
    [[nodiscard]] std::string display(const TemplateInfo& t, const std::vector<TypeRef>& args) const;
    /// Runs `body` with only the template's home scopes and its parameters visible.
    template <typename Body>
    auto in_template_scope(const TemplateInfo& t, const std::vector<TypeRef>& args, Body body);

    /// The shared analysis state.
    AnalysisContext& ctx_;
    /// Every template, by id.
    std::deque<TemplateInfo> templates_;
    /// Class instances -> (template id, arguments).
    std::map<TypeRef, std::pair<std::uint32_t, std::vector<TypeRef>>> origins_;
    /// String literals already placed in static storage, by text.
    std::map<std::string, BVar, std::less<>> literals_;
    int concept_depth_ = 0;  ///< recursion guard
};

}  // namespace cppi::sema
