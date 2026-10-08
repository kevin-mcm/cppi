#pragma once

/// @file CompilerOutput.hpp
/// @brief What the CompilerPipeline produces.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/ProgramData.hpp"

#include <cppi/Diagnostic.hpp>

#include <memory>
#include <vector>

namespace cppi::detail {

/// The compiled program data, or the diagnostics that prevented it.
struct CompilerOutput {
    std::shared_ptr<const ProgramData> program;  ///< null if compilation failed
    std::vector<Diagnostic> diagnostics;         ///< errors, and warnings on success too
};

}  // namespace cppi::detail
