#pragma once

/// Collects the diagnostics a compiler stage reports.

#include <cppi/Diagnostic.hpp>

#include <cppi/Severity.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace cppi::detail {

class DiagnosticSink {
public:
    explicit DiagnosticSink(std::vector<Diagnostic>& out) noexcept : out_(&out) {}

    void report(Diagnostic diagnostic) {
        if (muted_ > 0) {
            if (diagnostic.severity == Severity::Error) {
                ++dropped_errors_;  // a trial (concept check): only whether it failed matters
            }
            return;
        }
        out_->push_back(std::move(diagnostic));
    }

    /// While muted, diagnostics are counted, not kept.
    void mute() noexcept { ++muted_; }
    void unmute() noexcept { --muted_; }
    [[nodiscard]] std::size_t dropped_errors() const noexcept { return dropped_errors_; }
    [[nodiscard]] bool muted() const noexcept { return muted_ > 0; }

private:
    std::vector<Diagnostic>* out_;
    int muted_ = 0;
    std::size_t dropped_errors_ = 0;
};

}  // namespace cppi::detail
