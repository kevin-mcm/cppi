#pragma once

/// @file CompilerPipeline.hpp
/// @brief Pipeline pattern: source text -> parse -> analyze -> generate
/// bytecode.
///
/// Each stage has a single responsibility and its own data as the boundary; the
/// pipeline stops at the first stage that reports errors.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "pipeline/CompilerOutput.hpp"

#include <cppi/HostRegistry.hpp>
#include <cppi/Options.hpp>

#include <memory>
#include <string_view>
#include <utility>

namespace cppi::detail {

/// Runs the compilation stages in order, stopping at the first one that
/// reports errors.
class CompilerPipeline {
public:
    /// @param host    The host registry player code is compiled against.
    /// @param options Standard and locked features (must outlive the pipeline).
    CompilerPipeline(std::shared_ptr<const HostRegistry> host, const Options& options) noexcept
        : host_(std::move(host)), options_(options) {}

    /// Compiles `source`. Never throws for bad input.
    [[nodiscard]] CompilerOutput compile(std::string_view source) const;

private:
    /// The host registry.
    std::shared_ptr<const HostRegistry> host_;
    /// Compile options.
    const Options& options_;
};

}  // namespace cppi::detail
