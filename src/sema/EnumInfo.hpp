#pragma once

/// An enumeration, declared by the player or registered by the host.

#include <cppi/TypeId.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cppi::sema {

struct EnumInfo {
    std::string name;
    bool scoped = false;            ///< enum class (C++11)
    std::optional<TypeId> host_id;  ///< set for enums registered by the host
    std::vector<std::pair<std::string, std::int64_t>> enumerators;

    [[nodiscard]] const std::string* enumerator_name(std::int64_t value) const noexcept {
        for (const auto& entry : enumerators) {
            if (entry.second == value) {
                return &entry.first;
            }
        }
        return nullptr;
    }
};

}  // namespace cppi::sema
