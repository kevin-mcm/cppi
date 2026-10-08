#pragma once

/// Statement and declaration nodes of the AST. Declarations are statements
/// too: in script mode, functions, structs and statements share the top level.

#include "ast/Expr.hpp"
#include "ast/FeatureUse.hpp"
#include "ast/Unsupported.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace cppi::ast {

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

// --- Declarations -------------------------------------------------------------------

enum class InitStyle : std::uint8_t { None, Copy, Direct, List };

/// One name in a declaration: `x = 1` in `int x = 1, y;`.
struct VarDeclarator {
    Declarator declarator;
    InitStyle style = InitStyle::None;
    ExprPtr init;            ///< Copy (`= e`) or List (`{...}`, as an InitList)
    std::vector<Expr> args;  ///< Direct (`(a, b)`)
    SourceRange range;
};

struct DeclStmt {
    TypeSpec type;
    std::vector<VarDeclarator> vars;
};

enum class Access : std::uint8_t { Public, Protected, Private };

struct Param {
    TypeSpec type;
    Declarator declarator;
    ExprPtr default_value;
    SourceRange range;
};

/// `x(1)` in a constructor's member initializer list.
struct MemberInit {
    std::string name;
    SourceRange range;
    std::vector<Expr> args;
};

struct FunctionDecl {
    TypeSpec return_type;
    Declarator declarator;           ///< name and return-type modifiers
    std::vector<std::string> scope;  ///< `Point::move` defined outside its class
    std::vector<Param> params;
    std::vector<MemberInit> inits;  ///< constructors only
    StmtPtr body;                   ///< null for a prototype
    Access access = Access::Public;
    bool is_virtual = false;
    bool is_pure = false;   ///< `= 0`
    bool is_const = false;  ///< `void f() const`
    bool is_static = false;
    bool is_override = false;
    bool is_final = false;
    bool is_constructor = false;
    bool is_destructor = false;
    bool is_conversion = false;  ///< `operator bool()`: the return type is the target type
    bool is_explicit = false;
    bool is_deleted = false;                ///< `= delete` (C++11)
    bool is_defaulted = false;              ///< `= default` (C++11)
    std::shared_ptr<Expr> requires_clause;  ///< `T f(T x) requires C<T>` (C++20)
    /// A prototype that could also be an object with direct initialization
    /// (`std::vector<bool> seen(n * m, false);`): that reading, with every
    /// parameter written as an expression. Analysis picks one once names are known.
    std::shared_ptr<DeclStmt> object_reading;
    SourceRange range;
};

struct BaseSpec {
    std::string name;  ///< as written, for messages
    SourceRange range;
    TypeSpec type;  ///< `std::exception`, `Box<int>`
    Access access = Access::Public;
    bool is_virtual = false;
};

struct FieldDecl {
    TypeSpec type;
    std::vector<VarDeclarator> vars;
    Access access = Access::Public;
    SourceRange range;
};

struct TemplateParam {
    std::string name;
    SourceRange range;
    std::optional<TypeDesc> default_type;  ///< `class U = int`
    std::optional<TypeSpec> constraint;    ///< `Number T` (C++20): the concept T must satisfy
};

/// `template <class T> concept Number = ...;` (C++20)
struct ConceptDef {
    std::string name;
    SourceRange name_range;
    ExprPtr constraint;
};

/// `template <class T> T get() const;` inside a class.
struct MethodTemplate {
    std::vector<TemplateParam> params;
    ExprPtr requires_clause;  ///< `template <class T> requires C<T> ...`
    FunctionDecl decl;
};

struct RecordDef {
    bool is_class = false;  ///< `class` (private by default) vs `struct`
    std::string name;
    SourceRange name_range;
    std::vector<BaseSpec> bases;
    std::vector<FieldDecl> fields;
    std::vector<FunctionDecl> methods;
    std::vector<MethodTemplate> method_templates;
    bool has_body = true;  ///< false for `struct P;`
    bool is_final = false;
};

struct Enumerator {
    std::string name;
    ExprPtr value;
    SourceRange range;
};

struct EnumDef {
    std::string name;
    SourceRange name_range;
    bool scoped = false;  ///< enum class
    std::vector<Enumerator> enumerators;
};

/// `typedef int Score;` / `using Score = int;`
struct AliasDecl {
    std::string name;
    TypeDesc type;
};

/// `template <typename T> ...` around a function or a struct/class.
struct TemplateDecl {
    std::vector<TemplateParam> params;
    StmtPtr inner;
    ExprPtr requires_clause;  ///< `template <class T> requires C<T>`
};

// --- Statements ---------------------------------------------------------------------

struct ExprStmt {
    Expr expr;
};

struct Block {
    std::vector<Stmt> statements;
};

/// The condition of if/while/switch: an expression, or a declaration
/// (`if (int n = count())`).
struct Condition {
    std::unique_ptr<DeclStmt> decl;
    ExprPtr expr;
    SourceRange range;
};

struct IfStmt {
    bool is_constexpr = false;
    Condition condition;
    StmtPtr then_stmt;
    StmtPtr else_stmt;
};

struct WhileStmt {
    Condition condition;
    StmtPtr body;
};

struct DoWhileStmt {
    StmtPtr body;
    ExprPtr condition;
};

struct ForStmt {
    StmtPtr init;  ///< declaration or expression statement, may be null
    ExprPtr condition;
    ExprPtr update;
    StmtPtr body;
};

/// `for (int x : values)` (C++11).
struct RangeForStmt {
    DeclStmt variable;
    ExprPtr range_expr;
    StmtPtr body;
};

struct BreakStmt {};
struct ContinueStmt {};

struct ReturnStmt {
    ExprPtr value;
};

/// `throw value;`, or `throw;` (rethrow the exception being handled) when value is null.
struct ThrowStmt {
    ExprPtr value;
};

/// `catch (const std::exception& e) { ... }`; `catch (...)` has no parameter.
struct CatchClause {
    std::optional<Param> param;
    StmtPtr body;
    SourceRange range;
};

struct TryStmt {
    StmtPtr body;
    std::vector<CatchClause> catches;
};

struct SwitchCase {
    ExprPtr value;  ///< null for `default:`
    SourceRange range;
    std::vector<Stmt> statements;
};

struct SwitchStmt {
    Condition condition;
    std::vector<SwitchCase> cases;
};

struct StaticAssert {
    ExprPtr condition;
    std::string message;
};

struct FunctionDef {
    FunctionDecl decl;
};

struct Stmt;

/// `namespace geo { ... }`
struct NamespaceDef {
    std::string name;
    SourceRange name_range;
    std::vector<Stmt> body;
};

/// `using namespace std;` (a directive) or `using std::swap;` (a declaration).
struct UsingDecl {
    bool directive = false;
    std::vector<std::string> scope;
    std::string name;
};

struct Stmt {
    SourceRange range;
    /// Language features this statement uses itself, excluding nested
    /// statements (they carry their own). Checked before analysis.
    std::vector<FeatureUse> uses;
    std::variant<ExprStmt, DeclStmt, Block, IfStmt, WhileStmt, DoWhileStmt, ForStmt, RangeForStmt, BreakStmt,
                 ContinueStmt, ReturnStmt, SwitchStmt, StaticAssert, FunctionDef, RecordDef, EnumDef, AliasDecl,
                 TemplateDecl, NamespaceDef, UsingDecl, ThrowStmt, TryStmt, ConceptDef, Unsupported>
        node;
};

}  // namespace cppi::ast
