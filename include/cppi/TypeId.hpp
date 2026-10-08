#pragma once

/// @file TypeId.hpp
/// Identifiers for the types known to the interpreter.

#include <compare>
#include <cstdint>

namespace cppi {

/// Identifies a type known to the interpreter. Builtin types have fixed ids;
/// enums registered by the host get ids starting at kFirstUserType.
struct TypeId {
    std::uint16_t value = 0;

    friend constexpr auto operator<=>(const TypeId&, const TypeId&) = default;
};

namespace types {
inline constexpr TypeId Void{0};
inline constexpr TypeId Bool{1};
inline constexpr TypeId Int{2};
inline constexpr TypeId Char{3};
inline constexpr TypeId Long{4};
inline constexpr TypeId Double{5};
inline constexpr std::uint16_t kFirstUserType = 16;
}  // namespace types

}  // namespace cppi
