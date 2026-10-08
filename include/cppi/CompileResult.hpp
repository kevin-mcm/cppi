#pragma once

/// @file CompileResult.hpp
/// Outcome of Interpreter::compile().

#include <cppi/Diagnostic.hpp>
#include <cppi/Program.hpp>

#include <optional>
#include <vector>

namespace cppi {

struct CompileResult {
    std::optional<Program> program;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept { return program.has_value(); }
};

}  // namespace cppi
