#pragma once

/// @file AnalysisResult.hpp
/// @brief Outcome of semantic analysis: the bound program ready for code
/// generation, plus every error and warning found.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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
    /// Variable name.
    std::string name;
    /// First cell in the global area.
    std::uint32_t offset = 0;
    /// Its type.
    TypeRef type = 0;
};

/// A type some `throw` uses: what the VM needs to destroy and describe it.
struct ThrownType {
    /// The thrown type.
    TypeRef type = 0;
    /// Its name, for diagnostics.
    std::string name;
    /// Its destructor, if it has one.
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

/// The whole analyzed program, ready for code generation.
struct BoundProgram {
    std::vector<BoundFunction> functions;  ///< [0] is the script (top-level statements)
    /// Size of the global area.
    std::uint32_t global_cells = 0;
    /// Global variables, for debuggers.
    std::vector<GlobalDebug> globals;
    /// Every type the program uses.
    std::shared_ptr<TypeTable> types;
    /// Types thrown by some `throw`.
    std::vector<ThrownType> thrown;
    /// String literals: global offset -> text, written before the program runs.
    std::vector<std::pair<std::uint32_t, std::string>> strings;
    /// One catch table per try statement.
    std::vector<std::vector<CatchClauseInfo>> catch_tables;
};

/// What the Analyzer returns.
struct AnalysisResult {
    /// The bound program; usable only without errors.
    BoundProgram program;
    std::vector<Diagnostic> diagnostics;  ///< errors and warnings

    /// True if any diagnostic is an error.
    [[nodiscard]] bool has_errors() const noexcept;
};

}  // namespace cppi::sema
