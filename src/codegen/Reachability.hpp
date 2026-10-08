#pragma once

/// Which functions a program can actually run: those called (directly, or
/// through a virtual table of a class it creates) from the script. Code is
/// generated only for them, so the standard library prelude costs nothing
/// when unused.

#include "sema/AnalysisResult.hpp"
#include "sema/BoundTree.hpp"

#include <cstdint>
#include <vector>

namespace cppi::detail {

class Reachability {
public:
    /// reachable[id] for every function of `program`.
    [[nodiscard]] static std::vector<bool> compute(const sema::BoundProgram& program);

private:
    explicit Reachability(const sema::BoundProgram& program) : program_(program) {}

    void function(std::uint32_t id);
    void record(std::uint32_t id);
    void statements(const std::vector<sema::BStmt>& list);
    void statement(const sema::BStmt& s);
    void expression(const sema::BExpr& e);

    const sema::BoundProgram& program_;
    std::vector<bool> functions_;
    std::vector<bool> records_;
    std::vector<std::uint32_t> pending_;
};

}  // namespace cppi::detail
