#pragma once

/// @file TypeId.hpp
/// @brief Identifiers for the types known to the interpreter.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <compare>
#include <cstdint>

namespace cppi {

/// Identifies a type known to the interpreter. Builtin types have fixed ids;
/// enums registered by the host get ids starting at kFirstUserType.
struct TypeId {
    /// The id itself.
    std::uint16_t value = 0;

    friend constexpr auto operator<=>(const TypeId&, const TypeId&) = default;
};

/// Ids of the builtin types.
namespace types {
/// `void`.
inline constexpr TypeId Void{0};
/// `bool`.
inline constexpr TypeId Bool{1};
/// `int`.
inline constexpr TypeId Int{2};
/// `char`.
inline constexpr TypeId Char{3};
/// `long`.
inline constexpr TypeId Long{4};
/// `double`.
inline constexpr TypeId Double{5};
/// First id given to enums registered by the host.
inline constexpr std::uint16_t kFirstUserType = 16;
}  // namespace types

}  // namespace cppi
