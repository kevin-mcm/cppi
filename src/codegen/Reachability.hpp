#pragma once

/// @file Reachability.hpp
/// @brief Which functions a program can actually run: those called (directly,
/// or through a virtual table of a class it creates) from the script.
///
/// Code is generated only for them, so the standard library prelude costs
/// nothing when unused.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisResult.hpp"
#include "sema/BoundTree.hpp"

#include <cstdint>
#include <vector>

namespace cppi::detail {

/// Marks the functions reachable from the script, so code generation can skip
/// the rest (most of the standard library prelude, in a typical program).
class Reachability {
public:
    /// reachable[id] for every function of `program`.
    [[nodiscard]] static std::vector<bool> compute(const sema::BoundProgram& program);

private:
    /// Use compute().
    explicit Reachability(const sema::BoundProgram& program) : program_(program) {}

    /// Marks function `id` reachable and queues its body.
    void function(std::uint32_t id);
    /// Marks record `id` as created: its virtual functions become reachable.
    void record(std::uint32_t id);
    /// Visits each statement of `list`.
    void statements(const std::vector<sema::BStmt>& list);
    /// Visits a statement and everything it contains.
    void statement(const sema::BStmt& s);
    /// Visits an expression: calls, object creation and destruction.
    void expression(const sema::BExpr& e);

    /// The program analyzed.
    const sema::BoundProgram& program_;
    /// Reachable functions, by id.
    std::vector<bool> functions_;
    /// Records whose objects the program creates, by id.
    std::vector<bool> records_;
    /// Reachable functions whose bodies have not been visited yet.
    std::vector<std::uint32_t> pending_;
};

}  // namespace cppi::detail
