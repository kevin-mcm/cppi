#pragma once

/// @file HostCall.hpp
/// @brief Context handed to a host function while it runs.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/SourceRange.hpp>
#include <cppi/Value.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace cppi {

/// What a host function receives: the name it was called by, its arguments
/// and where the call is, plus a way to report failure.
class HostCall {
public:
    /// @param function Name of the called function.
    /// @param args     Arguments, already converted to the declared parameter types.
    /// @param location Where the call is in the player's code.
    HostCall(std::string_view function, std::span<const Value> args, SourceRange location) noexcept
        : function_(function), args_(args), location_(location) {}

    /// All arguments, in declaration order.
    [[nodiscard]] std::span<const Value> args() const noexcept { return args_; }
    /// The argument at `index` (0-based; must be in range).
    [[nodiscard]] const Value& arg(std::size_t index) const noexcept { return args_[index]; }
    /// Name of the called function.
    [[nodiscard]] std::string_view function_name() const noexcept { return function_; }
    /// Where in the player's code this call happens (useful for animations
    /// or highlighting the current line).
    [[nodiscard]] SourceRange location() const noexcept { return location_; }

    /// Aborts the program with a HostError diagnostic. `error` is a code
    /// defined by the host (e.g. "tile is blocked"), so it can be translated.
    void fail(std::uint32_t error, std::string detail = {}) {
        failed_ = true;
        error_ = error;
        detail_ = std::move(detail);
    }

    /// True once fail() has been called.
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    /// The host-defined error code passed to fail().
    [[nodiscard]] std::uint32_t error() const noexcept { return error_; }
    /// The optional detail text passed to fail().
    [[nodiscard]] const std::string& detail() const noexcept { return detail_; }

private:
    std::string_view function_;
    std::span<const Value> args_;
    SourceRange location_;
    bool failed_ = false;
    std::uint32_t error_ = 0;
    std::string detail_;
};

}  // namespace cppi
