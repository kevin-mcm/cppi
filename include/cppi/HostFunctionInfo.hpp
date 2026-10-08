#pragma once

/// @file HostFunctionInfo.hpp
/// What a HostRegistry knows about a registered function.

#include <cppi/HostFunction.hpp>
#include <cppi/TypeId.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cppi {

struct HostFunctionInfo {
    std::string name;
    std::vector<TypeId> params;
    TypeId result = types::Void;
    std::uint32_t cost = 1;  ///< extra operations charged per call
    HostFunction impl;
};

}  // namespace cppi
