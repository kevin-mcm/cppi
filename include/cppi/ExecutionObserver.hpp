#pragma once

/// @file ExecutionObserver.hpp
/// @brief Observer pattern: traces, debuggers and animations without touching
/// the VM.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/HostCall.hpp>
#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <string_view>

namespace cppi {

/// Receives execution events. All methods have empty defaults, so observers
/// only override what they need (tracing, debuggers, animations...).
class ExecutionObserver {
public:
    ExecutionObserver() = default;
    ExecutionObserver(const ExecutionObserver&) = default;
    ExecutionObserver& operator=(const ExecutionObserver&) = default;
    ExecutionObserver(ExecutionObserver&&) = default;
    ExecutionObserver& operator=(ExecutionObserver&&) = default;
    /// Observers are used polymorphically.
    virtual ~ExecutionObserver();

    /// Called right before a host function runs.
    virtual void on_host_call(const HostCall& call);
    /// Called after every executed instruction.
    virtual void on_step(SourceRange location, std::uint64_t operations_used);
    /// Called when the program prints text (std::cout).
    virtual void on_output(std::string_view text);
};

}  // namespace cppi
