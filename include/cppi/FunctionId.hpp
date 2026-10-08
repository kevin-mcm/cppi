#pragma once

/// @file FunctionId.hpp
/// @brief Handle to a function registered in a HostRegistry.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>

namespace cppi {

/// Opaque, comparable handle to a function registered in a HostRegistry.
struct FunctionId {
    /// Index of the function inside its registry.
    std::uint32_t value = 0;
    friend constexpr bool operator==(const FunctionId&, const FunctionId&) = default;
};

}  // namespace cppi
