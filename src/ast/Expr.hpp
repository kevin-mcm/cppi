#pragma once

/// Expression nodes of the AST, plus the syntactic description of types they
/// can mention (casts, sizeof). The AST is independent of tree-sitter: if the
/// parser is ever replaced, everything after this point stays the same.
///
/// Nodes are plain value types held in std::variant and processed with
/// std::visit (the modern, closed-set form of the Visitor pattern).

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

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

// --- Types as written in the source ------------------------------------------

/// The decl-specifiers of a declaration: `const int`, `Point`, `long long`.
struct TypeDesc;

struct TypeSpec {
    std::string name;  ///< "int", "long long", "double", "Point", ... ("" if missing)
    SourceRange range;
    bool is_const = false;
    bool is_static = false;
    bool is_constexpr = false;
    bool is_inline = false;                     ///< `inline static int n = 0;` (C++17)
    std::vector<std::string> scope;             ///< `Shape::Kind` -> {"Shape"}
    std::shared_ptr<TypeSpec> auto_constraint;  ///< `std::integral auto` (C++20): the concept
    std::vector<TypeDesc> template_args;        ///< `Box<int>` -> {int}
};

enum class DeclaratorKind : std::uint8_t { Pointer, Reference, RvalueReference, Array };

/// One step of a declarator, from the outside in: `int* a[3]` is
/// Pointer applied to Array(3)... see TypeResolver for the exact order.
struct DeclaratorPart {
    DeclaratorKind kind = DeclaratorKind::Pointer;
    ExprPtr size;           ///< arrays only; null for `int a[]`
    bool is_const = false;  ///< `int* const p`
    SourceRange range;
};

/// The name being declared and how it modifies the base type.
struct Declarator {
    std::string name;  ///< empty for abstract declarators (`(int*)p`)
    SourceRange range;
    std::vector<std::string> scope;     ///< `int Counter::count = 0;` defines a static member
    std::vector<DeclaratorPart> parts;  ///< outermost first
    std::vector<std::string> bindings;  ///< structured binding `auto [x, y]` (C++17)
};

/// A complete type written in an expression: `(int)x`, `sizeof(Point)`.
struct TypeDesc {
    TypeSpec spec;
    Declarator declarator;
};

// --- Operators ------------------------------------------------------------------

enum class UnaryOp : std::uint8_t { Plus, Minus, Not, BitNot, AddressOf, Deref };

enum class BinaryOp : std::uint8_t {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Shl,
    Shr,
    BitAnd,
    BitOr,
    BitXor,
    LogicalAnd,
    LogicalOr,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Comma,
};

// --- Expression nodes -------------------------------------------------------------

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

struct FloatLiteral {
    double value = 0;
    std::string text;
};

struct BoolLiteral {
    bool value = false;
};

struct CharLiteral {
    std::int64_t value = 0;
};

struct StringLiteral {
    std::string value;
};

struct NullptrLiteral {};

struct Identifier {
    std::string name;
    std::vector<std::string> scope;  ///< `Color::Red` -> {"Color"}
};

struct CallExpr {
    std::string callee;  ///< name, when the callee is a plain identifier
    SourceRange callee_range;
    std::vector<Expr> args;
    std::vector<std::string> scope;       ///< `Base::f()` -> {"Base"}
    std::vector<TypeDesc> template_args;  ///< `max<int>(a, b)`
    ExprPtr callee_expr;                  ///< set when the callee is not a plain name (`obj.f`)
    bool concept_id = false;              ///< `Number<int>`: no parentheses (a concept)
};

struct Param;

/// One requirement of a requires-expression.
struct Requirement {
    enum class Kind : std::uint8_t { Simple, Compound, Type };
    Kind kind = Kind::Simple;
    ExprPtr expr;                        ///< Simple and Compound: must be valid
    std::optional<TypeSpec> constraint;  ///< Compound `{ e } -> C<...>`: C<decltype(e), ...> holds
    std::optional<TypeDesc> type;        ///< Type `typename T::value_type;`
    SourceRange range;
};

/// `requires (T a, T b) { a + b; }` (C++20): true if every requirement is valid.
struct RequiresExpr {
    std::vector<std::shared_ptr<Param>> params;
    std::vector<Requirement> requirements;
};

struct UnaryExpr {
    UnaryOp op = UnaryOp::Plus;
    ExprPtr operand;
};

struct BinaryExpr {
    BinaryOp op = BinaryOp::Add;
    ExprPtr lhs;
    ExprPtr rhs;
};

/// `a = b`, or a compound assignment such as `a += b` (op is the arithmetic).
struct AssignExpr {
    std::optional<BinaryOp> op;
    ExprPtr lhs;
    ExprPtr rhs;
};

struct IncDecExpr {
    bool increment = true;
    bool prefix = true;
    ExprPtr operand;
};

struct ConditionalExpr {
    ExprPtr condition;
    ExprPtr then_expr;
    ExprPtr else_expr;
};

struct SubscriptExpr {
    ExprPtr base;
    ExprPtr index;
};

struct MemberExpr {
    ExprPtr base;
    std::string member;
    SourceRange member_range;
    bool arrow = false;
    std::vector<TypeDesc> template_args;  ///< `box.get<int>()`: a member function template
};

enum class CastKind : std::uint8_t { CStyle, Static, Functional };

struct CastExpr {
    CastKind kind = CastKind::CStyle;
    TypeDesc type;
    ExprPtr operand;
};

struct SizeofExpr {
    std::optional<TypeDesc> type;
    ExprPtr operand;  ///< when the operand is an expression
};

/// `{1, 2, 3}`: only valid as an initializer.
struct InitList {
    std::vector<Expr> elements;
};

struct ThisExpr {};

struct NewExpr {
    TypeDesc type;
    ExprPtr array_size;       ///< `new int[n]`
    std::vector<Expr> args;   ///< `new Point(1, 2)`
    bool has_parens = false;  ///< `new int()` value-initializes
};

struct FunctionDecl;

struct LambdaCapture {
    std::string name;
    bool by_reference = false;
    ExprPtr init;  ///< `[n = 3]` (C++14)
    SourceRange range;
};

struct LambdaExpr {
    char default_capture = 0;  ///< 0, '=' or '&'
    bool captures_this = false;
    bool is_mutable = false;
    bool has_return_type = false;  ///< `-> T` given; otherwise deduced from the returns
    std::vector<LambdaCapture> captures;
    std::shared_ptr<FunctionDecl> function;  ///< parameters, return type and body
};

struct DeleteExpr {
    bool array = false;
    ExprPtr operand;
};

struct Expr {
    SourceRange range;
    std::variant<IntLiteral, FloatLiteral, BoolLiteral, CharLiteral, StringLiteral, NullptrLiteral, Identifier,
                 CallExpr, UnaryExpr, BinaryExpr, AssignExpr, IncDecExpr, ConditionalExpr, SubscriptExpr, MemberExpr,
                 CastExpr, SizeofExpr, InitList, ThisExpr, NewExpr, DeleteExpr, LambdaExpr, RequiresExpr, Unsupported>
        node;
};

}  // namespace cppi::ast
