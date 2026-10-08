#include "sema/AnalysisResult.hpp"

#include <algorithm>

namespace cppi::sema {

bool AnalysisResult::has_errors() const noexcept {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [](const Diagnostic& d) { return d.severity == Severity::Error; });
}

}  // namespace cppi::sema
