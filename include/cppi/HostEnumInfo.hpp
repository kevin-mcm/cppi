#pragma once

/// @file HostEnumInfo.hpp
/// What a HostRegistry knows about a registered enum.

#include <cppi/TypeId.hpp>

#include <string>
#include <vector>

namespace cppi {

struct HostEnumInfo {
    TypeId id;
    std::string name;
    std::vector<std::string> enumerators;
};

}  // namespace cppi
