#pragma once

/// @file Expr.hpp
/// @brief Expression nodes of the AST, plus the syntactic description of types
/// they can mention (casts, sizeof).
///
/// The AST is independent of tree-sitter: if the parser is ever replaced,
/// everything after this point stays the same.
///
/// Nodes are plain value types held in std::variant and processed with
/// std::visit (the modern, closed-set form of the Visitor pattern).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "ast/FeatureUse.hpp"
#include "ast/Unsupported.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cppi::ast {

/// Forward declaration: expressions nest.
struct Expr;
/// Owning pointer to a sub-expression (null when absent).
using ExprPtr = std::unique_ptr<Expr>;

// --- Types as written in the source ------------------------------------------

/// The decl-specifiers of a declaration: `const int`, `Point`, `long long`.
struct TypeDesc;

/// The type part of a declaration (decl-specifiers): the base type name,
/// its qualifiers and storage specifiers.
struct TypeSpec {
    std::string name;  ///< "int", "long long", "double", "Point", ... ("" if missing)
    /// Where the type is written.
    SourceRange range;
    /// `const` given.
    bool is_const = false;
    /// `static` given.
    bool is_static = false;
    /// `constexpr` given.
    bool is_constexpr = false;
    bool is_inline = false;                     ///< `inline static int n = 0;` (C++17)
    std::vector<std::string> scope;             ///< `Shape::Kind` -> {"Shape"}
    std::shared_ptr<TypeSpec> auto_constraint;  ///< `std::integral auto` (C++20): the concept
    std::vector<TypeDesc> template_args;        ///< `Box<int>` -> {int}
};

/// What one DeclaratorPart adds: `*`, `&`, `&&` or `[n]`.
enum class DeclaratorKind : std::uint8_t { Pointer, Reference, RvalueReference, Array };

/// One step of a declarator, from the outside in: `int* a[3]` is
/// Pointer applied to Array(3)... see TypeResolver for the exact order.
struct DeclaratorPart {
    DeclaratorKind kind = DeclaratorKind::Pointer;
    ExprPtr size;           ///< arrays only; null for `int a[]`
    bool is_const = false;  ///< `int* const p`
    /// Where this part is written.
    SourceRange range;
};

/// The name being declared and how it modifies the base type.
struct Declarator {
    std::string name;  ///< empty for abstract declarators (`(int*)p`)
    /// Where the name is written.
    SourceRange range;
    std::vector<std::string> scope;     ///< `int Counter::count = 0;` defines a static member
    std::vector<DeclaratorPart> parts;  ///< outermost first
    std::vector<std::string> bindings;  ///< structured binding `auto [x, y]` (C++17)
};

/// A complete type written in an expression: `(int)x`, `sizeof(Point)`.
struct TypeDesc {
    /// Base type and qualifiers.
    TypeSpec spec;
    /// Pointers, references and arrays applied to it (no name).
    Declarator declarator;
};

// --- Operators ------------------------------------------------------------------

/// Prefix unary operators: `+x`, `-x`, `!x`, `~x`, `&x`, `*x`.
enum class UnaryOp : std::uint8_t { Plus, Minus, Not, BitNot, AddressOf, Deref };

/// Binary operators. Assignments are AssignExpr; `&&`, `||` and `,`
/// evaluate their operands in order.
enum class BinaryOp : std::uint8_t {
    /// `+`
    Add,
    /// `-`
    Sub,
    /// `*`
    Mul,
    /// `/`
    Div,
    /// `%`
    Mod,
    /// `<<`
    Shl,
    /// `>>`
    Shr,
    /// `&`
    BitAnd,
    /// `|`
    BitOr,
    /// `^`
    BitXor,
    /// `&&`
    LogicalAnd,
    /// `||`
    LogicalOr,
    /// `==`
    Eq,
    /// `!=`
    Ne,
    /// `<`
    Lt,
    /// `>`
    Gt,
    /// `<=`
    Le,
    /// `>=`
    Ge,
    /// `,`
    Comma,
};

// --- Expression nodes -------------------------------------------------------------

/// An integer literal: `42`, `0x2A`, `42UL`.
struct IntLiteral {
    std::int64_t value = 0;     ///< the bits of the value (above INT64_MAX: negative)
    std::string text;           ///< spelling in the source
    bool out_of_range = false;  ///< does not fit in 64 bits
    bool is_long = false;       ///< `L` / `LL` suffix
    bool is_unsigned = false;   ///< `U` suffix
    bool is_decimal = true;     ///< hex, octal and binary literals may become unsigned
    /// Set when the spelling needs a newer standard (0b101, 1'000).
    std::optional<FeatureUse> requires_feature;
};

/// A floating-point literal: `3.14`, `1e-3`.
struct FloatLiteral {
    /// The parsed value.
    double value = 0;
    /// Spelling in the source.
    std::string text;
};

/// `true` or `false`.
struct BoolLiteral {
    /// The literal's value.
    bool value = false;
};

/// A character literal: `'a'`, `'\n'`.
struct CharLiteral {
    /// The character's code.
    std::int64_t value = 0;
};

/// A string literal, with escapes already resolved.
struct StringLiteral {
    /// The characters, without the terminating null.
    std::string value;
};

/// `nullptr` (C++11).
struct NullptrLiteral {};

/// A name used as an expression, possibly qualified.
struct Identifier {
    /// The unqualified name.
    std::string name;
    std::vector<std::string> scope;  ///< `Color::Red` -> {"Color"}
};

/// A function call: `f(a, b)`, `obj.f(a)`, `Base::f()`, `max<int>(a, b)`.
struct CallExpr {
    std::string callee;  ///< name, when the callee is a plain identifier
    /// Where the callee is written.
    SourceRange callee_range;
    /// Arguments, in order.
    std::vector<Expr> args;
    std::vector<std::string> scope;       ///< `Base::f()` -> {"Base"}
    std::vector<TypeDesc> template_args;  ///< `max<int>(a, b)`
    ExprPtr callee_expr;                  ///< set when the callee is not a plain name (`obj.f`)
    bool concept_id = false;              ///< `Number<int>`: no parentheses (a concept)
};

/// Forward declaration (see Stmt.hpp).
struct Param;

/// One requirement of a requires-expression.
struct Requirement {
    /// `expr;`, `{ expr } -> Concept<...>;` or `typename T::type;`.
    enum class Kind : std::uint8_t { Simple, Compound, Type };
    /// Which form of requirement it is.
    Kind kind = Kind::Simple;
    ExprPtr expr;                        ///< Simple and Compound: must be valid
    std::optional<TypeSpec> constraint;  ///< Compound `{ e } -> C<...>`: C<decltype(e), ...> holds
    std::optional<TypeDesc> type;        ///< Type `typename T::value_type;`
    /// Where the requirement is written.
    SourceRange range;
};

/// `requires (T a, T b) { a + b; }` (C++20): true if every requirement is valid.
struct RequiresExpr {
    /// The parameters introduced by `requires (...)`.
    std::vector<std::shared_ptr<Param>> params;
    /// The requirements, in order.
    std::vector<Requirement> requirements;
};

/// A prefix unary operator applied to an operand: `-x`, `!ok`, `*p`.
struct UnaryExpr {
    /// The operator.
    UnaryOp op = UnaryOp::Plus;
    /// The operand.
    ExprPtr operand;
};

/// A binary operator applied to two operands: `a + b`, `x < y`.
struct BinaryExpr {
    /// The operator.
    BinaryOp op = BinaryOp::Add;
    /// Left operand.
    ExprPtr lhs;
    /// Right operand.
    ExprPtr rhs;
};

/// `a = b`, or a compound assignment such as `a += b` (op is the arithmetic).
struct AssignExpr {
    /// The arithmetic of a compound assignment; empty for plain `=`.
    std::optional<BinaryOp> op;
    /// The object assigned to.
    ExprPtr lhs;
    /// The value assigned.
    ExprPtr rhs;
};

/// `++x`, `x++`, `--x` or `x--`.
struct IncDecExpr {
    /// `++` (true) or `--` (false).
    bool increment = true;
    /// Prefix (`++x`) or postfix (`x++`).
    bool prefix = true;
    /// The object incremented or decremented.
    ExprPtr operand;
};

/// `condition ? then_expr : else_expr`.
struct ConditionalExpr {
    /// The condition.
    ExprPtr condition;
    /// Evaluated when the condition is true.
    ExprPtr then_expr;
    /// Evaluated when the condition is false.
    ExprPtr else_expr;
};

/// `base[index]`.
struct SubscriptExpr {
    /// The array, pointer or object subscripted.
    ExprPtr base;
    /// The index.
    ExprPtr index;
};

/// Member access: `obj.member` or `ptr->member`.
struct MemberExpr {
    /// The object (or pointer, with `->`).
    ExprPtr base;
    /// Name of the member.
    std::string member;
    /// Where the member name is written.
    SourceRange member_range;
    /// `->` (true) or `.` (false).
    bool arrow = false;
    std::vector<TypeDesc> template_args;  ///< `box.get<int>()`: a member function template
};

/// `(T)x`, `static_cast<T>(x)` or `T(x)`.
enum class CastKind : std::uint8_t { CStyle, Static, Functional };

/// An explicit type conversion.
struct CastExpr {
    /// How the cast is spelled.
    CastKind kind = CastKind::CStyle;
    /// The target type.
    TypeDesc type;
    /// The value converted.
    ExprPtr operand;
};

/// `sizeof(Type)` or `sizeof expr`.
struct SizeofExpr {
    /// When the operand is a type.
    std::optional<TypeDesc> type;
    ExprPtr operand;  ///< when the operand is an expression
};

/// `{1, 2, 3}`: only valid as an initializer.
struct InitList {
    /// The elements, in order.
    std::vector<Expr> elements;
};

/// `this`.
struct ThisExpr {};

/// `new T`, `new T(args)`, `new T{...}` or `new T[n]`.
struct NewExpr {
    /// The type allocated.
    TypeDesc type;
    ExprPtr array_size;       ///< `new int[n]`
    std::vector<Expr> args;   ///< `new Point(1, 2)`
    bool has_parens = false;  ///< `new int()` value-initializes
};

/// Forward declaration (see Stmt.hpp).
struct FunctionDecl;

/// One explicit capture of a lambda: `x`, `&x` or `n = 3`.
struct LambdaCapture {
    /// The captured name.
    std::string name;
    /// `&x` (true) or `x` (false).
    bool by_reference = false;
    ExprPtr init;  ///< `[n = 3]` (C++14)
    /// Where the capture is written.
    SourceRange range;
};

/// A lambda expression (C++11).
struct LambdaExpr {
    char default_capture = 0;  ///< 0, '=' or '&'
    /// `[this]` given.
    bool captures_this = false;
    /// `mutable` given.
    bool is_mutable = false;
    bool has_return_type = false;  ///< `-> T` given; otherwise deduced from the returns
    /// Explicit captures, in order.
    std::vector<LambdaCapture> captures;
    std::shared_ptr<FunctionDecl> function;  ///< parameters, return type and body
};

/// `delete p` or `delete[] p`.
struct DeleteExpr {
    /// `delete[]` (true) or `delete` (false).
    bool array = false;
    /// The pointer deleted.
    ExprPtr operand;
};

/// An expression node: its source range plus one of the node types above.
struct Expr {
    /// The whole expression in the source.
    SourceRange range;
    /// The node itself.
    std::variant<IntLiteral, FloatLiteral, BoolLiteral, CharLiteral, StringLiteral, NullptrLiteral, Identifier,
                 CallExpr, UnaryExpr, BinaryExpr, AssignExpr, IncDecExpr, ConditionalExpr, SubscriptExpr, MemberExpr,
                 CastExpr, SizeofExpr, InitList, ThisExpr, NewExpr, DeleteExpr, LambdaExpr, RequiresExpr, Unsupported>
        node;
};

}  // namespace cppi::ast
