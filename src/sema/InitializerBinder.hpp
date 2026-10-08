#pragma once

/// @file InitializerBinder.hpp
/// @brief Lowers C++ initialization to bound statements: copy- and direct-
/// initialization of scalars, brace lists for arrays and aggregates,
/// constructor calls for classes, and the matching destruction.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisContext.hpp"
#include "sema/BoundTree.hpp"

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <vector>

namespace cppi::sema {

/// Binds initialization and destruction of objects. Functions returning
/// bool return false after reporting an error; statements go to `out`.
class InitializerBinder {
public:
    /// @param ctx The shared analysis state.
    explicit InitializerBinder(AnalysisContext& ctx) noexcept : ctx_(ctx) {}

    /// Initializes the object `target` of `type` as the declarator says.
    /// `fresh_local`: new automatic storage, whose contents start out
    /// indeterminate (globals are zero-initialized beforehand).
    bool initialize(BExprPtr target, TypeRef type, const ast::VarDeclarator& var, bool fresh_local,
                    std::vector<BStmt>& out);

    /// `T x = expr;`
    bool initialize_expr(BExprPtr target, TypeRef type, const ast::Expr& init, std::vector<BStmt>& out);
    /// `T x = {a, b};` / `T x{a, b};`
    bool initialize_list(BExprPtr target, TypeRef type, const ast::InitList& list, SourceRange range,
                         std::vector<BStmt>& out);
    /// From an already bound expression (natural category).
    bool initialize_from(BExprPtr target, TypeRef type, BExprPtr value, std::vector<BStmt>& out);
    /// Runs a constructor (or aggregate/default initialization) with `args`.
    bool construct(BExprPtr target, TypeRef type, std::vector<BExprPtr> args, SourceRange range,
                   std::vector<BStmt>& out);
    /// No initializer at all. `complete`: `target` is a complete object (it
    /// gets its headers and its virtual bases), not a base subobject.
    bool default_initialize(BExprPtr target, TypeRef type, bool fresh_local, SourceRange range, std::vector<BStmt>& out,
                            bool complete = true);

    /// Default-initializing an object of `type` runs nothing: no constructor,
    /// no default member initializer, no virtual base to set up.
    [[nodiscard]] bool trivial_default_initialization(TypeRef type) const;

    /// Binds a reference variable: stores the address of the referent in `slot`.
    bool bind_reference(BExprPtr slot, TypeRef reference_type, const ast::Expr& init, std::vector<BStmt>& out);

    /// Destroys the object `target`, running destructors (members and bases
    /// are destroyed by the destructor itself).
    void destroy(BExprPtr target, TypeRef type, std::vector<BStmt>& out);
    /// True if destroying an object of `type` runs code (a destructor somewhere).
    [[nodiscard]] bool needs_destruction(TypeRef type) const;

    /// Runs the constructors of `record`'s bases and members for the
    /// constructor `fn` (its initializer list), then nothing else: the body
    /// follows. `self` is `*this`.
    bool construct_members(std::uint32_t record, const ast::FunctionDecl* decl, std::vector<BStmt>& out);
    /// Body of an implicit copy constructor: copies every base and member
    /// from `*other` (members run their own copy constructors).
    bool copy_members(std::uint32_t record, BExprPtr other, std::vector<BStmt>& out);
    /// Body of an implicit operator=: assigns every base and member (headers
    /// stay: the object keeps its dynamic type).
    bool assign_members(std::uint32_t record, BExprPtr other, std::vector<BStmt>& out);

    /// Destroys `record`'s members and bases, in reverse order (end of a destructor).
    void destroy_members(std::uint32_t record, std::vector<BStmt>& out);

private:
    /// Initializes the virtual bases of a complete object of `record`.
    bool init_virtual_bases(const BExpr& target, std::uint32_t record, SourceRange range, std::vector<BStmt>& out);
    /// Runs a constructor of a base class on a base subobject (no headers).
    bool construct_base(BExprPtr target, std::uint32_t base_record, std::vector<BExprPtr> args, SourceRange range,
                        std::vector<BStmt>& out);

    /// A bound statement holding `node`.
    static BStmt stmt(SourceRange range, auto node) {
        BStmt s;
        s.range = range;
        s.node = std::move(node);
        return s;
    }
    /// Element `index` of an array lvalue.
    [[nodiscard]] BExprPtr element(const BExpr& array, std::uint32_t index) const;
    /// A field of a record lvalue.
    [[nodiscard]] BExprPtr field_of(const BExpr& record, const FieldInfo& field) const;
    /// A base subobject of a record lvalue.
    [[nodiscard]] BExprPtr base_of(const BExpr& record, const BaseInfo& base) const;

    /// The shared analysis state.
    AnalysisContext& ctx_;
};

}  // namespace cppi::sema
