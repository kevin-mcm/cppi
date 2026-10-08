#pragma once

// Prints each host call as it happens: "  3:1  move(East)".

#include <cppi/ExecutionObserver.hpp>
#include <cppi/HostCall.hpp>
#include <cppi/HostRegistry.hpp>

#include <ostream>

namespace cppi_run {

class TraceObserver final : public cppi::ExecutionObserver {
public:
    TraceObserver(std::ostream& out, const cppi::HostRegistry& host) : out_(&out), host_(&host) {}

    void on_host_call(const cppi::HostCall& call) override;

private:
    std::ostream* out_;
    const cppi::HostRegistry* host_;
};

}  // namespace cppi_run
