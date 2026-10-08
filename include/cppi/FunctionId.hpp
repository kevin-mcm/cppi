#pragma once

/// @file FunctionId.hpp
/// Handle to a function registered in a HostRegistry.

#include <cstdint>

namespace cppi {

struct FunctionId {
    std::uint32_t value = 0;
    friend constexpr bool operator==(const FunctionId&, const FunctionId&) = default;
};

}  // namespace cppi
