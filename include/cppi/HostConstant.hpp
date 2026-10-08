#pragma once

/// @file HostConstant.hpp
/// A named constant visible to player code (today: enumerators).

#include <cppi/TypeId.hpp>

#include <cstdint>

namespace cppi {

struct HostConstant {
    TypeId type;
    std::int64_t value = 0;
};

}  // namespace cppi
