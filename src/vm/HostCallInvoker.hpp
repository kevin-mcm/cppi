#pragma once

/// @file HostCallInvoker.hpp
/// @brief Calls into host code safely: exceptions and return values that break
/// the declared signature are turned into HostCall failures, so a buggy host
/// function can never take the game down.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/HostCall.hpp>
#include <cppi/HostFunctionInfo.hpp>
#include <cppi/HostRegistry.hpp>
#include <cppi/Value.hpp>

#include <cstdint>
#include <limits>

namespace cppi::detail {

/// Calls host functions, shielding the VM from host bugs.
class HostCallInvoker {
public:
    /// Error code used when the host itself misbehaves (it threw, or returned
    /// a value of the wrong type).
    static constexpr std::uint32_t kHostFault = std::numeric_limits<std::uint32_t>::max();

    /// @param host The registry the functions belong to (used to check results).
    explicit HostCallInvoker(const HostRegistry& host) noexcept : host_(host) {}

    /// Runs `fn`. On failure `call.failed()` is true and the result is void.
    [[nodiscard]] Value invoke(const HostFunctionInfo& fn, HostCall& call) const;

private:
    /// The host registry.
    const HostRegistry& host_;
};

}  // namespace cppi::detail
