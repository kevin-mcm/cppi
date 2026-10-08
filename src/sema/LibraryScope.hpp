#pragma once

/// @file LibraryScope.hpp
/// @brief RAII: while alive, the analyzer is binding standard library code
/// (features are not gated, diagnostics point at the player's call site).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisContext.hpp"

#include <cppi/SourceRange.hpp>

#include <string>
#include <utility>

namespace cppi::sema {

/// RAII guard marking that the standard library is being bound.
class LibraryScope {
public:
    /// `entry`: the library template instance being bound, named for notes ("std::sort<P>").
    LibraryScope(AnalysisContext& ctx, SourceRange site, std::string entry = {})
        : ctx_(ctx), previous_site_(ctx.library_site), previous_entry_(ctx.library_entry) {
        if (ctx_.library_depth == 0) {
            ctx_.library_site = site;
            ctx_.library_entry = std::move(entry);
        }
        ++ctx_.library_depth;
        ctx_.features.suspend(true);
    }
    LibraryScope(const LibraryScope&) = delete;
    LibraryScope& operator=(const LibraryScope&) = delete;
    LibraryScope(LibraryScope&&) = delete;
    LibraryScope& operator=(LibraryScope&&) = delete;
    /// Restores the previous site and feature gating.
    ~LibraryScope() {
        --ctx_.library_depth;
        ctx_.library_site = previous_site_;
        ctx_.library_entry = std::move(previous_entry_);
        ctx_.features.suspend(ctx_.library_depth > 0);
    }

private:
    /// The shared analysis state.
    AnalysisContext& ctx_;
    /// Site to restore.
    SourceRange previous_site_;
    /// Entry to restore.
    std::string previous_entry_;
};

}  // namespace cppi::sema
