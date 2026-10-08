#pragma once

/// Binds statements: control flow, block scopes, local declarations, and the
/// destruction of local objects when their scope ends.

#include "sema/AnalysisContext.hpp"
#include "sema/BoundTree.hpp"

#include "ast/Stmt.hpp"

#include <vector>

namespace cppi::sema {

class StatementBinder {
public:
    explicit StatementBinder(AnalysisContext& ctx) noexcept : ctx_(ctx) {}

    /// Binds `stmt` (after checking its language features) and appends the
    /// result to `out`. May redirect later statements of the same block into a
    /// nested block, so `out` is passed by pointer and can change.
    void bind(const ast::Stmt& stmt, std::vector<BStmt>*& out);

    /// A `{ ... }` with its own scope.
    [[nodiscard]] BBlock bind_block(const ast::Block& block, SourceRange range);
    /// The statements of a block in the current scope (function bodies).
    void bind_statements(const std::vector<ast::Stmt>& statements, std::vector<BStmt>& out);

    /// Destroys the temporaries created since `mark` (newest first) and
    /// forgets them.
    [[nodiscard]] std::vector<BStmt> release_temporaries(std::size_t mark) const;
    /// Forgets the temporaries created since `mark` (their code never runs).
    void drop_temporaries(std::size_t mark) const;

    /// Whether control can never reach the end of `stmt`.
    [[nodiscard]] static bool always_returns(const BStmt& stmt);

private:
    static BStmt wrap(SourceRange range, auto node) {
        BStmt s;
        s.range = range;
        s.node = std::move(node);
        return s;
    }
    /// A sub-statement (if/loop body) in its own scope.
    BStmtPtr bind_substatement(const ast::Stmt* stmt, SourceRange range);
    void bind_expression(const ast::ExprStmt& s, SourceRange range, std::vector<BStmt>& out);
    /// Condition with an optional declaration; declarations go into `pre`.
    std::optional<BExpr> bind_condition(const ast::Condition& c, std::vector<BStmt>& pre);
    void bind_if(const ast::IfStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_while(const ast::WhileStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_do(const ast::DoWhileStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_for(const ast::ForStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_range_for(const ast::RangeForStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_range_for_class(const ast::RangeForStmt& s, BExprPtr container, SourceRange range,
                              std::vector<BStmt>& out);
    void bind_return(const ast::ReturnStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_switch(const ast::SwitchStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_static_assert(const ast::StaticAssert& s, SourceRange range);
    void bind_throw(const ast::ThrowStmt& s, SourceRange range, std::vector<BStmt>& out);
    void bind_try(const ast::TryStmt& s, SourceRange range, std::vector<BStmt>& out);
    /// The block that handles one catch clause of table `table`.
    std::optional<BStmt> bind_catch(const ast::CatchClause& clause, std::uint32_t table);

    AnalysisContext& ctx_;
};

}  // namespace cppi::sema
