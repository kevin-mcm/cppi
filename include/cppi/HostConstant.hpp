#pragma once

/// @file HostConstant.hpp
/// @brief A named constant visible to player code (today: enumerators).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/TypeId.hpp>

#include <cstdint>

namespace cppi {

/// A constant registered by the host and visible to player code.
struct HostConstant {
    /// Type of the constant (the enum it belongs to).
    TypeId type;
    /// Its value.
    std::int64_t value = 0;
};

}  // namespace cppi
