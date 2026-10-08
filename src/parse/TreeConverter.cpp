/// @file TreeConverter.cpp
/// @brief Implementation of TreeConverter.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "parse/TreeConverter.hpp"

#include "parse/FeatureScanner.hpp"
#include "parse/IntegerLiteralParser.hpp"
#include "parse/NodeFeatureMap.hpp"
#include "parse/TreeSitterUtils.hpp"
#include "support/NumberFormat.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace cppi::parse {

namespace {

TSNode field(TSNode node, const char* name) noexcept {
    return ts_node_child_by_field_name(node, name, static_cast<std::uint32_t>(std::strlen(name)));
}

bool has_field(TSNode node, const char* name) noexcept {
    return !ts_node_is_null(field(node, name));
}

/// Named children that are not comments.
std::vector<TSNode> named_children(TSNode node) {
    std::vector<TSNode> out;
    const std::uint32_t count = ts_node_named_child_count(node);
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const TSNode child = ts_node_named_child(node, i);
        if (!is_comment(child)) {
            out.push_back(child);
        }
    }
    return out;
}

/// Every child (named or not) with the given field name, in order.
std::vector<TSNode> children_with_field(TSNode node, std::string_view name) {
    std::vector<TSNode> out;
    const std::uint32_t count = ts_node_child_count(node);
    for (std::uint32_t i = 0; i < count; ++i) {
        const char* field_name = ts_node_field_name_for_child(node, i);
        if (field_name != nullptr && name == field_name) {
            out.push_back(ts_node_child(node, i));
        }
    }
    return out;
}

/// True if any direct child (named or anonymous) has kind `kind`.
bool has_child_kind(TSNode node, std::string_view kind) noexcept {
    const std::uint32_t count = ts_node_child_count(node);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (kind_of(ts_node_child(node, i)) == kind) {
            return true;
        }
    }
    return false;
}

std::optional<ast::BinaryOp> binary_op(std::string_view op) noexcept {
    using ast::BinaryOp;
    if (op == "+") return BinaryOp::Add;
    if (op == "-") return BinaryOp::Sub;
    if (op == "*") return BinaryOp::Mul;
    if (op == "/") return BinaryOp::Div;
    if (op == "%") return BinaryOp::Mod;
    if (op == "<<") return BinaryOp::Shl;
    if (op == ">>") return BinaryOp::Shr;
    if (op == "&" || op == "bitand") return BinaryOp::BitAnd;
    if (op == "|" || op == "bitor") return BinaryOp::BitOr;
    if (op == "^" || op == "xor") return BinaryOp::BitXor;
    if (op == "&&" || op == "and") return BinaryOp::LogicalAnd;
    if (op == "||" || op == "or") return BinaryOp::LogicalOr;
    if (op == "==") return BinaryOp::Eq;
    if (op == "!=" || op == "not_eq") return BinaryOp::Ne;
    if (op == "<") return BinaryOp::Lt;
    if (op == ">") return BinaryOp::Gt;
    if (op == "<=") return BinaryOp::Le;
    if (op == ">=") return BinaryOp::Ge;
    return std::nullopt;
}

std::optional<ast::BinaryOp> compound_assign_op(std::string_view op) noexcept {
    if (op.size() < 2 || op.back() != '=') {
        return std::nullopt;
    }
    if (op == "and_eq") return ast::BinaryOp::BitAnd;
    if (op == "or_eq") return ast::BinaryOp::BitOr;
    if (op == "xor_eq") return ast::BinaryOp::BitXor;
    return binary_op(op.substr(0, op.size() - 1));
}

std::string collapse_spaces(std::string_view text) {
    std::string out;
    bool space = false;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            space = !out.empty();
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out.push_back(c);
    }
    return out;
}

ast::Access access_from(std::string_view text) noexcept {
    if (text == "public") {
        return ast::Access::Public;
    }
    return text == "protected" ? ast::Access::Protected : ast::Access::Private;
}

bool is_name_kind(std::string_view kind) noexcept {
    return kind == "identifier" || kind == "field_identifier" || kind == "type_identifier" ||
           kind == "namespace_identifier";
}

}  // namespace

// =============================================================================
// Statements
// =============================================================================

ast::TranslationUnit TreeConverter::convert_unit(TSNode root) const {
    ast::TranslationUnit unit;
    for (const TSNode child : named_children(root)) {
        convert_statement(child, unit.statements);
    }
    return unit;
}

template <typename Node>
ast::Stmt TreeConverter::make_stmt(TSNode node, Node payload) const {
    ast::Stmt stmt;
    stmt.range = range_of(node);
    stmt.uses = FeatureScanner::scan_own(node, source_);
    stmt.node = std::move(payload);
    return stmt;
}

ast::Stmt TreeConverter::unsupported_stmt(TSNode node) const {
    return make_stmt(node, FeatureScanner::scan(node, source_));
}

void TreeConverter::convert_statement(TSNode node, std::vector<ast::Stmt>& out) const {
    const std::string_view kind = kind_of(node);
    std::optional<ast::Stmt> stmt;

    if (kind == "expression_statement") {
        auto inner = first_named_child(node);
        if (!inner) {
            return;  // empty statement `;`
        }
        stmt = make_stmt(node, ast::ExprStmt{convert_expr(*inner)});
    } else if (kind == "compound_statement") {
        stmt = make_stmt(node, convert_block(node));
    } else if (kind == "declaration") {
        convert_declaration(node, out);
        return;
    } else if (kind == "function_definition") {
        stmt = convert_function_definition(node);
    } else if (kind == "if_statement") {
        stmt = convert_if(node);
    } else if (kind == "while_statement") {
        ast::WhileStmt w;
        w.condition = convert_condition(field(node, "condition"));
        w.body = convert_child_statement(field(node, "body"));
        stmt = make_stmt(node, std::move(w));
    } else if (kind == "do_statement") {
        ast::DoWhileStmt d;
        d.body = convert_child_statement(field(node, "body"));
        d.condition = convert_expr_ptr(field(node, "condition"));
        stmt = make_stmt(node, std::move(d));
    } else if (kind == "for_statement") {
        stmt = convert_for(node);
    } else if (kind == "for_range_loop") {
        stmt = convert_range_for(node);
    } else if (kind == "break_statement") {
        stmt = make_stmt(node, ast::BreakStmt{});
    } else if (kind == "continue_statement") {
        stmt = make_stmt(node, ast::ContinueStmt{});
    } else if (kind == "throw_statement") {
        ast::ThrowStmt t;
        if (auto value = first_named_child(node)) {
            t.value = convert_expr_ptr(*value);
        }
        stmt = make_stmt(node, std::move(t));
    } else if (kind == "try_statement") {
        stmt = convert_try(node);
    } else if (kind == "concept_definition") {
        const TSNode name = field(node, "name");
        const auto children = named_children(node);
        if (!ts_node_is_null(name) && children.size() == 2) {
            ast::ConceptDef c;
            c.name = std::string(text(name));
            c.name_range = range_of(name);
            c.constraint = std::make_unique<ast::Expr>(convert_constraint(children.back()));
            stmt = make_stmt(node, std::move(c));
        }
    } else if (kind == "return_statement") {
        ast::ReturnStmt r;
        if (auto value = first_named_child(node)) {
            r.value = convert_expr_ptr(*value);
        }
        stmt = make_stmt(node, std::move(r));
    } else if (kind == "switch_statement") {
        stmt = convert_switch(node);
    } else if (kind == "struct_specifier" || kind == "class_specifier") {
        if (auto record = convert_record(node)) {
            stmt = make_stmt(node, std::move(*record));
        }
    } else if (kind == "enum_specifier") {
        if (auto e = convert_enum(node)) {
            stmt = make_stmt(node, std::move(*e));
        }
    } else if (kind == "type_definition") {
        stmt = convert_typedef(node);
    } else if (kind == "alias_declaration") {
        stmt = convert_alias(node);
    } else if (kind == "template_declaration") {
        stmt = convert_template(node);
    } else if (kind == "preproc_include") {
        // The standard library is always available: `#include <vector>` is a no-op.
        stmt = make_stmt(node, ast::Block{});
    } else if (kind == "namespace_definition") {
        const TSNode name = field(node, "name");
        const TSNode body = field(node, "body");
        if (!ts_node_is_null(name) && kind_of(name) == "namespace_identifier" && !ts_node_is_null(body)) {
            ast::NamespaceDef ns;
            ns.name = std::string(text(name));
            ns.name_range = range_of(name);
            for (const TSNode child : named_children(body)) {
                convert_statement(child, ns.body);
            }
            stmt = make_stmt(node, std::move(ns));
        }
    } else if (kind == "using_declaration") {
        ast::UsingDecl u;
        u.directive = has_child_kind(node, "namespace");
        auto target = first_named_child(node);
        if (target) {
            TSNode current = *target;
            bool ok = true;
            while (kind_of(current) == "qualified_identifier") {
                const TSNode scope = field(current, "scope");
                if (ts_node_is_null(scope) || !is_name_kind(kind_of(scope))) {
                    ok = false;
                    break;
                }
                u.scope.emplace_back(text(scope));
                current = field(current, "name");
            }
            if (ok && !ts_node_is_null(current) && is_name_kind(kind_of(current))) {
                u.name = std::string(text(current));
                stmt = make_stmt(node, std::move(u));
            }
        }
    } else if (kind == "static_assert_declaration") {
        ast::StaticAssert sa;
        sa.condition = convert_expr_ptr(field(node, "condition"));
        const TSNode message = field(node, "message");
        if (!ts_node_is_null(message)) {
            auto decoded = convert_expr(message);
            if (auto* str = std::get_if<ast::StringLiteral>(&decoded.node)) {
                sa.message = str->value;
            }
        }
        stmt = make_stmt(node, std::move(sa));
    }

    out.push_back(stmt ? std::move(*stmt) : unsupported_stmt(node));
}

ast::StmtPtr TreeConverter::convert_child_statement(TSNode node) const {
    std::vector<ast::Stmt> stmts;
    if (!ts_node_is_null(node)) {
        convert_statement(node, stmts);
    }
    if (stmts.size() == 1) {
        return std::make_unique<ast::Stmt>(std::move(stmts.front()));
    }
    // `while (x);` has an empty body; a declaration that also defines a
    // struct yields two statements. Either way, wrap them in a block.
    ast::Block block{std::move(stmts)};
    auto wrapper = std::make_unique<ast::Stmt>();
    wrapper->range = ts_node_is_null(node) ? SourceRange{} : range_of(node);
    wrapper->node = std::move(block);
    return wrapper;
}

ast::Block TreeConverter::convert_block(TSNode node) const {
    ast::Block block;
    for (const TSNode child : named_children(node)) {
        convert_statement(child, block.statements);
    }
    return block;
}

ast::Condition TreeConverter::convert_condition(TSNode node) const {
    ast::Condition condition;
    if (ts_node_is_null(node)) {
        return condition;
    }
    condition.range = range_of(node);
    if (kind_of(node) != "condition_clause") {
        condition.expr = convert_expr_ptr(node);
        return condition;
    }
    if (has_field(node, "initializer")) {  // C++17 `if (init; cond)`
        condition.expr = std::make_unique<ast::Expr>(unsupported_expr(node));
        return condition;
    }
    const TSNode value = field(node, "value");
    if (ts_node_is_null(value)) {
        condition.expr = std::make_unique<ast::Expr>(unsupported_expr(node));
        return condition;
    }
    if (kind_of(value) == "declaration") {
        auto type = convert_type_spec(value);
        ast::Declarator declarator;
        const TSNode name = field(value, "declarator");
        const TSNode init = field(value, "value");
        if (!type || ts_node_is_null(name) || ts_node_is_null(init) || !convert_declarator(name, declarator)) {
            condition.expr = std::make_unique<ast::Expr>(unsupported_expr(value));
            return condition;
        }
        auto decl = std::make_unique<ast::DeclStmt>();
        decl->type = std::move(*type);
        ast::VarDeclarator var;
        var.declarator = std::move(declarator);
        var.range = range_of(value);
        if (kind_of(init) == "initializer_list") {
            var.style = ast::InitStyle::List;
        } else {
            var.style = ast::InitStyle::Copy;
        }
        var.init = convert_expr_ptr(init);
        decl->vars.push_back(std::move(var));
        condition.decl = std::move(decl);
        return condition;
    }
    condition.expr = convert_expr_ptr(value);
    return condition;
}

std::optional<ast::Stmt> TreeConverter::convert_if(TSNode node) const {
    ast::IfStmt s;
    s.is_constexpr = has_child_kind(node, "constexpr");
    s.condition = convert_condition(field(node, "condition"));
    s.then_stmt = convert_child_statement(field(node, "consequence"));
    const TSNode alternative = field(node, "alternative");
    if (!ts_node_is_null(alternative)) {
        const TSNode inner = kind_of(alternative) == "else_clause" ? ts_node_named_child(alternative, 0) : alternative;
        s.else_stmt = convert_child_statement(inner);
    }
    return make_stmt(node, std::move(s));
}

std::optional<ast::Stmt> TreeConverter::convert_for(TSNode node) const {
    ast::ForStmt f;
    const TSNode init = field(node, "initializer");
    if (!ts_node_is_null(init)) {
        std::vector<ast::Stmt> stmts;
        if (kind_of(init) == "declaration") {
            convert_declaration(init, stmts);
        } else {
            stmts.push_back(make_stmt(init, ast::ExprStmt{convert_expr(init)}));
        }
        if (stmts.size() != 1) {
            return std::nullopt;
        }
        f.init = std::make_unique<ast::Stmt>(std::move(stmts.front()));
    }
    const TSNode condition = field(node, "condition");
    if (!ts_node_is_null(condition)) {
        f.condition = convert_expr_ptr(condition);
    }
    const TSNode update = field(node, "update");
    if (!ts_node_is_null(update)) {
        f.update = convert_expr_ptr(update);
    }
    f.body = convert_child_statement(field(node, "body"));
    return make_stmt(node, std::move(f));
}

std::optional<ast::Stmt> TreeConverter::convert_range_for(TSNode node) const {
    ast::RangeForStmt r;
    auto type = convert_type_spec(node);
    const TSNode declarator = field(node, "declarator");
    const TSNode right = field(node, "right");
    if (!type || ts_node_is_null(declarator) || ts_node_is_null(right) || has_field(node, "initializer")) {
        return std::nullopt;
    }
    r.variable.type = std::move(*type);
    ast::VarDeclarator var;
    if (!convert_declarator(declarator, var.declarator)) {
        return std::nullopt;
    }
    var.range = range_of(declarator);
    r.variable.vars.push_back(std::move(var));
    r.range_expr = convert_expr_ptr(right);
    r.body = convert_child_statement(field(node, "body"));
    return make_stmt(node, std::move(r));
}

std::optional<ast::Stmt> TreeConverter::convert_switch(TSNode node) const {
    ast::SwitchStmt s;
    s.condition = convert_condition(field(node, "condition"));
    const TSNode body = field(node, "body");
    if (ts_node_is_null(body)) {
        return std::nullopt;
    }
    for (const TSNode child : named_children(body)) {
        if (kind_of(child) != "case_statement") {
            return std::nullopt;  // statements before the first label are unreachable
        }
        ast::SwitchCase c;
        c.range = range_of(child);
        const TSNode value = field(child, "value");
        if (!ts_node_is_null(value)) {
            c.value = convert_expr_ptr(value);
        }
        for (const TSNode stmt : named_children(child)) {
            if (!ts_node_is_null(value) && ts_node_eq(stmt, value)) {
                continue;
            }
            convert_statement(stmt, c.statements);
        }
        s.cases.push_back(std::move(c));
    }
    return make_stmt(node, std::move(s));
}

void TreeConverter::convert_declaration(TSNode node, std::vector<ast::Stmt>& out) const {
    const TSNode type_node = field(node, "type");
    // `struct P { ... } p;` defines the type and declares a variable.
    if (!ts_node_is_null(type_node) && has_field(type_node, "body")) {
        const std::string_view type_kind = kind_of(type_node);
        if (type_kind == "struct_specifier" || type_kind == "class_specifier" || type_kind == "enum_specifier") {
            convert_statement(type_node, out);
        }
    }

    const auto declarators = children_with_field(node, "declarator");
    if (declarators.size() == 1 && is_function_declarator(declarators.front())) {
        if (auto fn = convert_function_decl(node, declarators.front())) {
            fn->object_reading = convert_object_reading(node, declarators.front());
            out.push_back(make_stmt(node, ast::FunctionDef{std::move(*fn)}));
            return;
        }
        out.push_back(unsupported_stmt(node));
        return;
    }

    auto type = convert_type_spec(node);
    if (!type) {
        out.push_back(unsupported_stmt(node));
        return;
    }
    ast::DeclStmt decl;
    decl.type = std::move(*type);
    for (const TSNode d : declarators) {
        if (is_function_declarator(d)) {
            out.push_back(unsupported_stmt(node));
            return;
        }
        auto var = convert_var_declarator(d);
        if (!var) {
            out.push_back(unsupported_stmt(node));
            return;
        }
        decl.vars.push_back(std::move(*var));
    }
    if (decl.vars.empty()) {
        return;  // `struct P { ... };` written as a declaration
    }
    out.push_back(make_stmt(node, std::move(decl)));
}

std::optional<ast::Stmt> TreeConverter::convert_function_definition(TSNode node) const {
    auto fn = convert_function_decl(node, field(node, "declarator"));
    if (!fn) {
        return std::nullopt;
    }
    // `void show(auto x)` (C++20) is `template <class auto#0> void show(auto#0 x)`.
    ast::TemplateDecl t;
    for (ast::Param& p : fn->params) {
        if (p.type.name != "auto") {
            continue;
        }
        ast::TemplateParam invented;
        invented.name = "auto#" + std::to_string(t.params.size());
        invented.range = p.type.range;
        if (p.type.auto_constraint) {
            invented.constraint = std::move(*p.type.auto_constraint);
        }
        p.type.name = invented.name;
        p.type.auto_constraint.reset();
        t.params.push_back(std::move(invented));
    }
    if (t.params.empty()) {
        return make_stmt(node, ast::FunctionDef{std::move(*fn)});
    }
    t.inner = std::make_unique<ast::Stmt>(make_stmt(node, ast::FunctionDef{std::move(*fn)}));
    return make_stmt(node, std::move(t));
}

std::optional<std::vector<ast::TemplateParam>> TreeConverter::convert_template_params(TSNode node) const {
    const TSNode params = field(node, "parameters");
    if (ts_node_is_null(params)) {
        return std::nullopt;
    }
    std::vector<ast::TemplateParam> out;
    for (const TSNode p : named_children(params)) {
        if (kind_of(p) == "optional_type_parameter_declaration") {
            const TSNode name = field(p, "name");
            const TSNode type = field(p, "default_type");
            if (ts_node_is_null(name) || ts_node_is_null(type)) {
                return std::nullopt;
            }
            std::optional<ast::TypeDesc> desc;
            if (kind_of(type) == "type_descriptor") {
                desc = convert_type_desc(type);
            } else if (auto spec = convert_type_node(type)) {
                desc.emplace();
                desc->spec = std::move(*spec);
            }
            if (!desc) {
                return std::nullopt;
            }
            out.push_back(ast::TemplateParam{std::string(text(name)), range_of(name), std::move(desc), std::nullopt});
            continue;
        }
        if (kind_of(p) == "parameter_declaration") {
            // `Number T`: a type constrained by a concept (C++20). Non-type
            // parameters look the same; analysis tells them apart.
            const TSNode type = field(p, "type");
            const TSNode name = field(p, "declarator");
            if (ts_node_is_null(type) || ts_node_is_null(name) || kind_of(name) != "identifier") {
                return std::nullopt;
            }
            auto spec = convert_type_node(type);
            if (!spec) {
                return std::nullopt;
            }
            out.push_back(ast::TemplateParam{std::string(text(name)), range_of(name), std::nullopt, std::move(spec)});
            continue;
        }
        if (kind_of(p) != "type_parameter_declaration") {
            return std::nullopt;  // non-type and template-template parameters
        }
        auto name = first_named_child(p);
        if (!name || kind_of(*name) != "type_identifier") {
            return std::nullopt;
        }
        out.push_back(ast::TemplateParam{std::string(text(*name)), range_of(*name), std::nullopt, std::nullopt});
    }
    return out;
}

std::optional<ast::Param> TreeConverter::convert_param(TSNode p) const {
    const std::string_view kind = kind_of(p);
    if (kind != "parameter_declaration" && kind != "optional_parameter_declaration") {
        return std::nullopt;  // variadic
    }
    ast::Param param;
    param.range = range_of(p);
    auto type = convert_type_spec(p);
    if (!type) {
        return std::nullopt;
    }
    param.type = std::move(*type);
    const TSNode d = field(p, "declarator");
    if (!ts_node_is_null(d) && !convert_declarator(d, param.declarator)) {
        return std::nullopt;
    }
    const TSNode def = field(p, "default_value");
    if (!ts_node_is_null(def)) {
        param.default_value = convert_expr_ptr(def);
    }
    return param;
}

std::optional<ast::Stmt> TreeConverter::convert_try(TSNode node) const {
    ast::TryStmt t;
    const TSNode body = field(node, "body");
    if (ts_node_is_null(body)) {
        return std::nullopt;
    }
    t.body = std::make_unique<ast::Stmt>(make_stmt(body, convert_block(body)));
    for (const TSNode child : named_children(node)) {
        if (kind_of(child) != "catch_clause") {
            continue;
        }
        ast::CatchClause c;
        c.range = range_of(child);
        const TSNode params = field(child, "parameters");
        const TSNode handler = field(child, "body");
        if (ts_node_is_null(params) || ts_node_is_null(handler)) {
            return std::nullopt;
        }
        const auto declared = named_children(params);
        if (declared.size() == 1) {
            c.param = convert_param(declared.front());
            if (!c.param) {
                return std::nullopt;
            }
        } else if (!declared.empty() || text(params).find("...") == std::string_view::npos) {
            return std::nullopt;
        }
        c.body = std::make_unique<ast::Stmt>(make_stmt(handler, convert_block(handler)));
        t.catches.push_back(std::move(c));
    }
    if (t.catches.empty()) {
        return std::nullopt;
    }
    return make_stmt(node, std::move(t));
}

std::optional<ast::MethodTemplate> TreeConverter::convert_method_template(TSNode node) const {
    auto params = convert_template_params(node);
    const auto children = named_children(node);
    if (!params || children.empty()) {
        return std::nullopt;
    }
    const TSNode inner = children.back();
    const std::string_view kind = kind_of(inner);
    auto declarator = TSNode{};
    if (kind == "function_definition") {
        declarator = field(inner, "declarator");
    } else if (kind == "field_declaration" || kind == "declaration") {
        const auto declarators = children_with_field(inner, "declarator");
        if (declarators.size() != 1 || !is_function_declarator(declarators.front())) {
            return std::nullopt;
        }
        declarator = declarators.front();
    } else {
        return std::nullopt;
    }
    auto fn = convert_function_decl(inner, declarator);
    if (!fn || fn->is_constructor || fn->is_destructor || fn->is_virtual) {
        return std::nullopt;
    }
    ast::MethodTemplate t;
    t.params = std::move(*params);
    for (const TSNode child : children) {
        if (kind_of(child) == "requires_clause") {
            t.requires_clause = std::make_unique<ast::Expr>(convert_constraint(field(child, "constraint")));
        }
    }
    t.decl = std::move(*fn);
    return t;
}

std::optional<ast::Stmt> TreeConverter::convert_template(TSNode node) const {
    ast::TemplateDecl t;
    auto params = convert_template_params(node);
    if (!params) {
        return std::nullopt;
    }
    t.params = std::move(*params);
    for (const TSNode child : named_children(node)) {
        if (kind_of(child) == "requires_clause") {
            t.requires_clause = std::make_unique<ast::Expr>(convert_constraint(field(child, "constraint")));
        }
    }
    const auto children = named_children(node);
    if (children.empty()) {
        return std::nullopt;
    }
    std::vector<ast::Stmt> inner;
    convert_statement(children.back(), inner);
    if (inner.size() != 1) {
        return std::nullopt;
    }
    t.inner = std::make_unique<ast::Stmt>(std::move(inner.front()));
    return make_stmt(node, std::move(t));
}

std::optional<ast::Stmt> TreeConverter::convert_alias(TSNode node) const {
    const TSNode name = field(node, "name");
    const TSNode type = field(node, "type");
    if (ts_node_is_null(name) || ts_node_is_null(type)) {
        return std::nullopt;
    }
    auto desc = convert_type_desc(type);
    if (!desc) {
        return std::nullopt;
    }
    return make_stmt(node, ast::AliasDecl{std::string(text(name)), std::move(*desc)});
}

std::optional<ast::Stmt> TreeConverter::convert_typedef(TSNode node) const {
    const auto declarators = children_with_field(node, "declarator");
    auto spec = convert_type_spec(node);
    if (declarators.size() != 1 || !spec) {
        return std::nullopt;
    }
    ast::TypeDesc desc;
    desc.spec = std::move(*spec);
    if (!convert_declarator(declarators.front(), desc.declarator)) {
        return std::nullopt;
    }
    std::string name = std::move(desc.declarator.name);
    desc.declarator.name.clear();
    return make_stmt(node, ast::AliasDecl{std::move(name), std::move(desc)});
}

// =============================================================================
// Declarations
// =============================================================================

std::optional<ast::TypeSpec> TreeConverter::convert_type_spec(TSNode decl) const {
    ast::TypeSpec spec;
    const TSNode type = field(decl, "type");
    if (!ts_node_is_null(type)) {
        auto converted = convert_type_node(type);
        if (!converted) {
            return std::nullopt;
        }
        spec = std::move(*converted);
    }
    const std::uint32_t count = ts_node_child_count(decl);
    for (std::uint32_t i = 0; i < count; ++i) {
        const TSNode child = ts_node_child(decl, i);
        const std::string_view kind = kind_of(child);
        if (kind == "type_qualifier") {
            const auto q = text(child);
            if (q == "const") {
                spec.is_const = true;
            } else if (q == "constexpr") {
                spec.is_constexpr = true;
                spec.is_const = true;
            } else if (q == "inline") {
                spec.is_inline = true;
            } else if (q != "volatile") {
                return std::nullopt;
            }
        } else if (kind == "storage_class_specifier") {
            const auto s = text(child);
            if (s == "static") {
                spec.is_static = true;
            } else if (s == "inline") {
                spec.is_inline = true;
            } else {
                return std::nullopt;  // extern, register, thread_local...
            }
        }
    }
    return spec;
}

std::optional<ast::TypeSpec> TreeConverter::convert_type_node(TSNode type) const {
    ast::TypeSpec spec;
    spec.range = range_of(type);
    const std::string_view kind = kind_of(type);
    if (kind == "primitive_type" || kind == "type_identifier") {
        spec.name = std::string(text(type));
        return spec;
    }
    if (kind == "sized_type_specifier") {
        spec.name = collapse_spaces(text(type));
        return spec;
    }
    if (kind == "placeholder_type_specifier") {
        const TSNode constraint = field(type, "constraint");
        if (!ts_node_is_null(constraint)) {
            auto concept_spec = convert_type_node(constraint);  // `std::integral auto`
            if (!concept_spec) {
                return std::nullopt;
            }
            spec.auto_constraint = std::make_shared<ast::TypeSpec>(std::move(*concept_spec));
        } else if (text(type) != "auto") {
            return std::nullopt;  // decltype(auto)
        }
        spec.name = "auto";
        return spec;
    }
    if (kind == "struct_specifier" || kind == "class_specifier" || kind == "enum_specifier") {
        const TSNode name = field(type, "name");
        if (ts_node_is_null(name) || kind_of(name) != "type_identifier") {
            return std::nullopt;
        }
        spec.name = std::string(text(name));
        return spec;
    }
    if (kind == "template_type") {
        const TSNode name = field(type, "name");
        auto args = convert_template_args(field(type, "arguments"));
        if (ts_node_is_null(name) || !args) {
            return std::nullopt;
        }
        spec.name = std::string(text(name));
        spec.template_args = std::move(*args);
        return spec;
    }
    if (kind == "qualified_identifier") {
        TSNode current = type;
        while (kind_of(current) == "qualified_identifier") {
            const TSNode scope = field(current, "scope");
            if (ts_node_is_null(scope) || !is_name_kind(kind_of(scope))) {
                return std::nullopt;
            }
            spec.scope.emplace_back(text(scope));
            current = field(current, "name");
            if (ts_node_is_null(current)) {
                return std::nullopt;
            }
        }
        auto inner = convert_type_node(current);
        if (!inner) {
            return std::nullopt;
        }
        inner->scope = std::move(spec.scope);
        inner->range = spec.range;
        return inner;
    }
    return std::nullopt;
}

bool TreeConverter::convert_declarator(TSNode node, ast::Declarator& out) const {
    TSNode current = node;
    while (!ts_node_is_null(current)) {
        const std::string_view kind = kind_of(current);
        if (is_name_kind(kind)) {
            out.name = std::string(text(current));
            out.range = range_of(current);
            return true;
        }
        if (kind == "qualified_identifier") {
            const SourceRange whole = range_of(current);
            while (kind_of(current) == "qualified_identifier") {
                const TSNode scope = field(current, "scope");
                if (ts_node_is_null(scope) || !is_name_kind(kind_of(scope))) {
                    return false;
                }
                out.scope.emplace_back(text(scope));
                current = field(current, "name");
                if (ts_node_is_null(current)) {
                    return false;
                }
            }
            if (!is_name_kind(kind_of(current))) {
                return false;
            }
            out.name = std::string(text(current));
            out.range = whole;
            return true;
        }
        if (kind == "pointer_declarator" || kind == "abstract_pointer_declarator") {
            ast::DeclaratorPart part;
            part.kind = ast::DeclaratorKind::Pointer;
            part.range = range_of(current);
            const std::uint32_t count = ts_node_named_child_count(current);
            for (std::uint32_t i = 0; i < count; ++i) {
                const TSNode child = ts_node_named_child(current, i);
                if (kind_of(child) == "type_qualifier" && text(child) == "const") {
                    part.is_const = true;
                }
            }
            out.parts.push_back(std::move(part));
            current = field(current, "declarator");
            continue;
        }
        if (kind == "reference_declarator" || kind == "abstract_reference_declarator") {
            ast::DeclaratorPart part;
            part.kind =
                has_child_kind(current, "&&") ? ast::DeclaratorKind::RvalueReference : ast::DeclaratorKind::Reference;
            part.range = range_of(current);
            out.parts.push_back(std::move(part));
            auto inner = first_named_child(current);
            current = inner ? *inner : TSNode{};
            if (!inner) {
                return true;
            }
            continue;
        }
        if (kind == "array_declarator" || kind == "abstract_array_declarator") {
            ast::DeclaratorPart part;
            part.kind = ast::DeclaratorKind::Array;
            part.range = range_of(current);
            const TSNode size = field(current, "size");
            if (!ts_node_is_null(size)) {
                part.size = convert_expr_ptr(size);
            }
            out.parts.push_back(std::move(part));
            current = field(current, "declarator");
            continue;
        }
        if (kind == "structured_binding_declarator") {
            for (const TSNode child : named_children(current)) {
                if (kind_of(child) != "identifier") {
                    return false;
                }
                out.bindings.emplace_back(text(child));
            }
            out.range = range_of(current);
            return !out.bindings.empty();
        }
        if (kind == "parenthesized_declarator" || kind == "abstract_parenthesized_declarator") {
            auto inner = first_named_child(current);
            if (!inner) {
                return false;
            }
            current = *inner;
            continue;
        }
        return false;
    }
    return true;  // abstract declarator: no name
}

std::optional<ast::VarDeclarator> TreeConverter::convert_var_declarator(TSNode node) const {
    ast::VarDeclarator var;
    var.range = range_of(node);
    if (kind_of(node) != "init_declarator") {
        if (!convert_declarator(node, var.declarator)) {
            return std::nullopt;
        }
        return var;
    }
    if (!convert_declarator(field(node, "declarator"), var.declarator)) {
        return std::nullopt;
    }
    const TSNode value = field(node, "value");
    if (ts_node_is_null(value)) {
        return var;
    }
    const std::string_view kind = kind_of(value);
    if (kind == "argument_list") {
        var.style = ast::InitStyle::Direct;
        auto args = convert_arguments(value);
        if (!args) {
            return std::nullopt;
        }
        var.args = std::move(*args);
    } else if (kind == "initializer_list" && !has_child_kind(node, "=")) {
        var.style = ast::InitStyle::List;
        var.init = convert_expr_ptr(value);
    } else {
        var.style = ast::InitStyle::Copy;
        var.init = convert_expr_ptr(value);
    }
    return var;
}

bool TreeConverter::is_function_declarator(TSNode node) const {
    TSNode current = node;
    while (!ts_node_is_null(current)) {
        const std::string_view kind = kind_of(current);
        if (kind == "function_declarator" || kind == "operator_cast") {
            return true;
        }
        if (kind == "pointer_declarator" || kind == "reference_declarator" || kind == "parenthesized_declarator") {
            auto inner = kind == "pointer_declarator" ? std::optional<TSNode>(field(current, "declarator"))
                                                      : first_named_child(current);
            if (!inner || ts_node_is_null(*inner)) {
                return false;
            }
            current = *inner;
            continue;
        }
        return false;
    }
    return false;
}

std::shared_ptr<ast::DeclStmt> TreeConverter::convert_object_reading(TSNode node, TSNode declarator) const {
    // Only `T name(args);`: the most vexing parse. Anything else (`T* f(...)`,
    // `f(...) const`, a qualified name) can only be a function.
    if (kind_of(declarator) != "function_declarator" || ts_node_named_child_count(declarator) != 2) {
        return nullptr;
    }
    const TSNode name = field(declarator, "declarator");
    const TSNode params = field(declarator, "parameters");
    if (ts_node_is_null(name) || kind_of(name) != "identifier" || ts_node_is_null(params)) {
        return nullptr;
    }
    const auto param_nodes = named_children(params);
    if (param_nodes.empty()) {
        return nullptr;  // `T x();` declares a function in C++ too
    }
    ast::VarDeclarator var;
    var.range = range_of(declarator);
    var.declarator.name = std::string(text(name));
    var.declarator.range = range_of(name);
    var.style = ast::InitStyle::Direct;
    for (const TSNode p : param_nodes) {
        auto arg = param_as_expr(p);
        if (!arg) {
            return nullptr;
        }
        var.args.push_back(std::move(*arg));
    }
    auto type = convert_type_spec(node);
    if (!type) {
        return nullptr;
    }
    auto decl = std::make_shared<ast::DeclStmt>();
    decl->type = std::move(*type);
    decl->vars.push_back(std::move(var));
    return decl;
}

std::optional<ast::Expr> TreeConverter::param_as_expr(TSNode param) const {
    // `kWidth * kHeight`, `a & b`, `a && b`, `grid[2]`, `North`, `false`:
    // a parameter declaration that is also an expression.
    if (kind_of(param) != "parameter_declaration" || ts_node_named_child_count(param) > 2) {
        return std::nullopt;
    }
    const TSNode type = field(param, "type");
    if (ts_node_is_null(type)) {
        return std::nullopt;
    }
    std::optional<ast::Expr> lhs;
    if (kind_of(type) == "type_identifier") {
        const std::string_view spelled = text(type);
        if (spelled == "true" || spelled == "false") {
            lhs = ast::Expr{range_of(type), ast::BoolLiteral{spelled == "true"}};
        } else if (spelled == "nullptr") {
            lhs = ast::Expr{range_of(type), ast::NullptrLiteral{}};
        } else {
            lhs = convert_name(type);
        }
    } else if (kind_of(type) == "qualified_identifier") {
        lhs = convert_name(type);
    }
    if (!lhs) {
        return std::nullopt;
    }
    const TSNode d = field(param, "declarator");
    if (ts_node_is_null(d)) {
        return lhs;
    }
    const SourceRange range = range_of(param);
    const std::string_view kind = kind_of(d);
    if (kind == "abstract_array_declarator") {
        // `grid[2][3]`: the outermost declarator is the last subscript.
        std::vector<TSNode> sizes;
        for (TSNode current = d; !ts_node_is_null(current); current = field(current, "declarator")) {
            if (kind_of(current) != "abstract_array_declarator" || !has_field(current, "size")) {
                return std::nullopt;
            }
            sizes.push_back(field(current, "size"));
        }
        ast::Expr result = std::move(*lhs);
        for (auto it = sizes.rbegin(); it != sizes.rend(); ++it) {
            ast::SubscriptExpr sub;
            sub.base = std::make_unique<ast::Expr>(std::move(result));
            sub.index = convert_expr_ptr(*it);
            result = ast::Expr{SourceRange{range.begin, range_of(*it).end}, std::move(sub)};
        }
        return result;
    }
    ast::BinaryExpr bin;
    TSNode operand;
    if (kind == "pointer_declarator" && ts_node_named_child_count(d) == 1) {
        bin.op = ast::BinaryOp::Mul;
        operand = field(d, "declarator");
    } else if (kind == "reference_declarator" && ts_node_named_child_count(d) == 1) {
        bin.op = has_child_kind(d, "&&") ? ast::BinaryOp::LogicalAnd : ast::BinaryOp::BitAnd;
        operand = ts_node_named_child(d, 0);
    } else {
        return std::nullopt;  // `Point p`: only a declaration
    }
    auto rhs = declarator_as_expr(operand);
    if (!rhs) {
        return std::nullopt;
    }
    bin.lhs = std::make_unique<ast::Expr>(std::move(*lhs));
    bin.rhs = std::make_unique<ast::Expr>(std::move(*rhs));
    return ast::Expr{range, std::move(bin)};
}

std::optional<ast::Expr> TreeConverter::declarator_as_expr(TSNode declarator) const {
    if (ts_node_is_null(declarator)) {
        return std::nullopt;
    }
    const std::string_view kind = kind_of(declarator);
    if (kind == "identifier") {
        return convert_name(declarator);
    }
    if (kind == "array_declarator" && has_field(declarator, "size")) {
        auto base = declarator_as_expr(field(declarator, "declarator"));
        if (!base) {
            return std::nullopt;
        }
        ast::SubscriptExpr sub;
        sub.base = std::make_unique<ast::Expr>(std::move(*base));
        sub.index = convert_expr_ptr(field(declarator, "size"));
        return ast::Expr{range_of(declarator), std::move(sub)};
    }
    return std::nullopt;
}

bool TreeConverter::name_function(TSNode name, ast::FunctionDecl& fn) const {
    if (ts_node_is_null(name)) {
        return false;
    }
    const std::string_view kind = kind_of(name);
    if (kind == "operator_name") {
        // `operator +` -> "operator+"
        std::string spelled;
        for (const char c : text(name)) {
            if (c != ' ' && c != '\t' && c != '\n') {
                spelled.push_back(c);
            }
        }
        fn.declarator.name = spelled;
        return true;
    }
    if (kind == "destructor_name") {
        auto id = first_named_child(name);
        if (!id) {
            return false;
        }
        fn.declarator.name = "~" + std::string(text(*id));
        fn.is_destructor = true;
        return true;
    }
    if (is_name_kind(kind)) {
        fn.declarator.name = std::string(text(name));
        return true;
    }
    return false;
}

std::optional<ast::FunctionDecl> TreeConverter::convert_function_decl(TSNode node, TSNode declarator) const {
    ast::FunctionDecl fn;
    fn.range = range_of(node);
    auto ret = convert_type_spec(node);
    if (!ret) {
        return std::nullopt;
    }
    fn.return_type = std::move(*ret);
    fn.is_static = fn.return_type.is_static;
    fn.is_virtual = has_child_kind(node, "virtual");

    // Return-type modifiers (`int* f()`), then the function declarator.
    TSNode current = declarator;
    while (!ts_node_is_null(current) && kind_of(current) != "function_declarator") {
        const std::string_view kind = kind_of(current);
        ast::DeclaratorPart part;
        part.range = range_of(current);
        if (kind == "pointer_declarator") {
            part.kind = ast::DeclaratorKind::Pointer;
            current = field(current, "declarator");
        } else if (kind == "reference_declarator") {
            part.kind =
                has_child_kind(current, "&&") ? ast::DeclaratorKind::RvalueReference : ast::DeclaratorKind::Reference;
            auto inner = first_named_child(current);
            current = inner ? *inner : TSNode{};
        } else if (kind == "operator_cast") {
            break;
        } else {
            return std::nullopt;
        }
        fn.declarator.parts.push_back(std::move(part));
    }
    if (ts_node_is_null(current)) {
        return std::nullopt;
    }
    fn.is_explicit = has_child_kind(node, "explicit_function_specifier");
    if (kind_of(current) == "operator_cast") {
        // `operator bool() const`: the name is the target type, which is also the result.
        auto target = convert_type_node(field(current, "type"));
        const TSNode inner = field(current, "declarator");
        if (!target || ts_node_is_null(inner) || kind_of(inner) != "abstract_function_declarator") {
            return std::nullopt;
        }
        fn.return_type = std::move(*target);
        fn.is_conversion = true;
        fn.declarator.name = "operator " + fn.return_type.name;
        fn.declarator.range = range_of(current);
        current = inner;  // parameters and qualifiers live here
    }

    TSNode name = fn.is_conversion ? TSNode{} : field(current, "declarator");
    while (!fn.is_conversion && !ts_node_is_null(name) && kind_of(name) == "qualified_identifier") {
        const TSNode scope = field(name, "scope");
        if (ts_node_is_null(scope) || !is_name_kind(kind_of(scope))) {
            return std::nullopt;
        }
        fn.scope.emplace_back(text(scope));
        name = field(name, "name");
    }
    if (!fn.is_conversion) {
        // Conversion functions were named above, from their target type.
        if (!name_function(name, fn)) {
            return std::nullopt;
        }
        fn.declarator.range = range_of(name);
        fn.is_constructor = !fn.is_destructor && fn.return_type.name.empty() && kind_of(name) != "operator_name";
    }

    const TSNode params = field(current, "parameters");
    if (ts_node_is_null(params)) {
        return std::nullopt;
    }
    for (const TSNode p : named_children(params)) {
        auto converted = convert_param(p);
        if (!converted) {
            return std::nullopt;
        }
        ast::Param param = std::move(*converted);
        // `f(void)` means no parameters.
        if (param.type.name == "void" && param.declarator.parts.empty() && param.declarator.name.empty() &&
            named_children(params).size() == 1) {
            break;
        }
        fn.params.push_back(std::move(param));
    }

    const std::uint32_t count = ts_node_child_count(current);
    for (std::uint32_t i = 0; i < count; ++i) {
        const TSNode child = ts_node_child(current, i);
        const std::string_view kind = kind_of(child);
        if (kind == "type_qualifier" && text(child) == "const") {
            fn.is_const = true;
        } else if (kind == "virtual_specifier") {
            fn.is_override = fn.is_override || text(child) == "override";
            fn.is_final = fn.is_final || text(child) == "final";
        } else if (kind == "requires_clause") {
            fn.requires_clause = std::make_shared<ast::Expr>(convert_constraint(field(child, "constraint")));
        } else if (kind == "noexcept") {
            // noexcept is accepted and not checked
        } else if (kind == "throw_specifier" || kind == "trailing_return_type" || kind == "ref_qualifier") {
            return std::nullopt;
        }
    }

    const TSNode default_value = field(node, "default_value");
    if (!ts_node_is_null(default_value)) {
        if (text(default_value) != "0") {
            return std::nullopt;
        }
        fn.is_pure = true;
    }
    fn.is_deleted = has_child_kind(node, "delete_method_clause");
    fn.is_defaulted = has_child_kind(node, "default_method_clause");

    for (const TSNode child : named_children(node)) {
        if (kind_of(child) != "field_initializer_list") {
            continue;
        }
        for (const TSNode init : named_children(child)) {
            ast::MemberInit mi;
            mi.range = range_of(init);
            auto parts = named_children(init);
            if (parts.size() != 2) {
                return std::nullopt;
            }
            // `std::logic_error(...)`, `Base<int>(...)`: a base named by its class name.
            TSNode target = parts[0];
            while (kind_of(target) == "qualified_identifier" && !ts_node_is_null(field(target, "name"))) {
                target = field(target, "name");
            }
            if (kind_of(target) == "template_type" || kind_of(target) == "template_method") {
                target = field(target, "name");
            }
            if (ts_node_is_null(target) || !is_name_kind(kind_of(target))) {
                return std::nullopt;
            }
            mi.name = std::string(text(target));
            if (kind_of(parts[1]) == "argument_list") {
                auto args = convert_arguments(parts[1]);
                if (!args) {
                    return std::nullopt;
                }
                mi.args = std::move(*args);
            } else {
                mi.args.push_back(convert_expr(parts[1]));
            }
            fn.inits.push_back(std::move(mi));
        }
    }

    const TSNode body = field(node, "body");
    if (!ts_node_is_null(body)) {
        if (kind_of(body) != "compound_statement") {
            return std::nullopt;  // function-try-block
        }
        fn.body = std::make_unique<ast::Stmt>(make_stmt(body, convert_block(body)));
    }
    return fn;
}

std::optional<ast::TypeDesc> TreeConverter::convert_type_desc(TSNode node) const {
    if (ts_node_is_null(node) || kind_of(node) != "type_descriptor") {
        return std::nullopt;
    }
    ast::TypeDesc desc;
    auto spec = convert_type_spec(node);
    if (!spec) {
        return std::nullopt;
    }
    desc.spec = std::move(*spec);
    const TSNode declarator = field(node, "declarator");
    if (!ts_node_is_null(declarator) && !convert_declarator(declarator, desc.declarator)) {
        return std::nullopt;
    }
    return desc;
}

std::optional<std::vector<ast::TypeDesc>> TreeConverter::convert_template_args(TSNode list) const {
    if (ts_node_is_null(list)) {
        return std::nullopt;
    }
    std::vector<ast::TypeDesc> args;
    for (const TSNode child : named_children(list)) {
        auto desc = convert_type_desc(child);
        if (!desc) {
            return std::nullopt;  // non-type template arguments
        }
        args.push_back(std::move(*desc));
    }
    return args;
}

std::optional<ast::RecordDef> TreeConverter::convert_record(TSNode node) const {
    ast::RecordDef record;
    record.is_class = kind_of(node) == "class_specifier";
    const TSNode name = field(node, "name");
    if (ts_node_is_null(name) || kind_of(name) != "type_identifier") {
        return std::nullopt;  // anonymous structs, templated names
    }
    record.name = std::string(text(name));
    record.name_range = range_of(name);
    const ast::Access default_access = record.is_class ? ast::Access::Private : ast::Access::Public;

    for (const TSNode child : named_children(node)) {
        if (kind_of(child) == "virtual_specifier" && text(child) == "final") {
            record.is_final = true;
        }
        if (kind_of(child) != "base_class_clause") {
            continue;
        }
        ast::BaseSpec base;
        base.access = default_access;
        const std::uint32_t count = ts_node_child_count(child);
        for (std::uint32_t i = 0; i < count; ++i) {
            const TSNode part = ts_node_child(child, i);
            const std::string_view kind = kind_of(part);
            if (kind == "access_specifier") {
                const auto a = text(part);
                base.access = access_from(a);
            } else if (kind == "virtual") {
                base.is_virtual = true;
            } else if (kind == "type_identifier" || kind == "qualified_identifier" || kind == "template_type") {
                auto spec = convert_type_node(part);
                if (!spec) {
                    return std::nullopt;
                }
                base.name = std::string(text(part));
                base.range = range_of(part);
                base.type = std::move(*spec);
                record.bases.push_back(std::move(base));
                base = ast::BaseSpec{};
                base.access = default_access;
            }
        }
    }

    const TSNode body = field(node, "body");
    if (ts_node_is_null(body)) {
        record.has_body = false;
        return record;
    }
    ast::Access access = default_access;
    for (const TSNode member : named_children(body)) {
        const std::string_view kind = kind_of(member);
        if (kind == "access_specifier") {
            const auto a = text(member);
            access = access_from(a);
            continue;
        }
        if (kind == "function_definition") {
            auto fn = convert_function_decl(member, field(member, "declarator"));
            if (!fn) {
                return std::nullopt;
            }
            fn->access = access;
            record.methods.push_back(std::move(*fn));
            continue;
        }
        if (kind == "field_declaration" || kind == "declaration") {
            const auto declarators = children_with_field(member, "declarator");
            if (declarators.size() == 1 && is_function_declarator(declarators.front())) {
                auto fn = convert_function_decl(member, declarators.front());
                if (!fn) {
                    return std::nullopt;
                }
                fn->access = access;
                record.methods.push_back(std::move(*fn));
                continue;
            }
            ast::FieldDecl f;
            f.access = access;
            f.range = range_of(member);
            auto type = convert_type_spec(member);
            if (!type) {
                return std::nullopt;
            }
            f.type = std::move(*type);
            // Declarators and their default values come interleaved.
            const std::uint32_t count = ts_node_child_count(member);
            for (std::uint32_t i = 0; i < count; ++i) {
                const char* field_name = ts_node_field_name_for_child(member, i);
                if (field_name == nullptr) {
                    continue;
                }
                const TSNode part = ts_node_child(member, i);
                if (std::string_view(field_name) == "declarator") {
                    if (is_function_declarator(part)) {
                        return std::nullopt;
                    }
                    auto var = convert_var_declarator(part);
                    if (!var) {
                        return std::nullopt;
                    }
                    f.vars.push_back(std::move(*var));
                } else if (std::string_view(field_name) == "default_value" && !f.vars.empty()) {
                    auto& last = f.vars.back();
                    last.style = kind_of(part) == "initializer_list" ? ast::InitStyle::List : ast::InitStyle::Copy;
                    last.init = convert_expr_ptr(part);
                }
            }
            if (f.vars.empty()) {
                return std::nullopt;  // nested type definitions, bit-fields...
            }
            record.fields.push_back(std::move(f));
            continue;
        }
        if (kind == "template_declaration") {
            auto t = convert_method_template(member);
            if (!t) {
                return std::nullopt;
            }
            t->decl.access = access;
            record.method_templates.push_back(std::move(*t));
            continue;
        }
        return std::nullopt;  // friends, nested classes, using-declarations...
    }
    return record;
}

std::optional<ast::EnumDef> TreeConverter::convert_enum(TSNode node) const {
    ast::EnumDef e;
    const TSNode name = field(node, "name");
    if (ts_node_is_null(name) || kind_of(name) != "type_identifier") {
        return std::nullopt;
    }
    e.name = std::string(text(name));
    e.name_range = range_of(name);
    e.scoped = has_child_kind(node, "class") || has_child_kind(node, "struct");
    if (has_field(node, "base")) {
        return std::nullopt;  // explicit underlying type
    }
    const TSNode body = field(node, "body");
    if (ts_node_is_null(body)) {
        return std::nullopt;
    }
    for (const TSNode child : named_children(body)) {
        if (kind_of(child) != "enumerator") {
            return std::nullopt;
        }
        ast::Enumerator en;
        const TSNode en_name = field(child, "name");
        if (ts_node_is_null(en_name)) {
            return std::nullopt;
        }
        en.name = std::string(text(en_name));
        en.range = range_of(child);
        const TSNode value = field(child, "value");
        if (!ts_node_is_null(value)) {
            en.value = convert_expr_ptr(value);
        }
        e.enumerators.push_back(std::move(en));
    }
    return e;
}

// =============================================================================
// Expressions
// =============================================================================

ast::ExprPtr TreeConverter::convert_expr_ptr(TSNode node) const {
    return std::make_unique<ast::Expr>(convert_expr(node));
}

ast::Expr TreeConverter::unsupported_expr(TSNode node) const {
    return {range_of(node), FeatureScanner::scan(node, source_)};
}

ast::Expr TreeConverter::convert_expr(TSNode node) const {
    const std::string_view kind = kind_of(node);
    const SourceRange range = range_of(node);

    if (kind == "parenthesized_expression") {
        if (auto inner = first_named_child(node)) {
            return convert_expr(*inner);
        }
        return unsupported_expr(node);
    }
    if (kind == "template_function" || kind == "template_type" ||
        (kind == "qualified_identifier" && has_template_name(node))) {
        // `Number<int>` used as a value: a concept applied to types.
        ast::CallExpr id;
        id.concept_id = true;
        TSNode current = node;
        while (kind_of(current) == "qualified_identifier") {
            const TSNode scope = field(current, "scope");
            if (ts_node_is_null(scope) || !is_name_kind(kind_of(scope))) {
                return unsupported_expr(node);
            }
            id.scope.emplace_back(text(scope));
            current = field(current, "name");
        }
        const TSNode name = field(current, "name");
        auto targs = convert_template_args(field(current, "arguments"));
        if (ts_node_is_null(name) || !targs) {
            return unsupported_expr(node);
        }
        id.callee = std::string(text(name));
        id.callee_range = range_of(name);
        id.template_args = std::move(*targs);
        return {range, std::move(id)};
    }
    if (kind == "identifier" || kind == "qualified_identifier") {
        if (auto name = convert_name(node)) {
            return std::move(*name);
        }
        return unsupported_expr(node);
    }
    if (kind == "constraint_conjunction" || kind == "constraint_disjunction") {
        return convert_constraint(node);
    }
    if (kind == "requires_expression") {
        return convert_requires(node);
    }
    if (kind == "true" || kind == "false") {
        return {range, ast::BoolLiteral{kind == "true"}};
    }
    if (kind == "number_literal") {
        return convert_number(node);
    }
    if (kind == "char_literal") {
        const auto spelling = text(node);
        if (spelling.size() < 3 || spelling.front() != '\'' || spelling.back() != '\'') {
            return unsupported_expr(node);  // u8'x', L'x'
        }
        auto decoded = decode_escapes(spelling.substr(1, spelling.size() - 2));
        if (!decoded || decoded->size() != 1) {
            return unsupported_expr(node);
        }
        return {range, ast::CharLiteral{static_cast<signed char>((*decoded)[0])}};
    }
    if (kind == "string_literal") {
        const auto spelling = text(node);
        if (spelling.size() < 2 || spelling.front() != '"' || spelling.back() != '"') {
            return unsupported_expr(node);
        }
        auto decoded = decode_escapes(spelling.substr(1, spelling.size() - 2));
        if (!decoded) {
            return unsupported_expr(node);
        }
        return {range, ast::StringLiteral{std::move(*decoded)}};
    }
    if (kind == "concatenated_string") {
        std::string value;
        for (const TSNode part : named_children(node)) {
            auto piece = convert_expr(part);
            auto* str = std::get_if<ast::StringLiteral>(&piece.node);
            if (str == nullptr) {
                return unsupported_expr(node);
            }
            value += str->value;
        }
        return {range, ast::StringLiteral{std::move(value)}};
    }
    if (kind == "nullptr" || (kind == "null" && text(node) == "nullptr")) {
        return {range, ast::NullptrLiteral{}};
    }
    if (kind == "null") {  // `NULL`: the integer 0, as in C++98
        ast::IntLiteral zero;
        zero.text = std::string(text(node));
        return {range, std::move(zero)};
    }
    if (kind == "this") {
        return {range, ast::ThisExpr{}};
    }
    if (kind == "call_expression") {
        return convert_call(node);
    }
    if (kind == "unary_expression" || kind == "pointer_expression") {
        const auto op = operator_of(node);
        const TSNode argument = field(node, "argument");
        std::optional<ast::UnaryOp> unary;
        if (op == "-") unary = ast::UnaryOp::Minus;
        if (op == "+") unary = ast::UnaryOp::Plus;
        if (op == "!" || op == "not") unary = ast::UnaryOp::Not;
        if (op == "~" || op == "compl") unary = ast::UnaryOp::BitNot;
        if (op == "&") unary = ast::UnaryOp::AddressOf;
        if (op == "*") unary = ast::UnaryOp::Deref;
        if (!unary || ts_node_is_null(argument)) {
            return unsupported_expr(node);
        }
        return {range, ast::UnaryExpr{*unary, convert_expr_ptr(argument)}};
    }
    if (kind == "binary_expression") {
        const auto op = binary_op(operator_of(node));
        const TSNode left = field(node, "left");
        const TSNode right = field(node, "right");
        if (!op || ts_node_is_null(left) || ts_node_is_null(right)) {
            return unsupported_expr(node);
        }
        return {range, ast::BinaryExpr{*op, convert_expr_ptr(left), convert_expr_ptr(right)}};
    }
    if (kind == "comma_expression") {
        const TSNode left = field(node, "left");
        const TSNode right = field(node, "right");
        if (ts_node_is_null(left) || ts_node_is_null(right)) {
            return unsupported_expr(node);
        }
        return {range, ast::BinaryExpr{ast::BinaryOp::Comma, convert_expr_ptr(left), convert_expr_ptr(right)}};
    }
    if (kind == "assignment_expression") {
        const auto op = operator_of(node);
        const TSNode left = field(node, "left");
        const TSNode right = field(node, "right");
        if (ts_node_is_null(left) || ts_node_is_null(right)) {
            return unsupported_expr(node);
        }
        ast::AssignExpr assign;
        if (op != "=") {
            assign.op = compound_assign_op(op);
            if (!assign.op) {
                return unsupported_expr(node);
            }
        }
        assign.lhs = convert_expr_ptr(left);
        assign.rhs = convert_expr_ptr(right);
        return {range, std::move(assign)};
    }
    if (kind == "update_expression") {
        const TSNode op = field(node, "operator");
        const TSNode argument = field(node, "argument");
        if (ts_node_is_null(op) || ts_node_is_null(argument)) {
            return unsupported_expr(node);
        }
        ast::IncDecExpr inc;
        inc.increment = kind_of(op) == "++";
        inc.prefix = ts_node_start_byte(op) < ts_node_start_byte(argument);
        inc.operand = convert_expr_ptr(argument);
        return {range, std::move(inc)};
    }
    if (kind == "conditional_expression") {
        const TSNode c = field(node, "condition");
        const TSNode t = field(node, "consequence");
        const TSNode e = field(node, "alternative");
        if (ts_node_is_null(c) || ts_node_is_null(t) || ts_node_is_null(e)) {
            return unsupported_expr(node);
        }
        return {range, ast::ConditionalExpr{convert_expr_ptr(c), convert_expr_ptr(t), convert_expr_ptr(e)}};
    }
    if (kind == "subscript_expression") {
        const TSNode base = field(node, "argument");
        TSNode index = field(node, "index");
        const TSNode indices = field(node, "indices");
        if (ts_node_is_null(index) && !ts_node_is_null(indices)) {
            const auto items = named_children(indices);
            if (items.size() != 1) {
                return unsupported_expr(node);
            }
            index = items.front();
        }
        if (ts_node_is_null(base) || ts_node_is_null(index)) {
            return unsupported_expr(node);
        }
        return {range, ast::SubscriptExpr{convert_expr_ptr(base), convert_expr_ptr(index)}};
    }
    if (kind == "field_expression") {
        const TSNode base = field(node, "argument");
        const TSNode member = field(node, "field");
        if (ts_node_is_null(base) || ts_node_is_null(member)) {
            return unsupported_expr(node);
        }
        ast::MemberExpr m;
        if (kind_of(member) == "template_method") {
            const TSNode name = field(member, "name");
            auto targs = convert_template_args(field(member, "arguments"));
            if (ts_node_is_null(name) || !targs) {
                return unsupported_expr(node);
            }
            m.member = std::string(text(name));
            m.template_args = std::move(*targs);
        } else if (kind_of(member) == "field_identifier") {
            m.member = std::string(text(member));
        } else {
            return unsupported_expr(node);
        }
        m.base = convert_expr_ptr(base);
        m.member_range = range_of(member);
        m.arrow = operator_of(node) == "->";
        return {range, std::move(m)};
    }
    if (kind == "cast_expression") {
        auto type = convert_type_desc(field(node, "type"));
        const TSNode value = field(node, "value");
        if (!type || ts_node_is_null(value)) {
            return unsupported_expr(node);
        }
        return {range, ast::CastExpr{ast::CastKind::CStyle, std::move(*type), convert_expr_ptr(value)}};
    }
    if (kind == "sizeof_expression") {
        ast::SizeofExpr s;
        const TSNode type = field(node, "type");
        const TSNode value = field(node, "value");
        if (!ts_node_is_null(type)) {
            s.type = convert_type_desc(type);
            if (!s.type) {
                return unsupported_expr(node);
            }
        } else if (!ts_node_is_null(value)) {
            s.operand = convert_expr_ptr(value);
        } else {
            return unsupported_expr(node);
        }
        return {range, std::move(s)};
    }
    if (kind == "initializer_list") {
        ast::InitList list;
        for (const TSNode child : named_children(node)) {
            if (kind_of(child) == "initializer_pair") {
                return unsupported_expr(node);  // designated initializers
            }
            list.elements.push_back(convert_expr(child));
        }
        return {range, std::move(list)};
    }
    if (kind == "new_expression") {
        return convert_new(node);
    }
    if (kind == "lambda_expression") {
        return convert_lambda(node);
    }
    if (kind == "delete_expression") {
        auto operand = first_named_child(node);
        if (!operand) {
            return unsupported_expr(node);
        }
        return {range, ast::DeleteExpr{has_child_kind(node, "["), convert_expr_ptr(*operand)}};
    }
    return unsupported_expr(node);
}

bool TreeConverter::has_template_name(TSNode node) const {
    TSNode current = node;
    while (kind_of(current) == "qualified_identifier") {
        current = field(current, "name");
        if (ts_node_is_null(current)) {
            return false;
        }
    }
    return kind_of(current) == "template_function" || kind_of(current) == "template_type";
}

ast::Expr TreeConverter::convert_constraint(TSNode node) const {
    if (ts_node_is_null(node)) {
        return ast::Expr{};
    }
    const std::string_view kind = kind_of(node);
    if (kind == "constraint_conjunction" || kind == "constraint_disjunction") {
        ast::BinaryExpr b;
        b.op = kind == "constraint_conjunction" ? ast::BinaryOp::LogicalAnd : ast::BinaryOp::LogicalOr;
        b.lhs = std::make_unique<ast::Expr>(convert_constraint(field(node, "left")));
        b.rhs = std::make_unique<ast::Expr>(convert_constraint(field(node, "right")));
        return {range_of(node), std::move(b)};
    }
    return convert_expr(node);
}

ast::Expr TreeConverter::convert_requires(TSNode node) const {
    ast::RequiresExpr r;
    const TSNode params = field(node, "parameters");
    if (!ts_node_is_null(params)) {
        for (const TSNode p : named_children(params)) {
            auto param = convert_param(p);
            if (!param) {
                return unsupported_expr(node);
            }
            r.params.push_back(std::make_shared<ast::Param>(std::move(*param)));
        }
    }
    const TSNode seq = field(node, "requirements");
    for (const TSNode req : ts_node_is_null(seq) ? std::vector<TSNode>{} : named_children(seq)) {
        const std::string_view kind = kind_of(req);
        ast::Requirement out;
        out.range = range_of(req);
        const auto parts = named_children(req);
        if (kind == "simple_requirement") {
            if (parts.empty()) {
                continue;  // a stray `;`
            }
            out.expr = convert_expr_ptr(parts.front());
        } else if (kind == "compound_requirement") {
            if (parts.empty()) {
                return unsupported_expr(node);
            }
            out.kind = ast::Requirement::Kind::Compound;
            out.expr = convert_expr_ptr(parts.front());
            for (const TSNode part : parts) {
                if (kind_of(part) != "trailing_return_type") {
                    continue;
                }
                auto desc = first_named_child(part);
                auto spec = desc ? convert_type_desc(*desc) : std::nullopt;
                if (!spec) {
                    return unsupported_expr(node);
                }
                out.constraint = std::move(spec->spec);
            }
        } else if (kind == "type_requirement") {
            if (parts.empty()) {
                return unsupported_expr(node);
            }
            out.kind = ast::Requirement::Kind::Type;
            auto spec = convert_type_node(parts.front());
            if (!spec) {
                return unsupported_expr(node);
            }
            out.type.emplace();
            out.type->spec = std::move(*spec);
        } else {
            return unsupported_expr(node);
        }
        r.requirements.push_back(std::move(out));
    }
    return {range_of(node), std::move(r)};
}

std::optional<ast::Expr> TreeConverter::convert_name(TSNode node) const {
    ast::Identifier id;
    TSNode current = node;
    while (kind_of(current) == "qualified_identifier") {
        const TSNode scope = field(current, "scope");
        if (ts_node_is_null(scope) || !is_name_kind(kind_of(scope))) {
            return std::nullopt;
        }
        id.scope.emplace_back(text(scope));
        current = field(current, "name");
        if (ts_node_is_null(current)) {
            return std::nullopt;
        }
    }
    if (!is_name_kind(kind_of(current))) {
        return std::nullopt;
    }
    id.name = std::string(text(current));
    return ast::Expr{range_of(node), std::move(id)};
}

std::optional<std::vector<ast::Expr>> TreeConverter::convert_arguments(TSNode list) const {
    if (ts_node_is_null(list)) {
        return std::nullopt;
    }
    std::vector<ast::Expr> args;
    for (const TSNode arg : named_children(list)) {
        args.push_back(convert_expr(arg));
    }
    return args;
}

ast::Expr TreeConverter::convert_call(TSNode node) const {
    const SourceRange range = range_of(node);
    const TSNode callee = field(node, "function");
    auto args = convert_arguments(field(node, "arguments"));
    if (ts_node_is_null(callee) || !args) {
        return unsupported_expr(node);
    }
    const std::string_view kind = kind_of(callee);

    ast::CallExpr call;
    call.callee_range = range_of(callee);
    call.args = std::move(*args);

    if (kind == "identifier") {
        call.callee = std::string(text(callee));
        return {range, std::move(call)};
    }
    if (kind == "primitive_type" || kind == "sized_type_specifier") {
        // Functional cast: `int(x)`.
        if (call.args.size() != 1) {
            return unsupported_expr(node);
        }
        ast::CastExpr cast;
        cast.kind = ast::CastKind::Functional;
        auto spec = convert_type_node(callee);
        if (!spec) {
            return unsupported_expr(node);
        }
        cast.type.spec = std::move(*spec);
        cast.operand = std::make_unique<ast::Expr>(std::move(call.args.front()));
        return {range, std::move(cast)};
    }
    TSNode name = callee;
    while (kind_of(name) == "qualified_identifier") {
        const TSNode scope = field(name, "scope");
        if (ts_node_is_null(scope) || !is_name_kind(kind_of(scope))) {
            return unsupported_expr(node);
        }
        call.scope.emplace_back(text(scope));
        name = field(name, "name");
        if (ts_node_is_null(name)) {
            return unsupported_expr(node);
        }
    }
    if (kind_of(name) == "identifier") {
        call.callee = std::string(text(name));
        return {range, std::move(call)};
    }
    if (kind_of(name) == "template_function") {
        const TSNode fname = field(name, "name");
        auto targs = convert_template_args(field(name, "arguments"));
        if (ts_node_is_null(fname) || !targs) {
            return unsupported_expr(node);
        }
        const auto spelling = text(fname);
        if (call.scope.empty() && spelling == "static_cast") {
            if (targs->size() != 1 || call.args.size() != 1) {
                return unsupported_expr(node);
            }
            ast::CastExpr cast;
            cast.kind = ast::CastKind::Static;
            cast.type = std::move(targs->front());
            cast.operand = std::make_unique<ast::Expr>(std::move(call.args.front()));
            return {range, std::move(cast)};
        }
        if (spelling == "dynamic_cast" || spelling == "const_cast" || spelling == "reinterpret_cast") {
            return unsupported_expr(node);
        }
        call.callee = std::string(spelling);
        call.template_args = std::move(*targs);
        return {range, std::move(call)};
    }
    if (kind == "field_expression") {
        call.callee_expr = convert_expr_ptr(callee);
        return {range, std::move(call)};
    }
    if (!call.scope.empty()) {
        return unsupported_expr(node);
    }
    call.callee_expr = convert_expr_ptr(callee);
    return {range, std::move(call)};
}

ast::Expr TreeConverter::convert_new(TSNode node) const {
    const SourceRange range = range_of(node);
    if (has_field(node, "placement")) {
        return unsupported_expr(node);
    }
    ast::NewExpr n;
    auto spec = convert_type_node(field(node, "type"));
    if (!spec) {
        return unsupported_expr(node);
    }
    n.type.spec = std::move(*spec);
    const TSNode declarator = field(node, "declarator");
    if (!ts_node_is_null(declarator)) {
        if (kind_of(declarator) != "new_declarator" || has_field(declarator, "declarator")) {
            return unsupported_expr(node);  // pointers in new-type, multidimensional arrays
        }
        const TSNode length = field(declarator, "length");
        if (ts_node_is_null(length)) {
            return unsupported_expr(node);
        }
        n.array_size = convert_expr_ptr(length);
    }
    const TSNode arguments = field(node, "arguments");
    if (!ts_node_is_null(arguments)) {
        n.has_parens = true;
        for (const TSNode arg : named_children(arguments)) {
            n.args.push_back(convert_expr(arg));
        }
    }
    return {range, std::move(n)};
}

ast::Expr TreeConverter::convert_lambda(TSNode node) const {
    const SourceRange range = range_of(node);
    ast::LambdaExpr lambda;
    const TSNode captures = field(node, "captures");
    if (!ts_node_is_null(captures)) {
        bool reference = false;
        const std::uint32_t count = ts_node_child_count(captures);
        for (std::uint32_t i = 0; i < count; ++i) {
            const TSNode part = ts_node_child(captures, i);
            const std::string_view kind = kind_of(part);
            if (kind == "&") {
                reference = true;
            } else if (kind == "lambda_default_capture") {
                lambda.default_capture = text(part) == "&" ? '&' : '=';
            } else if (kind == "this") {
                lambda.captures_this = true;
            } else if (kind == "identifier") {
                lambda.captures.push_back(
                    ast::LambdaCapture{std::string(text(part)), reference, nullptr, range_of(part)});
                reference = false;
            } else if (kind == "lambda_capture_initializer") {
                const TSNode left = field(part, "left");
                const TSNode right = field(part, "right");
                if (ts_node_is_null(left) || ts_node_is_null(right)) {
                    return unsupported_expr(node);
                }
                lambda.captures.push_back(
                    ast::LambdaCapture{std::string(text(left)), reference, convert_expr_ptr(right), range_of(part)});
                reference = false;
            } else if (kind == "," || kind == "[" || kind == "]") {
                reference = false;
            } else {
                return unsupported_expr(node);  // `*this`, pack expansions...
            }
        }
    }
    auto fn = std::make_shared<ast::FunctionDecl>();
    fn->range = range;
    fn->declarator.name = "operator()";
    fn->declarator.range = range;
    fn->return_type.name = "auto";
    fn->return_type.range = range;
    const TSNode declarator = field(node, "declarator");
    if (!ts_node_is_null(declarator)) {
        const TSNode params = field(declarator, "parameters");
        if (!ts_node_is_null(params)) {
            for (const TSNode p : named_children(params)) {
                const std::string_view kind = kind_of(p);
                if (kind != "parameter_declaration" && kind != "optional_parameter_declaration") {
                    return unsupported_expr(node);
                }
                ast::Param param;
                param.range = range_of(p);
                auto type = convert_type_spec(p);
                if (!type) {
                    return unsupported_expr(node);
                }
                param.type = std::move(*type);
                const TSNode d = field(p, "declarator");
                if (!ts_node_is_null(d) && !convert_declarator(d, param.declarator)) {
                    return unsupported_expr(node);
                }
                fn->params.push_back(std::move(param));
            }
        }
        const std::uint32_t count = ts_node_named_child_count(declarator);
        for (std::uint32_t i = 0; i < count; ++i) {
            const TSNode child = ts_node_named_child(declarator, i);
            if (kind_of(child) == "type_qualifier" && text(child) == "mutable") {
                lambda.is_mutable = true;
            } else if (kind_of(child) == "trailing_return_type") {
                auto inner = first_named_child(child);
                auto desc = inner ? convert_type_desc(*inner) : std::nullopt;
                if (!desc) {
                    return unsupported_expr(node);
                }
                fn->return_type = std::move(desc->spec);
                fn->declarator.parts = std::move(desc->declarator.parts);
                lambda.has_return_type = true;
            }
        }
    }
    const TSNode body = field(node, "body");
    if (ts_node_is_null(body) || kind_of(body) != "compound_statement") {
        return unsupported_expr(node);
    }
    fn->body = std::make_unique<ast::Stmt>(make_stmt(body, convert_block(body)));
    lambda.function = std::move(fn);
    return {range, std::move(lambda)};
}

ast::Expr TreeConverter::convert_number(TSNode node) const {
    const SourceRange range = range_of(node);
    const std::string_view spelling = text(node);
    // tree-sitter folds a leading sign into the literal (`-1`): C++ sees a
    // unary operator applied to `1`.
    if (!spelling.empty() && (spelling.front() == '-' || spelling.front() == '+')) {
        const bool minus = spelling.front() == '-';
        std::size_t skip = 1;
        while (skip < spelling.size() && (spelling[skip] == ' ' || spelling[skip] == '\t')) {
            ++skip;
        }
        ast::Expr inner = convert_number_text(spelling.substr(skip), range);
        return {range, ast::UnaryExpr{minus ? ast::UnaryOp::Minus : ast::UnaryOp::Plus,
                                      std::make_unique<ast::Expr>(std::move(inner))}};
    }
    return convert_number_text(spelling, range);
}

ast::Expr TreeConverter::convert_number_text(std::string_view spelling, SourceRange range) const {
    const auto feature = NodeFeatureMap::number_literal_feature(spelling);
    if (feature == Feature::FloatingPoint) {
        // Without digit separators (1'000.5) and the suffix (3.0f, 2.5L); in a
        // hexadecimal literal an `f` before the `p` exponent is a digit.
        std::string digits;
        for (const char c : spelling) {
            if (c != '\'') {
                digits.push_back(c);
            }
        }
        const bool hex = digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X');
        if (!hex || digits.find_first_of("pP") != std::string::npos) {
            while (!digits.empty() &&
                   (digits.back() == 'f' || digits.back() == 'F' || digits.back() == 'l' || digits.back() == 'L')) {
                digits.pop_back();
            }
        }
        const auto value = detail::NumberFormat::parse(digits);
        if (!value) {
            return {range, ast::Unsupported{"number_literal", {ast::FeatureUse{Feature::FloatingPoint, range}}}};
        }
        return {range, ast::FloatLiteral{*value, std::string(spelling)}};
    }
    ast::IntLiteral literal;
    literal.text = std::string(spelling);
    if (feature) {
        literal.requires_feature = ast::FeatureUse{*feature, range};
    }
    std::size_t end = spelling.size();
    while (end > 0 && std::string_view("uUlLzZ").find(spelling[end - 1]) != std::string_view::npos) {
        --end;
    }
    const auto suffix = spelling.substr(end);
    literal.is_unsigned = suffix.find_first_of("uU") != std::string_view::npos;
    literal.is_long = suffix.find_first_of("lL") != std::string_view::npos;
    literal.is_decimal = spelling.size() == 1 || spelling[0] != '0';
    if (auto value = IntegerLiteralParser::parse(spelling)) {
        literal.value = static_cast<std::int64_t>(*value);  // bits; values above INT64_MAX are negative here
    } else {
        literal.out_of_range = true;
    }
    return {range, std::move(literal)};
}

// =============================================================================
// Helpers
// =============================================================================

std::string_view TreeConverter::text(TSNode node) const noexcept {
    return text_of(node, source_);
}

std::string_view TreeConverter::operator_of(TSNode node) const noexcept {
    const TSNode op = field(node, "operator");
    return ts_node_is_null(op) ? std::string_view{} : text(op);
}

std::optional<std::string> TreeConverter::decode_escapes(std::string_view body) {
    std::string out;
    for (std::size_t i = 0; i < body.size(); ++i) {
        const char c = body[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (++i >= body.size()) {
            return std::nullopt;
        }
        const char e = body[i];
        switch (e) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case 'a': out.push_back('\a'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'v': out.push_back('\v'); break;
            case '\\': out.push_back('\\'); break;
            case '\'': out.push_back('\''); break;
            case '"': out.push_back('"'); break;
            case '?': out.push_back('?'); break;
            case 'x': {
                unsigned value = 0;
                std::size_t digits = 0;
                while (i + 1 < body.size() && std::isxdigit(static_cast<unsigned char>(body[i + 1])) != 0 &&
                       digits < 2) {
                    const char h = body[++i];
                    value = value * 16 + static_cast<unsigned>(h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
                    ++digits;
                }
                if (digits == 0) {
                    return std::nullopt;
                }
                out.push_back(static_cast<char>(value));
                break;
            }
            default:
                if (e >= '0' && e <= '7') {
                    auto value = static_cast<unsigned>(e - '0');
                    std::size_t digits = 1;
                    while (i + 1 < body.size() && body[i + 1] >= '0' && body[i + 1] <= '7' && digits < 3) {
                        value = value * 8 + static_cast<unsigned>(body[++i] - '0');
                        ++digits;
                    }
                    out.push_back(static_cast<char>(value));
                    break;
                }
                return std::nullopt;
        }
    }
    return out;
}

}  // namespace cppi::parse
