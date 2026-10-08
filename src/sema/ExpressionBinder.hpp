#pragma once

/// @file ExpressionBinder.hpp
/// @brief Binds AST expressions: resolves names, checks types, makes every
/// implicit conversion explicit and decides what is an object (lvalue) and what
/// is a value (rvalue).
///
/// A Visitor over ast::Expr.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisContext.hpp"
#include "sema/BoundTree.hpp"
#include "sema/MemberLookup.hpp"
#include "sema/Symbol.hpp"

#include "ast/Expr.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

/// Binds expressions: from AST expressions to typed, explicit bound expressions.
/// Functions returning BExprPtr return null after reporting an error.
class ExpressionBinder {
public:
    /// Where the expression appears: a function named as a whole statement
    /// (`harvest;`) is only a warning, but as a value it is an error.
    enum class Use : std::uint8_t { Statement, Value };

    /// @param ctx The shared analysis state.
    explicit ExpressionBinder(AnalysisContext& ctx) noexcept : ctx_(ctx), members_(ctx.types, ctx.hierarchy) {}

    /// The expression in its natural category (lvalue or rvalue), or null
    /// after reporting why it is invalid.
    [[nodiscard]] BExprPtr bind(const ast::Expr& expr, Use use = Use::Value);
    /// Scalars are loaded and arrays decay; records stay lvalues.
    [[nodiscard]] BExprPtr bind_rvalue(const ast::Expr& expr);
    /// An rvalue converted to `to`, reporting cannot-convert otherwise.
    [[nodiscard]] BExprPtr bind_converted(const ast::Expr& expr, TypeRef to);
    /// A condition (if, while, &&...): converted to bool.
    [[nodiscard]] BExprPtr bind_condition(const ast::Expr& expr);

    /// `expr` as an rvalue: scalars are loaded, arrays decay to pointers.
    [[nodiscard]] BExprPtr to_rvalue(BExprPtr expr);
    /// `rvalue` implicitly converted to `to` (through a conversion operator for
    /// classes), reporting cannot-convert otherwise.
    [[nodiscard]] BExprPtr convert_to(BExprPtr rvalue, TypeRef to);
    /// `expr` converted to bool for a condition (explicit operator bool allowed).
    [[nodiscard]] BExprPtr to_condition(BExprPtr expr);
    /// A class operand of a built-in operator through its conversion operator.
    [[nodiscard]] BExprPtr to_number(BExprPtr expr);
    /// Calls the conversion operator `function` on `object`.
    [[nodiscard]] BExprPtr call_conversion(BExprPtr object, std::uint32_t function);

    /// What a callee receives for a parameter of type `param`.
    [[nodiscard]] BExprPtr as_argument(BExprPtr arg, TypeRef param);

    /// A temporary in the current frame holding the scalar `value`.
    [[nodiscard]] BExprPtr materialize(BExprPtr value);

    /// `this` inside a member function (a pointer rvalue), or null.
    [[nodiscard]] BExprPtr this_pointer(SourceRange range);
    /// The object `*this`.
    [[nodiscard]] BExprPtr this_object(SourceRange range);

    /// The `base` subobject of the record lvalue `object`.
    [[nodiscard]] BExprPtr base_subobject(BExprPtr object, std::uint32_t base_record);

    /// Calls a resolved function with prepared arguments.
    [[nodiscard]] BExprPtr make_call(const Callee& callee, std::vector<BExprPtr> args, SourceRange range,
                                     std::optional<std::uint32_t> virtual_slot = std::nullopt);

    /// Member lookup, shared with the other binders.
    [[nodiscard]] MemberLookup& members() noexcept { return members_; }

    /// `object.name(args)`, reporting if there is no such member function.
    [[nodiscard]] BExprPtr call_method(BExprPtr object, const std::string& name, std::vector<BExprPtr> args,
                                       SourceRange range) {
        return call_candidates(name, {}, std::move(args), range, range, std::move(object), true);
    }

    /// Calls `name`: one of `candidates` or, with an `object`, one of its
    /// member functions (dispatched virtually if `dynamic`) or one of the
    /// `member_templates` instances. `templates_only`: `obj.f<int>()`.
    BExprPtr call_candidates(const std::string& name, const std::vector<Callee>& candidates, std::vector<BExprPtr> args,
                             SourceRange range, SourceRange callee_range, BExprPtr object, bool dynamic,
                             const std::vector<Callee>& member_templates = {}, bool templates_only = false);

    /// `new T(source)` for a class copied cell by cell.
    BExprPtr heap_copy(BNew node, BExprPtr source, TypeRef type, bool is_const, SourceRange range);
    /// `requires (T a) { a + a; }`: a compile-time bool.
    BExprPtr requires_expr(const ast::RequiresExpr& r, SourceRange range);
    /// Does `record` have a call operator (possibly a template: generic lambdas)?
    [[nodiscard]] bool is_function_object(std::uint32_t record) const;
    /// `f(args)` on a function object.
    BExprPtr call_function_object(BExprPtr object, std::vector<BExprPtr> args, SourceRange range,
                                  SourceRange callee_range);

    /// `Point(1, 2)`: a temporary object built by a constructor.
    [[nodiscard]] BExprPtr construct_temporary(TypeRef type, std::vector<BExprPtr> args, SourceRange range);

    // Small node builders shared with the other binders.
    /// A bound expression of `type` holding `node`.
    [[nodiscard]] static BExprPtr make(TypeRef type, bool lvalue, SourceRange range, auto node) {
        auto e = std::make_unique<BExpr>();
        e->type = type;
        e->lvalue = lvalue;
        e->range = range;
        e->node = std::move(node);
        return e;
    }
    /// A constant of `type` with the given bits.
    [[nodiscard]] static BExprPtr constant(TypeRef type, std::int64_t bits, SourceRange range) {
        return make(type, false, range, BConst{bits});
    }
    /// The variable `symbol` names, checking access to static data members.
    [[nodiscard]] BExprPtr variable(const Symbol& symbol, const std::string& name, SourceRange range) const;
    /// A static data member of `record` (or of a base), or nullptr.
    [[nodiscard]] Symbol* static_member(std::uint32_t record, std::string_view name) const;

private:
    /// Binds `expr` by node type (the work behind bind()).
    BExprPtr bind_node(const ast::Expr& expr, Use use);

    /// An integer literal, typed as the standard says (`int`, `long`, unsigned...).
    BExprPtr int_literal(const ast::IntLiteral& lit, SourceRange range);
    /// A name: a variable, constant, function or member.
    BExprPtr identifier(const ast::Identifier& id, SourceRange range, Use use);
    /// The expression a found symbol stands for.
    BExprPtr from_symbol(Symbol& found, const std::string& name, SourceRange range, Use use);
    /// A call expression: free function, host function, constructor, function object...
    BExprPtr call(const ast::CallExpr& call, SourceRange range);
    /// `object.f(args)` / `pointer->f(args)`.
    BExprPtr method_call(const ast::CallExpr& call, const ast::MemberExpr& callee, SourceRange range);
    /// A unary operator (built-in or overloaded).
    BExprPtr unary(const ast::UnaryExpr& u, SourceRange range);
    /// A binary operator (built-in or overloaded).
    BExprPtr binary(const ast::BinaryExpr& b, SourceRange range);
    /// A built-in arithmetic, comparison or pointer operator, after the usual
    /// arithmetic conversions.
    BExprPtr arithmetic(ast::BinaryOp op, BExprPtr lhs, BExprPtr rhs, SourceRange range);
    /// `=` and compound assignments.
    BExprPtr assign(const ast::AssignExpr& a, SourceRange range);
    /// `++` and `--`.
    BExprPtr inc_dec(const ast::IncDecExpr& e, SourceRange range);
    /// `?:`.
    BExprPtr conditional(const ast::ConditionalExpr& c, SourceRange range);
    /// `a[i]` (built-in or operator[]).
    BExprPtr subscript(const ast::SubscriptExpr& s, SourceRange range);
    /// `object.member` / `pointer->member`.
    BExprPtr member(const ast::MemberExpr& m, SourceRange range);
    /// Member `name` of the record lvalue `object` (`want_methods`: a member
    /// function may be named, for a call).
    BExprPtr member_of(BExprPtr object, const std::string& name, SourceRange name_range, SourceRange range,
                       bool want_methods);
    /// C-style, functional and static_cast conversions.
    BExprPtr cast(const ast::CastExpr& c, SourceRange range);
    /// For `p->x` on a class: applies its operator-> until a pointer results.
    BExprPtr through_arrow(BExprPtr base, SourceRange range);
    /// `sizeof`.
    BExprPtr size_of(const ast::SizeofExpr& s, SourceRange range);
    /// `new`.
    BExprPtr new_expr(const ast::NewExpr& n, SourceRange range);
    /// `delete` / `delete[]`.
    BExprPtr delete_expr(const ast::DeleteExpr& d, SourceRange range);
    /// A lambda: its closure class and the object initialized with the captures.
    BExprPtr lambda(const ast::LambdaExpr& l, SourceRange range);

    /// Overloaded operators: is there a user-declared `name` (e.g. "operator+")
    /// for these operand types? Then calls it.
    [[nodiscard]] bool has_operator(std::string_view name, const BExpr& lhs, const BExpr* rhs);
    /// Non-member operator functions named `name` that could take these operands.
    [[nodiscard]] std::vector<Callee> free_operators(std::string_view name, const BExpr& lhs, const BExpr* rhs);
    /// Calls the overloaded operator `name` with one or two operands.
    BExprPtr call_operator(const std::string& name, BExprPtr lhs, BExprPtr rhs, SourceRange range);

    /// Must be a modifiable lvalue; reports otherwise.
    bool check_modifiable(const BExpr& target, const ast::Expr& source);
    /// Reports that `op` cannot take these operands (no-operator for classes).
    void report_invalid_operands(std::string_view op, const BExpr& lhs, const BExpr* rhs, SourceRange range);
    /// Every visible variable and constant name, for "did you mean" suggestions.
    [[nodiscard]] std::vector<std::string> value_names() const;

    /// The shared analysis state.
    AnalysisContext& ctx_;
    /// Finds members of classes.
    MemberLookup members_;
};

}  // namespace cppi::sema
