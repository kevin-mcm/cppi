#pragma once

/// @file TypeInfo.hpp
/// @brief How the analyzer describes a type.
///
/// Types are interned in a TypeTable and referred to by TypeRef, so comparing
/// two types is comparing two integers.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>

namespace cppi::sema {

/// Index of a type in the TypeTable.
using TypeRef = std::uint32_t;

/// The kinds of types the analyzer knows.
enum class TypeKind : std::uint8_t {
    Error,  ///< the type of an expression that already produced an error
    /// `void`
    Void,
    /// `bool`
    Bool,
    /// `char` (signed)
    Char,
    /// `int`
    Int,
    /// `long` / `long long` (64 bits)
    Long,
    /// `double`, `float` and `long double`
    Double,
    /// `short`
    Short,
    /// `unsigned char`
    UChar,
    /// `unsigned short`
    UShort,
    /// `unsigned int`
    UInt,
    /// `unsigned long` / `unsigned long long`
    ULong,
    /// `std::nullptr_t`
    Nullptr,
    /// An enumeration.
    Enum,
    /// A struct or class.
    Record,
    /// An array.
    Array,
    /// A pointer.
    Pointer,
    /// An lvalue or rvalue reference.
    Reference,
};

/// One entry of the TypeTable.
struct TypeInfo {
    /// The kind of type.
    TypeKind kind = TypeKind::Error;
    TypeRef target = 0;         ///< element (Array) or pointee/referent (Pointer, Reference)
    bool target_const = false;  ///< pointer/reference to const
    std::uint32_t count = 0;    ///< Array: number of elements (0 = unknown bound); Reference: 1 for `&&`
    std::uint32_t decl = 0;     ///< Enum/Record: index in the TypeTable's enum/record list
};

}  // namespace cppi::sema
