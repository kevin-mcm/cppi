#pragma once

/// @file TypeTable.hpp
/// @brief Interns every type of a compilation and answers questions about them:
/// names for diagnostics, sizes, and the classification rules C++ uses for
/// conversions and operators.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/EnumInfo.hpp"
#include "sema/RecordInfo.hpp"
#include "sema/TypeInfo.hpp"

#include <cppi/TypeId.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace cppi::sema {

/// Every type of one compilation, interned: equal types have equal TypeRefs.
class TypeTable {
public:
    /// The error type.
    static constexpr TypeRef kError = 0;
    /// `void`
    static constexpr TypeRef kVoid = 1;
    /// `bool`
    static constexpr TypeRef kBool = 2;
    /// `char`
    static constexpr TypeRef kChar = 3;
    /// `int`
    static constexpr TypeRef kInt = 4;
    /// `long`
    static constexpr TypeRef kLong = 5;
    /// `double`
    static constexpr TypeRef kDouble = 6;
    /// `std::nullptr_t`
    static constexpr TypeRef kNullptr = 7;
    /// `short`
    static constexpr TypeRef kShort = 8;
    /// `unsigned char`
    static constexpr TypeRef kUChar = 9;
    /// `unsigned short`
    static constexpr TypeRef kUShort = 10;
    /// `unsigned int`
    static constexpr TypeRef kUInt = 11;
    /// `unsigned long`
    static constexpr TypeRef kULong = 12;

    /// A table with the builtin types at their fixed TypeRefs.
    TypeTable();

    // --- Construction -----------------------------------------------------
    /// `T*`, or `const T*` with `target_const`.
    [[nodiscard]] TypeRef pointer_to(TypeRef target, bool target_const = false);
    /// `T&`, or `T&&` with `rvalue`.
    [[nodiscard]] TypeRef reference_to(TypeRef target, bool target_const = false, bool rvalue = false);
    /// True for `T&&`.
    [[nodiscard]] bool is_rvalue_reference(TypeRef t) const { return is_reference(t) && info(t).count == 1; }
    /// `T[count]` (count 0: unknown bound).
    [[nodiscard]] TypeRef array_of(TypeRef element, std::uint32_t count);
    /// A new enum type.
    [[nodiscard]] TypeRef add_enum(EnumInfo info);
    /// A new record type.
    [[nodiscard]] TypeRef add_record(RecordInfo info);

    /// The type for a host TypeId (builtins and registered enums).
    [[nodiscard]] std::optional<TypeRef> from_host(TypeId id) const;
    /// The host TypeId for a type, if the host knows it.
    [[nodiscard]] std::optional<TypeId> to_host(TypeRef type) const;
    /// Records that host enum `id` is `type`.
    void map_host_enum(TypeId id, TypeRef type);

    // --- Queries ------------------------------------------------------------
    /// The entry of `type`.
    [[nodiscard]] const TypeInfo& info(TypeRef type) const { return types_[type]; }
    /// The kind of `type`.
    [[nodiscard]] TypeKind kind(TypeRef type) const { return types_[type].kind; }
    /// The enum `type` names (must be an enum).
    [[nodiscard]] EnumInfo& enum_info(TypeRef type) { return enums_[types_[type].decl]; }
    /// @copydoc enum_info(TypeRef)
    [[nodiscard]] const EnumInfo& enum_info(TypeRef type) const { return enums_[types_[type].decl]; }
    /// The record `type` names (must be a record).
    [[nodiscard]] RecordInfo& record(TypeRef type) { return records_[types_[type].decl]; }
    /// @copydoc record(TypeRef)
    [[nodiscard]] const RecordInfo& record(TypeRef type) const { return records_[types_[type].decl]; }
    /// The record with index `index`.
    [[nodiscard]] RecordInfo& record_at(std::uint32_t index) { return records_[index]; }
    /// @copydoc record_at(std::uint32_t)
    [[nodiscard]] const RecordInfo& record_at(std::uint32_t index) const { return records_[index]; }
    /// The type of the record with index `index`.
    [[nodiscard]] TypeRef record_type(std::uint32_t index) const { return record_types_[index]; }
    /// Number of records.
    [[nodiscard]] std::size_t record_count() const noexcept { return records_.size(); }
    /// Number of types.
    [[nodiscard]] std::size_t type_count() const noexcept { return types_.size(); }

    /// "int", "const char*", "int[3]", "Point&"...
    [[nodiscard]] std::string name(TypeRef type) const;

    /// Size of an object of `type` in VM cells.
    [[nodiscard]] std::uint32_t cells(TypeRef type) const;
    /// `sizeof(type)` as a native compiler would report it.
    [[nodiscard]] std::uint32_t byte_size(TypeRef type) const;
    /// `alignof(type)`.
    [[nodiscard]] std::uint32_t byte_align(TypeRef type) const;

    /// True for the error type.
    [[nodiscard]] bool is_error(TypeRef t) const { return kind(t) == TypeKind::Error; }
    /// True for `void`.
    [[nodiscard]] bool is_void(TypeRef t) const { return kind(t) == TypeKind::Void; }
    /// True for enumerations.
    [[nodiscard]] bool is_enum(TypeRef t) const { return kind(t) == TypeKind::Enum; }
    /// True for `enum class` types.
    [[nodiscard]] bool is_scoped_enum(TypeRef t) const { return is_enum(t) && enum_info(t).scoped; }
    /// True for structs and classes.
    [[nodiscard]] bool is_record(TypeRef t) const { return kind(t) == TypeKind::Record; }
    /// True for arrays.
    [[nodiscard]] bool is_array(TypeRef t) const { return kind(t) == TypeKind::Array; }
    /// True for pointers.
    [[nodiscard]] bool is_pointer(TypeRef t) const { return kind(t) == TypeKind::Pointer; }
    /// True for references.
    [[nodiscard]] bool is_reference(TypeRef t) const { return kind(t) == TypeKind::Reference; }
    /// bool, the char, short, int and long families, and unscoped enums.
    [[nodiscard]] bool is_integral(TypeRef t) const;
    /// unsigned char, unsigned short, unsigned int, unsigned long.
    [[nodiscard]] bool is_unsigned(TypeRef t) const { return is_unsigned_kind(kind(t)); }
    /// True for the unsigned integer kinds.
    [[nodiscard]] static bool is_unsigned_kind(TypeKind k) {
        return k == TypeKind::UChar || k == TypeKind::UShort || k == TypeKind::UInt || k == TypeKind::ULong;
    }
    /// Bits of an integer type (bool: 1; enums: 32); 0 for anything else.
    [[nodiscard]] static std::uint32_t integer_bits(TypeKind k);
    /// Integral or double.
    [[nodiscard]] bool is_arithmetic(TypeRef t) const;
    /// Values that fit in one cell: arithmetic, enums, pointers, nullptr.
    [[nodiscard]] bool is_scalar(TypeRef t) const;

private:
    /// The TypeRef of `info`, adding it the first time.
    TypeRef intern(TypeInfo info);

    // Deques: references to entries stay valid while new types are added
    // (templates are instantiated in the middle of binding).
    /// Entries, by TypeRef.
    std::deque<TypeInfo> types_;
    /// Structural key -> TypeRef, for interning.
    std::map<std::tuple<TypeKind, TypeRef, bool, std::uint32_t>, TypeRef> interned_;
    /// Enums, by index.
    std::deque<EnumInfo> enums_;
    /// Records, by index.
    std::deque<RecordInfo> records_;
    /// The type of each record.
    std::vector<TypeRef> record_types_;
    /// Host TypeId value -> type.
    std::unordered_map<std::uint16_t, TypeRef> host_enums_;
};

}  // namespace cppi::sema
