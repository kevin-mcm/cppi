#pragma once

/// @file Interpreter.hpp
/// @brief Facade over the whole pipeline: parse -> analyze -> generate bytecode
/// -> execute.
///
/// Together with HostRegistry, this is all most hosts need.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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

/// Compiles and runs player programs against one host. Compiling is const and
/// does not touch shared state, so one interpreter can be used from several
/// threads.
class Interpreter {
public:
    /// @param host    What the host exposes to player code (copied, then immutable).
    /// @param options Standard and locked features for compilation.
    explicit Interpreter(HostRegistry host, Options options = {});

    /// Compiles player code. Never throws for bad input: problems are
    /// reported as diagnostics.
    /// @param source The player's code.
    [[nodiscard]] CompileResult compile(std::string_view source) const;

    /// Runs a program to completion (or until it stops).
    /// @param program A program compiled by an interpreter with the same host.
    /// @param options Budget, cost model and observer for this run.
    [[nodiscard]] RunResult run(const Program& program, const RunOptions& options = {}) const;

    /// Prepares a step-by-step execution.
    /// @param program A program compiled by an interpreter with the same host.
    /// @param options Budget, cost model and observer for this run.
    [[nodiscard]] Execution start(const Program& program, const RunOptions& options = {}) const;

    /// Current compile options.
    [[nodiscard]] const Options& options() const noexcept { return options_; }
    /// Replaces the compile options, e.g. when the player reaches a new level.
    void set_options(Options options) noexcept { options_ = options; }
    /// The host registry programs are compiled against.
    [[nodiscard]] const HostRegistry& host() const noexcept { return *host_; }

private:
    std::shared_ptr<const HostRegistry> host_;
    Options options_;
};

}  // namespace cppi
