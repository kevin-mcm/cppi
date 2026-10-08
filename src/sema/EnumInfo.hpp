#pragma once

/// @file EnumInfo.hpp
/// @brief An enumeration, declared by the player or registered by the host.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/TypeId.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cppi::sema {

/// An enumeration and its enumerators.
struct EnumInfo {
    /// Enum name.
    std::string name;
    bool scoped = false;            ///< enum class (C++11)
    std::optional<TypeId> host_id;  ///< set for enums registered by the host
    /// Enumerators (name, value), in declaration order.
    std::vector<std::pair<std::string, std::int64_t>> enumerators;

    /// The name of the first enumerator with `value`, or nullptr.
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
