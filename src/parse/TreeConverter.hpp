#pragma once

/// Adapter: converts tree-sitter's concrete syntax tree into cppi's AST.
/// Assumes the tree has already passed the SyntaxChecker. Constructs the
/// converter does not understand become ast::Unsupported nodes that list the
/// language features they use, so the analyzer can explain them.

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "ast/TranslationUnit.hpp"

#include <tree_sitter/api.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppi::parse {

class TreeConverter {
public:
    explicit TreeConverter(std::string_view source) noexcept : source_(source) {}

    [[nodiscard]] ast::TranslationUnit convert_unit(TSNode root) const;
    /// Converts one top-level item, appending what it declares or does.
    void convert_into(TSNode node, std::vector<ast::Stmt>& out) const { convert_statement(node, out); }

private:
    // --- Statements ---------------------------------------------------------
    void convert_statement(TSNode node, std::vector<ast::Stmt>& out) const;
    template <typename Node>
    [[nodiscard]] ast::Stmt make_stmt(TSNode node, Node payload) const;
    [[nodiscard]] ast::Stmt unsupported_stmt(TSNode node) const;
    [[nodiscard]] ast::StmtPtr convert_child_statement(TSNode node) const;
    [[nodiscard]] ast::Block convert_block(TSNode node) const;
    [[nodiscard]] ast::Condition convert_condition(TSNode node) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_if(TSNode node) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_for(TSNode node) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_range_for(TSNode node) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_switch(TSNode node) const;
    void convert_declaration(TSNode node, std::vector<ast::Stmt>& out) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_function_definition(TSNode node) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_template(TSNode node) const;
    [[nodiscard]] ast::Expr convert_constraint(TSNode node) const;
    [[nodiscard]] ast::Expr convert_requires(TSNode node) const;
    [[nodiscard]] bool has_template_name(TSNode node) const;
    [[nodiscard]] std::optional<ast::Param> convert_param(TSNode p) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_try(TSNode node) const;
    [[nodiscard]] std::optional<std::vector<ast::TemplateParam>> convert_template_params(TSNode node) const;
    [[nodiscard]] std::optional<ast::MethodTemplate> convert_method_template(TSNode node) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_alias(TSNode node) const;
    [[nodiscard]] std::optional<ast::Stmt> convert_typedef(TSNode node) const;

    // --- Declarations -------------------------------------------------------
    [[nodiscard]] std::optional<ast::TypeSpec> convert_type_spec(TSNode decl) const;
    [[nodiscard]] std::optional<ast::TypeSpec> convert_type_node(TSNode type) const;
    [[nodiscard]] bool convert_declarator(TSNode node, ast::Declarator& out) const;
    [[nodiscard]] std::optional<ast::VarDeclarator> convert_var_declarator(TSNode node) const;
    [[nodiscard]] std::optional<ast::FunctionDecl> convert_function_decl(TSNode node, TSNode declarator) const;
    /// The name of a function declarator (`f`, `~T`, `operator+`); false if unsupported.
    bool name_function(TSNode name, ast::FunctionDecl& fn) const;
    [[nodiscard]] std::optional<ast::TypeDesc> convert_type_desc(TSNode node) const;
    [[nodiscard]] std::optional<std::vector<ast::TypeDesc>> convert_template_args(TSNode list) const;
    [[nodiscard]] std::optional<ast::RecordDef> convert_record(TSNode node) const;
    [[nodiscard]] std::optional<ast::EnumDef> convert_enum(TSNode node) const;
    [[nodiscard]] bool is_function_declarator(TSNode node) const;
    [[nodiscard]] std::shared_ptr<ast::DeclStmt> convert_object_reading(TSNode node, TSNode declarator) const;
    [[nodiscard]] std::optional<ast::Expr> param_as_expr(TSNode param) const;
    [[nodiscard]] std::optional<ast::Expr> declarator_as_expr(TSNode declarator) const;

    // --- Expressions --------------------------------------------------------
    [[nodiscard]] ast::Expr convert_expr(TSNode node) const;
    [[nodiscard]] ast::ExprPtr convert_expr_ptr(TSNode node) const;
    [[nodiscard]] ast::Expr unsupported_expr(TSNode node) const;
    [[nodiscard]] ast::Expr convert_number(TSNode node) const;
    [[nodiscard]] ast::Expr convert_number_text(std::string_view spelling, SourceRange range) const;
    [[nodiscard]] ast::Expr convert_call(TSNode node) const;
    [[nodiscard]] std::optional<ast::Expr> convert_name(TSNode node) const;
    [[nodiscard]] std::optional<std::vector<ast::Expr>> convert_arguments(TSNode list) const;
    [[nodiscard]] ast::Expr convert_new(TSNode node) const;
    [[nodiscard]] ast::Expr convert_lambda(TSNode node) const;

    // --- Helpers ------------------------------------------------------------
    [[nodiscard]] std::string_view text(TSNode node) const noexcept;
    [[nodiscard]] std::string_view operator_of(TSNode node) const noexcept;
    [[nodiscard]] static std::optional<std::string> decode_escapes(std::string_view body);

    std::string_view source_;
};

}  // namespace cppi::parse
