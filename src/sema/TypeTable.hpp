#pragma once

/// Interns every type of a compilation and answers questions about them:
/// names for diagnostics, sizes, and the classification rules C++ uses for
/// conversions and operators.

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

class TypeTable {
public:
    static constexpr TypeRef kError = 0;
    static constexpr TypeRef kVoid = 1;
    static constexpr TypeRef kBool = 2;
    static constexpr TypeRef kChar = 3;
    static constexpr TypeRef kInt = 4;
    static constexpr TypeRef kLong = 5;
    static constexpr TypeRef kDouble = 6;
    static constexpr TypeRef kNullptr = 7;
    static constexpr TypeRef kShort = 8;
    static constexpr TypeRef kUChar = 9;
    static constexpr TypeRef kUShort = 10;
    static constexpr TypeRef kUInt = 11;
    static constexpr TypeRef kULong = 12;

    TypeTable();

    // --- Construction -----------------------------------------------------
    [[nodiscard]] TypeRef pointer_to(TypeRef target, bool target_const = false);
    /// `T&`, or `T&&` with `rvalue`.
    [[nodiscard]] TypeRef reference_to(TypeRef target, bool target_const = false, bool rvalue = false);
    [[nodiscard]] bool is_rvalue_reference(TypeRef t) const { return is_reference(t) && info(t).count == 1; }
    [[nodiscard]] TypeRef array_of(TypeRef element, std::uint32_t count);
    [[nodiscard]] TypeRef add_enum(EnumInfo info);
    [[nodiscard]] TypeRef add_record(RecordInfo info);

    /// The type for a host TypeId (builtins and registered enums).
    [[nodiscard]] std::optional<TypeRef> from_host(TypeId id) const;
    /// The host TypeId for a type, if the host knows it.
    [[nodiscard]] std::optional<TypeId> to_host(TypeRef type) const;
    void map_host_enum(TypeId id, TypeRef type);

    // --- Queries ------------------------------------------------------------
    [[nodiscard]] const TypeInfo& info(TypeRef type) const { return types_[type]; }
    [[nodiscard]] TypeKind kind(TypeRef type) const { return types_[type].kind; }
    [[nodiscard]] EnumInfo& enum_info(TypeRef type) { return enums_[types_[type].decl]; }
    [[nodiscard]] const EnumInfo& enum_info(TypeRef type) const { return enums_[types_[type].decl]; }
    [[nodiscard]] RecordInfo& record(TypeRef type) { return records_[types_[type].decl]; }
    [[nodiscard]] const RecordInfo& record(TypeRef type) const { return records_[types_[type].decl]; }
    [[nodiscard]] RecordInfo& record_at(std::uint32_t index) { return records_[index]; }
    [[nodiscard]] const RecordInfo& record_at(std::uint32_t index) const { return records_[index]; }
    [[nodiscard]] TypeRef record_type(std::uint32_t index) const { return record_types_[index]; }
    [[nodiscard]] std::size_t record_count() const noexcept { return records_.size(); }
    [[nodiscard]] std::size_t type_count() const noexcept { return types_.size(); }

    /// "int", "const char*", "int[3]", "Point&"...
    [[nodiscard]] std::string name(TypeRef type) const;

    [[nodiscard]] std::uint32_t cells(TypeRef type) const;
    [[nodiscard]] std::uint32_t byte_size(TypeRef type) const;
    [[nodiscard]] std::uint32_t byte_align(TypeRef type) const;

    [[nodiscard]] bool is_error(TypeRef t) const { return kind(t) == TypeKind::Error; }
    [[nodiscard]] bool is_void(TypeRef t) const { return kind(t) == TypeKind::Void; }
    [[nodiscard]] bool is_enum(TypeRef t) const { return kind(t) == TypeKind::Enum; }
    [[nodiscard]] bool is_scoped_enum(TypeRef t) const { return is_enum(t) && enum_info(t).scoped; }
    [[nodiscard]] bool is_record(TypeRef t) const { return kind(t) == TypeKind::Record; }
    [[nodiscard]] bool is_array(TypeRef t) const { return kind(t) == TypeKind::Array; }
    [[nodiscard]] bool is_pointer(TypeRef t) const { return kind(t) == TypeKind::Pointer; }
    [[nodiscard]] bool is_reference(TypeRef t) const { return kind(t) == TypeKind::Reference; }
    /// bool, the char, short, int and long families, and unscoped enums.
    [[nodiscard]] bool is_integral(TypeRef t) const;
    /// unsigned char, unsigned short, unsigned int, unsigned long.
    [[nodiscard]] bool is_unsigned(TypeRef t) const { return is_unsigned_kind(kind(t)); }
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
    TypeRef intern(TypeInfo info);

    // Deques: references to entries stay valid while new types are added
    // (templates are instantiated in the middle of binding).
    std::deque<TypeInfo> types_;
    std::map<std::tuple<TypeKind, TypeRef, bool, std::uint32_t>, TypeRef> interned_;
    std::deque<EnumInfo> enums_;
    std::deque<RecordInfo> records_;
    std::vector<TypeRef> record_types_;
    std::unordered_map<std::uint16_t, TypeRef> host_enums_;
};

}  // namespace cppi::sema
