#pragma once

/// Pipeline pattern: source text -> parse -> analyze -> generate bytecode.
/// Each stage has a single responsibility and its own data as the boundary;
/// the pipeline stops at the first stage that reports errors.

#include "pipeline/CompilerOutput.hpp"

#include <cppi/HostRegistry.hpp>
#include <cppi/Options.hpp>

#include <memory>
#include <string_view>
#include <utility>

namespace cppi::detail {

class CompilerPipeline {
public:
    CompilerPipeline(std::shared_ptr<const HostRegistry> host, const Options& options) noexcept
        : host_(std::move(host)), options_(options) {}

    [[nodiscard]] CompilerOutput compile(std::string_view source) const;

private:
    std::shared_ptr<const HostRegistry> host_;
    const Options& options_;
};

}  // namespace cppi::detail
