#pragma once

/// @file HostEnumInfo.hpp
/// @brief What a HostRegistry knows about a registered enum.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/TypeId.hpp>

#include <string>
#include <vector>

namespace cppi {

/// An enum the host registered with HostRegistry::add_enum().
struct HostEnumInfo {
    /// The type id assigned to the enum.
    TypeId id;
    /// Name of the enum as player code spells it.
    std::string name;
    /// Enumerator names, in value order (the first is 0).
    std::vector<std::string> enumerators;
};

}  // namespace cppi
