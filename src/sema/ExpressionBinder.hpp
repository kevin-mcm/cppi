#pragma once

/// Binds AST expressions: resolves names, checks types, makes every
/// implicit conversion explicit and decides what is an object (lvalue) and
/// what is a value (rvalue). A Visitor over ast::Expr.

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

class ExpressionBinder {
public:
    /// Where the expression appears: a function named as a whole statement
    /// (`harvest;`) is only a warning, but as a value it is an error.
    enum class Use : std::uint8_t { Statement, Value };

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

    [[nodiscard]] BExprPtr to_rvalue(BExprPtr expr);
    [[nodiscard]] BExprPtr convert_to(BExprPtr rvalue, TypeRef to);
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
    [[nodiscard]] static BExprPtr make(TypeRef type, bool lvalue, SourceRange range, auto node) {
        auto e = std::make_unique<BExpr>();
        e->type = type;
        e->lvalue = lvalue;
        e->range = range;
        e->node = std::move(node);
        return e;
    }
    [[nodiscard]] static BExprPtr constant(TypeRef type, std::int64_t bits, SourceRange range) {
        return make(type, false, range, BConst{bits});
    }
    [[nodiscard]] BExprPtr variable(const Symbol& symbol, const std::string& name, SourceRange range) const;
    /// A static data member of `record` (or of a base), or nullptr.
    [[nodiscard]] Symbol* static_member(std::uint32_t record, std::string_view name) const;

private:
    BExprPtr bind_node(const ast::Expr& expr, Use use);

    BExprPtr int_literal(const ast::IntLiteral& lit, SourceRange range);
    BExprPtr identifier(const ast::Identifier& id, SourceRange range, Use use);
    BExprPtr from_symbol(Symbol& found, const std::string& name, SourceRange range, Use use);
    BExprPtr call(const ast::CallExpr& call, SourceRange range);
    BExprPtr method_call(const ast::CallExpr& call, const ast::MemberExpr& callee, SourceRange range);
    BExprPtr unary(const ast::UnaryExpr& u, SourceRange range);
    BExprPtr binary(const ast::BinaryExpr& b, SourceRange range);
    BExprPtr arithmetic(ast::BinaryOp op, BExprPtr lhs, BExprPtr rhs, SourceRange range);
    BExprPtr assign(const ast::AssignExpr& a, SourceRange range);
    BExprPtr inc_dec(const ast::IncDecExpr& e, SourceRange range);
    BExprPtr conditional(const ast::ConditionalExpr& c, SourceRange range);
    BExprPtr subscript(const ast::SubscriptExpr& s, SourceRange range);
    BExprPtr member(const ast::MemberExpr& m, SourceRange range);
    BExprPtr member_of(BExprPtr object, const std::string& name, SourceRange name_range, SourceRange range,
                       bool want_methods);
    BExprPtr cast(const ast::CastExpr& c, SourceRange range);
    BExprPtr through_arrow(BExprPtr base, SourceRange range);
    BExprPtr size_of(const ast::SizeofExpr& s, SourceRange range);
    BExprPtr new_expr(const ast::NewExpr& n, SourceRange range);
    BExprPtr delete_expr(const ast::DeleteExpr& d, SourceRange range);
    BExprPtr lambda(const ast::LambdaExpr& l, SourceRange range);

    /// Overloaded operators: is there a user-declared `name` (e.g. "operator+")
    /// for these operand types? Then calls it.
    [[nodiscard]] bool has_operator(std::string_view name, const BExpr& lhs, const BExpr* rhs);
    [[nodiscard]] std::vector<Callee> free_operators(std::string_view name, const BExpr& lhs, const BExpr* rhs);
    BExprPtr call_operator(const std::string& name, BExprPtr lhs, BExprPtr rhs, SourceRange range);

    /// Must be a modifiable lvalue; reports otherwise.
    bool check_modifiable(const BExpr& target, const ast::Expr& source);
    void report_invalid_operands(std::string_view op, const BExpr& lhs, const BExpr* rhs, SourceRange range);
    [[nodiscard]] std::vector<std::string> value_names() const;

    AnalysisContext& ctx_;
    MemberLookup members_;
};

}  // namespace cppi::sema
