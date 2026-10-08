#pragma once

/// Outcome of semantic analysis: the bound program ready for code
/// generation, plus every error and warning found.

#include "sema/BoundTree.hpp"
#include "sema/TypeTable.hpp"

#include <cppi/Diagnostic.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

/// A global variable, for debuggers.
struct GlobalDebug {
    std::string name;
    std::uint32_t offset = 0;
    TypeRef type = 0;
};

/// A type some `throw` uses: what the VM needs to destroy and describe it.
struct ThrownType {
    TypeRef type = 0;
    std::string name;
    std::optional<std::uint32_t> destructor;
    /// std::exception and derived classes: cells from the object to the
    /// `char*` of its message (for "uncaught exception: what()").
    std::optional<std::uint32_t> message_offset;
};

/// The catch clauses of one try statement, in order.
struct CatchClauseInfo {
    TypeRef type = 0;        ///< as written (references removed); unused for `catch (...)`
    bool catch_all = false;  ///< `catch (...)`
    /// Thrown types it catches, and the cells to add to reach the caught
    /// base subobject. Filled in when the whole program is known.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> matches;
};

struct BoundProgram {
    std::vector<BoundFunction> functions;  ///< [0] is the script (top-level statements)
    std::uint32_t global_cells = 0;
    std::vector<GlobalDebug> globals;
    std::shared_ptr<TypeTable> types;
    std::vector<ThrownType> thrown;
    /// String literals: global offset -> text, written before the program runs.
    std::vector<std::pair<std::uint32_t, std::string>> strings;
    std::vector<std::vector<CatchClauseInfo>> catch_tables;
};

struct AnalysisResult {
    BoundProgram program;
    std::vector<Diagnostic> diagnostics;  ///< errors and warnings

    [[nodiscard]] bool has_errors() const noexcept;
};

}  // namespace cppi::sema
