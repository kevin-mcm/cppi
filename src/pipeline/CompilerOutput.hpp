#pragma once

/// What the CompilerPipeline produces.

#include "codegen/ProgramData.hpp"

#include <cppi/Diagnostic.hpp>

#include <memory>
#include <vector>

namespace cppi::detail {

struct CompilerOutput {
    std::shared_ptr<const ProgramData> program;  ///< null if compilation failed
    std::vector<Diagnostic> diagnostics;         ///< errors, and warnings on success too
};

}  // namespace cppi::detail
