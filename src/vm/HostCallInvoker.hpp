#pragma once

/// Calls into host code safely: exceptions and return values that break the
/// declared signature are turned into HostCall failures, so a buggy host
/// function can never take the game down.

#include <cppi/HostCall.hpp>
#include <cppi/HostFunctionInfo.hpp>
#include <cppi/HostRegistry.hpp>
#include <cppi/Value.hpp>

#include <cstdint>
#include <limits>

namespace cppi::detail {

class HostCallInvoker {
public:
    /// Error code used when the host itself misbehaves (it threw, or returned
    /// a value of the wrong type).
    static constexpr std::uint32_t kHostFault = std::numeric_limits<std::uint32_t>::max();

    explicit HostCallInvoker(const HostRegistry& host) noexcept : host_(host) {}

    /// Runs `fn`. On failure `call.failed()` is true and the result is void.
    [[nodiscard]] Value invoke(const HostFunctionInfo& fn, HostCall& call) const;

private:
    const HostRegistry& host_;
};

}  // namespace cppi::detail
