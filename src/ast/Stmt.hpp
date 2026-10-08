#pragma once

/// @file Stmt.hpp
/// @brief Statement and declaration nodes of the AST.
///
/// Declarations are statements too: in script mode, functions, structs and
/// statements share the top level.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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

/// Forward declaration: statements nest.
struct Stmt;
/// Owning pointer to a sub-statement (null when absent).
using StmtPtr = std::unique_ptr<Stmt>;

// --- Declarations -------------------------------------------------------------------

/// How a variable is initialized: not at all, `= e`, `(args)` or `{...}`.
enum class InitStyle : std::uint8_t { None, Copy, Direct, List };

/// One name in a declaration: `x = 1` in `int x = 1, y;`.
struct VarDeclarator {
    /// Name, pointers, references and array bounds.
    Declarator declarator;
    /// Which initializer form is used.
    InitStyle style = InitStyle::None;
    ExprPtr init;            ///< Copy (`= e`) or List (`{...}`, as an InitList)
    std::vector<Expr> args;  ///< Direct (`(a, b)`)
    /// The whole declarator, with its initializer.
    SourceRange range;
};

/// A variable declaration: `const int x = 1, y;`.
struct DeclStmt {
    /// The type shared by every declarator.
    TypeSpec type;
    /// The names declared, in order.
    std::vector<VarDeclarator> vars;
};

/// Member access: `public`, `protected` or `private`.
enum class Access : std::uint8_t { Public, Protected, Private };

/// A function parameter: `const Point& p = {}`.
struct Param {
    /// Parameter type.
    TypeSpec type;
    /// Parameter name (may be empty) and modifiers.
    Declarator declarator;
    /// Default argument; null if none.
    ExprPtr default_value;
    /// The whole parameter.
    SourceRange range;
};

/// `x(1)` in a constructor's member initializer list.
struct MemberInit {
    /// The member (or base class, or delegated-to class) initialized.
    std::string name;
    /// Where the initializer is written.
    SourceRange range;
    /// Constructor arguments.
    std::vector<Expr> args;
};

/// A function declaration or definition, free or member, with every
/// specifier the analyzer needs.
struct FunctionDecl {
    /// Declared return type (`auto` when deduced or trailing).
    TypeSpec return_type;
    Declarator declarator;           ///< name and return-type modifiers
    std::vector<std::string> scope;  ///< `Point::move` defined outside its class
    /// Parameters, in order.
    std::vector<Param> params;
    std::vector<MemberInit> inits;  ///< constructors only
    StmtPtr body;                   ///< null for a prototype
    /// Access, for member functions.
    Access access = Access::Public;
    /// `virtual` given.
    bool is_virtual = false;
    bool is_pure = false;   ///< `= 0`
    bool is_const = false;  ///< `void f() const`
    /// `static` member function.
    bool is_static = false;
    /// `override` given.
    bool is_override = false;
    /// `final` given.
    bool is_final = false;
    /// A constructor.
    bool is_constructor = false;
    /// A destructor.
    bool is_destructor = false;
    bool is_conversion = false;  ///< `operator bool()`: the return type is the target type
    /// `explicit` given.
    bool is_explicit = false;
    bool is_deleted = false;                ///< `= delete` (C++11)
    bool is_defaulted = false;              ///< `= default` (C++11)
    std::shared_ptr<Expr> requires_clause;  ///< `T f(T x) requires C<T>` (C++20)
    /// A prototype that could also be an object with direct initialization
    /// (`std::vector<bool> seen(n * m, false);`): that reading, with every
    /// parameter written as an expression. Analysis picks one once names are known.
    std::shared_ptr<DeclStmt> object_reading;
    /// The whole declaration.
    SourceRange range;
};

/// One base class of a class: `public virtual Shape`.
struct BaseSpec {
    std::string name;  ///< as written, for messages
    /// Where the base is written.
    SourceRange range;
    TypeSpec type;  ///< `std::exception`, `Box<int>`
    /// Inheritance access.
    Access access = Access::Public;
    /// Virtual inheritance.
    bool is_virtual = false;
};

/// A data member declaration: `int x = 0, y = 0;` inside a class.
struct FieldDecl {
    /// The type shared by every declarator.
    TypeSpec type;
    /// The fields declared, with their default member initializers.
    std::vector<VarDeclarator> vars;
    /// Access of the fields.
    Access access = Access::Public;
    /// The whole declaration.
    SourceRange range;
};

/// A template type parameter: `class T`, `typename U = int`, `Number T`.
struct TemplateParam {
    /// The parameter's name.
    std::string name;
    /// Where the parameter is written.
    SourceRange range;
    std::optional<TypeDesc> default_type;  ///< `class U = int`
    std::optional<TypeSpec> constraint;    ///< `Number T` (C++20): the concept T must satisfy
};

/// `template <class T> concept Number = ...;` (C++20)
struct ConceptDef {
    /// The concept's name.
    std::string name;
    /// Where the name is written.
    SourceRange name_range;
    /// The constraint expression the concept stands for.
    ExprPtr constraint;
};

/// `template <class T> T get() const;` inside a class.
struct MethodTemplate {
    /// The member template's parameters.
    std::vector<TemplateParam> params;
    ExprPtr requires_clause;  ///< `template <class T> requires C<T> ...`
    /// The member function itself.
    FunctionDecl decl;
};

/// A `struct` or `class` definition (or forward declaration).
struct RecordDef {
    bool is_class = false;  ///< `class` (private by default) vs `struct`
    /// The class name.
    std::string name;
    /// Where the name is written.
    SourceRange name_range;
    /// Base classes, in order.
    std::vector<BaseSpec> bases;
    /// Data members, in declaration order.
    std::vector<FieldDecl> fields;
    /// Member functions, constructors and destructor.
    std::vector<FunctionDecl> methods;
    /// Member function templates.
    std::vector<MethodTemplate> method_templates;
    bool has_body = true;  ///< false for `struct P;`
    /// `final` given.
    bool is_final = false;
};

/// One enumerator: `Red` or `Red = 1`.
struct Enumerator {
    /// Enumerator name.
    std::string name;
    /// Explicit value; null to continue from the previous one.
    ExprPtr value;
    /// Where the enumerator is written.
    SourceRange range;
};

/// An `enum` or `enum class` definition.
struct EnumDef {
    /// Enum name.
    std::string name;
    /// Where the name is written.
    SourceRange name_range;
    bool scoped = false;  ///< enum class
    /// Enumerators, in order.
    std::vector<Enumerator> enumerators;
};

/// `typedef int Score;` / `using Score = int;`
struct AliasDecl {
    /// The alias.
    std::string name;
    /// The type it stands for.
    TypeDesc type;
};

/// `template <typename T> ...` around a function or a struct/class.
struct TemplateDecl {
    /// Template parameters.
    std::vector<TemplateParam> params;
    /// The function or class being templated.
    StmtPtr inner;
    ExprPtr requires_clause;  ///< `template <class T> requires C<T>`
};

// --- Statements ---------------------------------------------------------------------

/// An expression used as a statement: `harvest();`.
struct ExprStmt {
    /// The expression.
    Expr expr;
};

/// A compound statement: `{ ... }`.
struct Block {
    /// The statements, in order.
    std::vector<Stmt> statements;
};

/// The condition of if/while/switch: an expression, or a declaration
/// (`if (int n = count())`).
struct Condition {
    /// The declaration form; null for a plain expression.
    std::unique_ptr<DeclStmt> decl;
    /// The expression form; null for a declaration.
    ExprPtr expr;
    /// Where the condition is written.
    SourceRange range;
};

/// `if (condition) then_stmt else else_stmt`.
struct IfStmt {
    /// `if constexpr` (C++17).
    bool is_constexpr = false;
    /// The condition.
    Condition condition;
    /// Runs when the condition is true.
    StmtPtr then_stmt;
    /// Runs otherwise; null without `else`.
    StmtPtr else_stmt;
};

/// `while (condition) body`.
struct WhileStmt {
    /// Checked before each iteration.
    Condition condition;
    /// The loop body.
    StmtPtr body;
};

/// `do body while (condition);`.
struct DoWhileStmt {
    /// The loop body, run at least once.
    StmtPtr body;
    /// Checked after each iteration.
    ExprPtr condition;
};

/// `for (init; condition; update) body`.
struct ForStmt {
    StmtPtr init;  ///< declaration or expression statement, may be null
    /// Checked before each iteration; null loops forever.
    ExprPtr condition;
    /// Evaluated after each iteration; may be null.
    ExprPtr update;
    /// The loop body.
    StmtPtr body;
};

/// `for (int x : values)` (C++11).
struct RangeForStmt {
    /// The loop variable.
    DeclStmt variable;
    /// The range iterated over.
    ExprPtr range_expr;
    /// The loop body.
    StmtPtr body;
};

/// `break;`
struct BreakStmt {};
/// `continue;`
struct ContinueStmt {};

/// `return;` or `return value;`.
struct ReturnStmt {
    /// The returned value; null for `return;`.
    ExprPtr value;
};

/// `throw value;`, or `throw;` (rethrow the exception being handled) when value is null.
struct ThrowStmt {
    /// The thrown value; null for `throw;`.
    ExprPtr value;
};

/// `catch (const std::exception& e) { ... }`; `catch (...)` has no parameter.
struct CatchClause {
    /// The caught parameter; empty for `catch (...)`.
    std::optional<Param> param;
    /// The handler.
    StmtPtr body;
    /// The whole clause.
    SourceRange range;
};

/// `try { ... } catch (...) { ... }`.
struct TryStmt {
    /// The protected block.
    StmtPtr body;
    /// Handlers, tried in order.
    std::vector<CatchClause> catches;
};

/// One `case value:` or `default:` label and the statements after it.
struct SwitchCase {
    ExprPtr value;  ///< null for `default:`
    /// Where the label is written.
    SourceRange range;
    /// Statements up to the next label.
    std::vector<Stmt> statements;
};

/// `switch (condition) { case ...: ... }`.
struct SwitchStmt {
    /// The value switched on.
    Condition condition;
    /// Labels, in source order.
    std::vector<SwitchCase> cases;
};

/// `static_assert(condition, "message");` (C++11).
struct StaticAssert {
    /// Must be a true constant expression.
    ExprPtr condition;
    /// Message shown when it fails; may be empty.
    std::string message;
};

/// A function declaration or definition used as a statement.
struct FunctionDef {
    /// The function.
    FunctionDecl decl;
};

/// Forward declaration: namespaces contain statements.
struct Stmt;

/// `namespace geo { ... }`
struct NamespaceDef {
    /// Namespace name.
    std::string name;
    /// Where the name is written.
    SourceRange name_range;
    /// The namespace's declarations.
    std::vector<Stmt> body;
};

/// `using namespace std;` (a directive) or `using std::swap;` (a declaration).
struct UsingDecl {
    /// `using namespace` (true) or a using-declaration (false).
    bool directive = false;
    /// Qualifiers before the name: `std::chrono` -> {"std"}.
    std::vector<std::string> scope;
    /// The namespace or the name brought into scope.
    std::string name;
};

/// A statement node: its source range, the features it uses, and one of the
/// node types above.
struct Stmt {
    /// The whole statement in the source.
    SourceRange range;
    /// Language features this statement uses itself, excluding nested
    /// statements (they carry their own). Checked before analysis.
    std::vector<FeatureUse> uses;
    /// The node itself.
    std::variant<ExprStmt, DeclStmt, Block, IfStmt, WhileStmt, DoWhileStmt, ForStmt, RangeForStmt, BreakStmt,
                 ContinueStmt, ReturnStmt, SwitchStmt, StaticAssert, FunctionDef, RecordDef, EnumDef, AliasDecl,
                 TemplateDecl, NamespaceDef, UsingDecl, ThrowStmt, TryStmt, ConceptDef, Unsupported>
        node;
};

}  // namespace cppi::ast
