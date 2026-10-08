#pragma once

/// How the analyzer describes a type. Types are interned in a TypeTable and
/// referred to by TypeRef, so comparing two types is comparing two integers.

#include <cstdint>

namespace cppi::sema {

using TypeRef = std::uint32_t;

enum class TypeKind : std::uint8_t {
    Error,  ///< the type of an expression that already produced an error
    Void,
    Bool,
    Char,
    Int,
    Long,
    Double,
    Short,
    UChar,
    UShort,
    UInt,
    ULong,
    Nullptr,
    Enum,
    Record,
    Array,
    Pointer,
    Reference,
};

struct TypeInfo {
    TypeKind kind = TypeKind::Error;
    TypeRef target = 0;         ///< element (Array) or pointee/referent (Pointer, Reference)
    bool target_const = false;  ///< pointer/reference to const
    std::uint32_t count = 0;    ///< Array: number of elements (0 = unknown bound); Reference: 1 for `&&`
    std::uint32_t decl = 0;     ///< Enum/Record: index in the TypeTable's enum/record list
};

}  // namespace cppi::sema
