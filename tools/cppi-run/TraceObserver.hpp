#pragma once

/// @file TraceObserver.hpp
/// @brief Prints each host call as it happens: "  3:1  move(East)".
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/ExecutionObserver.hpp>
#include <cppi/HostCall.hpp>
#include <cppi/HostRegistry.hpp>

#include <ostream>

namespace cppi_run {

/// Prints host calls as they happen (`--trace`).
class TraceObserver final : public cppi::ExecutionObserver {
public:
    /// @param out  Where to print.
    /// @param host Registry used to render enum arguments.
    TraceObserver(std::ostream& out, const cppi::HostRegistry& host) : out_(&out), host_(&host) {}

    /// Prints the call with its location and arguments.
    void on_host_call(const cppi::HostCall& call) override;

private:
    /// Output stream.
    std::ostream* out_;
    /// The host registry.
    const cppi::HostRegistry* host_;
};

}  // namespace cppi_run
