#pragma once

/// @file Interpreter.hpp
/// Facade over the whole pipeline: parse -> analyze -> generate bytecode ->
/// execute. Together with HostRegistry, this is all most hosts need.

#include <cppi/CompileResult.hpp>
#include <cppi/Execution.hpp>
#include <cppi/ExecutionObserver.hpp>
#include <cppi/HostRegistry.hpp>
#include <cppi/Options.hpp>
#include <cppi/Program.hpp>
#include <cppi/RunOptions.hpp>
#include <cppi/RunResult.hpp>

#include <memory>
#include <string_view>

namespace cppi {

class Interpreter {
public:
    explicit Interpreter(HostRegistry host, Options options = {});

    /// Compiles player code. Never throws for bad input: problems are
    /// reported as diagnostics.
    [[nodiscard]] CompileResult compile(std::string_view source) const;

    /// Runs a program to completion (or until it stops).
    [[nodiscard]] RunResult run(const Program& program, const RunOptions& options = {}) const;

    /// Prepares a step-by-step execution.
    [[nodiscard]] Execution start(const Program& program, const RunOptions& options = {}) const;

    [[nodiscard]] const Options& options() const noexcept { return options_; }
    void set_options(Options options) noexcept { options_ = options; }
    [[nodiscard]] const HostRegistry& host() const noexcept { return *host_; }

private:
    std::shared_ptr<const HostRegistry> host_;
    Options options_;
};

}  // namespace cppi
