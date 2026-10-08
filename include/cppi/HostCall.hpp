#pragma once

/// @file HostCall.hpp
/// Context handed to a host function while it runs.

#include <cppi/SourceRange.hpp>
#include <cppi/Value.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace cppi {

class HostCall {
public:
    HostCall(std::string_view function, std::span<const Value> args, SourceRange location) noexcept
        : function_(function), args_(args), location_(location) {}

    [[nodiscard]] std::span<const Value> args() const noexcept { return args_; }
    [[nodiscard]] const Value& arg(std::size_t index) const noexcept { return args_[index]; }
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

    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] std::uint32_t error() const noexcept { return error_; }
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
