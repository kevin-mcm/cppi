#pragma once

/// @file DiagnosticSink.hpp
/// @brief Collects the diagnostics a compiler stage reports.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Diagnostic.hpp>

#include <cppi/Severity.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace cppi::detail {

/// Appends diagnostics to a list; can be muted while the analyzer tries
/// something that may fail (e.g. checking whether a concept is satisfied).
class DiagnosticSink {
public:
    /// @param out Where diagnostics are appended (must outlive the sink).
    explicit DiagnosticSink(std::vector<Diagnostic>& out) noexcept : out_(&out) {}

    /// Keeps `diagnostic`, or only counts it (errors) while muted.
    void report(Diagnostic diagnostic) {
        if (diagnostic.severity == Severity::Error) {
            ++errors_;
        }
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
    /// Ends one mute(); mutes nest.
    void unmute() noexcept { --muted_; }
    /// Errors reported while muted, in total.
    [[nodiscard]] std::size_t dropped_errors() const noexcept { return dropped_errors_; }
    /// True while at least one mute() is active.
    [[nodiscard]] bool muted() const noexcept { return muted_ > 0; }
    /// Errors reported so far, kept or muted.
    [[nodiscard]] std::size_t errors() const noexcept { return errors_; }
    /// True if an equal diagnostic (code, severity, place and arguments) was kept already.
    [[nodiscard]] bool has(const Diagnostic& d) const {
        return std::any_of(out_->begin(), out_->end(), [&](const Diagnostic& o) {
            return o.code == d.code && o.severity == d.severity && o.range == d.range &&
                   std::equal(
                       o.args.begin(), o.args.end(), d.args.begin(), d.args.end(),
                       [](const DiagArg& a, const DiagArg& b) { return a.name == b.name && a.value == b.value; });
        });
    }

private:
    /// Output list.
    std::vector<Diagnostic>* out_;
    /// Nesting depth of mute().
    int muted_ = 0;
    /// See dropped_errors().
    std::size_t dropped_errors_ = 0;
    /// See errors().
    std::size_t errors_ = 0;
};

}  // namespace cppi::detail
