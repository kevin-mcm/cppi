#pragma once

/// @file CompileResult.hpp
/// @brief Outcome of Interpreter::compile().
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Diagnostic.hpp>
#include <cppi/Program.hpp>

#include <optional>
#include <vector>

namespace cppi {

/// Result of compiling a program: the program itself, or the diagnostics that
/// prevented it. Warnings may be present even when compilation succeeded.
struct CompileResult {
    /// The compiled program; empty when compilation failed.
    std::optional<Program> program;
    /// Every error, warning and note reported while compiling.
    std::vector<Diagnostic> diagnostics;

    /// @return true if compilation produced a program.
    [[nodiscard]] bool ok() const noexcept { return program.has_value(); }
};

}  // namespace cppi
