#pragma once

/// @file HostFunctionInfo.hpp
/// @brief What a HostRegistry knows about a registered function.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/HostFunction.hpp>
#include <cppi/TypeId.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cppi {

/// Signature, cost and implementation of a registered host function.
struct HostFunctionInfo {
    /// Name player code calls it by.
    std::string name;
    /// Parameter types, in order.
    std::vector<TypeId> params;
    /// Return type.
    TypeId result = types::Void;
    std::uint32_t cost = 1;  ///< extra operations charged per call
    /// The host code that runs on each call.
    HostFunction impl;
};

}  // namespace cppi
