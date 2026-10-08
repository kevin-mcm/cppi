#pragma once

/// @file TreeConverter.hpp
/// @brief Adapter: converts tree-sitter's concrete syntax tree into cppi's AST.
///
/// Assumes the tree has already passed the SyntaxChecker. Constructs the
/// converter does not understand become ast::Unsupported nodes that list the
/// language features they use, so the analyzer can explain them.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "ast/TranslationUnit.hpp"

#include <tree_sitter/api.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppi::parse {

/// Converts a tree-sitter-cpp syntax tree (already checked for syntax
/// errors) into the AST. Constructs it cannot represent become
/// ast::Unsupported nodes; helpers returning std::optional return nullopt for
/// such constructs.
class TreeConverter {
public:
    /// @param source The source text the tree was parsed from (must outlive the
    /// converter).
    explicit TreeConverter(std::string_view source) noexcept : source_(source) {}

    /// Converts the whole program from the tree's root.
    [[nodiscard]] ast::TranslationUnit convert_unit(TSNode root) const;
    /// Converts one top-level item, appending what it declares or does.
    void convert_into(TSNode node, std::vector<ast::Stmt>& out) const { convert_statement(node, out); }

private:
    // --- Statements ---------------------------------------------------------
    /// Converts a statement or declaration, appending the result(s) to `out`.
    void convert_statement(TSNode node, std::vector<ast::Stmt>& out) const;
    /// A statement node at `node`'s range, with the features the statement uses.
    template <typename Node>
    [[nodiscard]] ast::Stmt make_stmt(TSNode node, Node payload) const;
    /// An Unsupported statement explaining which features `node` uses.
    [[nodiscard]] ast::Stmt unsupported_stmt(TSNode node) const;
    /// The body of an `if`, a loop, etc. as one statement (several become a block).
    [[nodiscard]] ast::StmtPtr convert_child_statement(TSNode node) const;
    /// `{ ... }`.
    [[nodiscard]] ast::Block convert_block(TSNode node) const;
    /// The condition of `if`, `while` or `switch`: expression or declaration.
    [[nodiscard]] ast::Condition convert_condition(TSNode node) const;
    /// `if` / `if constexpr`, with its `else`.
    [[nodiscard]] std::optional<ast::Stmt> convert_if(TSNode node) const;
    /// A classic `for` loop.
    [[nodiscard]] std::optional<ast::Stmt> convert_for(TSNode node) const;
    /// A range-based `for` loop.
    [[nodiscard]] std::optional<ast::Stmt> convert_range_for(TSNode node) const;
    /// `switch` and its case labels.
    [[nodiscard]] std::optional<ast::Stmt> convert_switch(TSNode node) const;
    /// A declaration: variables, a function prototype, a class or an enum.
    void convert_declaration(TSNode node, std::vector<ast::Stmt>& out) const;
    /// A function definition, free or member.
    [[nodiscard]] std::optional<ast::Stmt> convert_function_definition(TSNode node) const;
    /// `template <...>` around a function, class, alias or concept.
    [[nodiscard]] std::optional<ast::Stmt> convert_template(TSNode node) const;
    /// A constraint (`C<T> && D<T>`, `requires ...`) as an expression.
    [[nodiscard]] ast::Expr convert_constraint(TSNode node) const;
    /// A requires-expression.
    [[nodiscard]] ast::Expr convert_requires(TSNode node) const;
    /// True if the (possibly qualified) name ends in a template-id (`f<int>`).
    [[nodiscard]] bool has_template_name(TSNode node) const;
    /// One function parameter.
    [[nodiscard]] std::optional<ast::Param> convert_param(TSNode p) const;
    /// `try` and its `catch` clauses.
    [[nodiscard]] std::optional<ast::Stmt> convert_try(TSNode node) const;
    /// A template parameter list.
    [[nodiscard]] std::optional<std::vector<ast::TemplateParam>> convert_template_params(TSNode node) const;
    /// A member function template inside a class.
    [[nodiscard]] std::optional<ast::MethodTemplate> convert_method_template(TSNode node) const;
    /// `using Name = Type;`.
    [[nodiscard]] std::optional<ast::Stmt> convert_alias(TSNode node) const;
    /// `typedef Type Name;`.
    [[nodiscard]] std::optional<ast::Stmt> convert_typedef(TSNode node) const;

    // --- Declarations -------------------------------------------------------
    /// The type and specifiers (`const`, `static`...) of declaration `decl`.
    [[nodiscard]] std::optional<ast::TypeSpec> convert_type_spec(TSNode decl) const;
    /// A type node alone: a name, a template-id, a qualified name, `auto`...
    [[nodiscard]] std::optional<ast::TypeSpec> convert_type_node(TSNode type) const;
    /// Fills `out` from a declarator: name, pointers, references, array bounds.
    /// Returns false if the declarator is unsupported.
    [[nodiscard]] bool convert_declarator(TSNode node, ast::Declarator& out) const;
    /// One declarator of a variable declaration, with its initializer.
    [[nodiscard]] std::optional<ast::VarDeclarator> convert_var_declarator(TSNode node) const;
    /// A function declaration from its declaration node and function declarator.
    [[nodiscard]] std::optional<ast::FunctionDecl> convert_function_decl(TSNode node, TSNode declarator) const;
    /// The name of a function declarator (`f`, `~T`, `operator+`); false if unsupported.
    bool name_function(TSNode name, ast::FunctionDecl& fn) const;
    /// A complete type as written in a cast, `sizeof` or a template argument.
    [[nodiscard]] std::optional<ast::TypeDesc> convert_type_desc(TSNode node) const;
    /// A template argument list (types only).
    [[nodiscard]] std::optional<std::vector<ast::TypeDesc>> convert_template_args(TSNode list) const;
    /// A `struct` or `class` specifier, with its members.
    [[nodiscard]] std::optional<ast::RecordDef> convert_record(TSNode node) const;
    /// An `enum` or `enum class` specifier.
    [[nodiscard]] std::optional<ast::EnumDef> convert_enum(TSNode node) const;
    /// True if the declarator declares a function.
    [[nodiscard]] bool is_function_declarator(TSNode node) const;
    /// For `T name(args);` (the most vexing parse), the object-declaration
    /// reading; null when the declaration can only be a function.
    [[nodiscard]] std::shared_ptr<ast::DeclStmt> convert_object_reading(TSNode node, TSNode declarator) const;
    /// A parameter declaration read as an expression (`kWidth * kHeight`).
    [[nodiscard]] std::optional<ast::Expr> param_as_expr(TSNode param) const;
    /// A declarator read as an expression.
    [[nodiscard]] std::optional<ast::Expr> declarator_as_expr(TSNode declarator) const;

    // --- Expressions --------------------------------------------------------
    /// Any expression; Unsupported if it cannot be represented.
    [[nodiscard]] ast::Expr convert_expr(TSNode node) const;
    /// convert_expr() on the heap; null for a null node.
    [[nodiscard]] ast::ExprPtr convert_expr_ptr(TSNode node) const;
    /// An Unsupported expression explaining which features `node` uses.
    [[nodiscard]] ast::Expr unsupported_expr(TSNode node) const;
    /// A number literal node.
    [[nodiscard]] ast::Expr convert_number(TSNode node) const;
    /// A number literal from its spelling: integer or floating point.
    [[nodiscard]] ast::Expr convert_number_text(std::string_view spelling, SourceRange range) const;
    /// A call expression.
    [[nodiscard]] ast::Expr convert_call(TSNode node) const;
    /// An identifier, possibly qualified (`Color::Red`).
    [[nodiscard]] std::optional<ast::Expr> convert_name(TSNode node) const;
    /// An argument list.
    [[nodiscard]] std::optional<std::vector<ast::Expr>> convert_arguments(TSNode list) const;
    /// A `new` expression.
    [[nodiscard]] ast::Expr convert_new(TSNode node) const;
    /// A lambda expression.
    [[nodiscard]] ast::Expr convert_lambda(TSNode node) const;

    // --- Helpers ------------------------------------------------------------
    /// The source text of `node`.
    [[nodiscard]] std::string_view text(TSNode node) const noexcept;
    /// The text of `node`'s `operator` field ("" if none).
    [[nodiscard]] std::string_view operator_of(TSNode node) const noexcept;
    /// The characters of a string or character literal body, with escape
    /// sequences resolved; nullopt for an invalid escape.
    [[nodiscard]] static std::optional<std::string> decode_escapes(std::string_view body);

    /// The source text.
    std::string_view source_;
};

}  // namespace cppi::parse
