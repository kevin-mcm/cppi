/// @file TypeTable.cpp
/// @brief Implementation of TypeTable.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/TypeTable.hpp"

#include <utility>

namespace cppi::sema {

TypeTable::TypeTable() {
    for (const TypeKind k : {TypeKind::Error, TypeKind::Void, TypeKind::Bool, TypeKind::Char, TypeKind::Int,
                             TypeKind::Long, TypeKind::Double, TypeKind::Nullptr, TypeKind::Short, TypeKind::UChar,
                             TypeKind::UShort, TypeKind::UInt, TypeKind::ULong}) {
        types_.push_back(TypeInfo{k, 0, false, 0, 0});
    }
}

TypeRef TypeTable::intern(TypeInfo info) {
    const auto key = std::tuple{info.kind, info.target, info.target_const, info.count};
    if (auto it = interned_.find(key); it != interned_.end()) {
        return it->second;
    }
    const auto ref = static_cast<TypeRef>(types_.size());
    types_.push_back(info);
    interned_.emplace(key, ref);
    return ref;
}

TypeRef TypeTable::pointer_to(TypeRef target, bool target_const) {
    return intern(TypeInfo{TypeKind::Pointer, target, target_const, 0, 0});
}

TypeRef TypeTable::reference_to(TypeRef target, bool target_const, bool rvalue) {
    return intern(TypeInfo{TypeKind::Reference, target, target_const, rvalue ? 1U : 0U, 0});
}

TypeRef TypeTable::array_of(TypeRef element, std::uint32_t count) {
    return intern(TypeInfo{TypeKind::Array, element, false, count, 0});
}

TypeRef TypeTable::add_enum(EnumInfo info) {
    const auto ref = static_cast<TypeRef>(types_.size());
    types_.push_back(TypeInfo{TypeKind::Enum, 0, false, 0, static_cast<std::uint32_t>(enums_.size())});
    enums_.push_back(std::move(info));
    return ref;
}

TypeRef TypeTable::add_record(RecordInfo info) {
    const auto ref = static_cast<TypeRef>(types_.size());
    types_.push_back(TypeInfo{TypeKind::Record, 0, false, 0, static_cast<std::uint32_t>(records_.size())});
    records_.push_back(std::move(info));
    record_types_.push_back(ref);
    return ref;
}

std::optional<TypeRef> TypeTable::from_host(TypeId id) const {
    if (id == types::Void) return kVoid;
    if (id == types::Bool) return kBool;
    if (id == types::Char) return kChar;
    if (id == types::Int) return kInt;
    if (id == types::Long) return kLong;
    if (id == types::Double) return kDouble;
    if (auto it = host_enums_.find(id.value); it != host_enums_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<TypeId> TypeTable::to_host(TypeRef type) const {
    switch (kind(type)) {
        case TypeKind::Void: return types::Void;
        case TypeKind::Bool: return types::Bool;
        case TypeKind::Char: return types::Char;
        case TypeKind::Int: return types::Int;
        case TypeKind::Long: return types::Long;
        case TypeKind::Double: return types::Double;
        case TypeKind::Enum: return enum_info(type).host_id;
        default: return std::nullopt;
    }
}

void TypeTable::map_host_enum(TypeId id, TypeRef type) {
    host_enums_[id.value] = type;
}

std::string TypeTable::name(TypeRef type) const {
    const TypeInfo& t = types_[type];
    switch (t.kind) {
        case TypeKind::Error: return "<error>";
        case TypeKind::Void: return "void";
        case TypeKind::Bool: return "bool";
        case TypeKind::Char: return "char";
        case TypeKind::Int: return "int";
        case TypeKind::Long: return "long";
        case TypeKind::Double: return "double";
        case TypeKind::Short: return "short";
        case TypeKind::UChar: return "unsigned char";
        case TypeKind::UShort: return "unsigned short";
        case TypeKind::UInt: return "unsigned int";
        case TypeKind::ULong: return "unsigned long";
        case TypeKind::Nullptr: return "std::nullptr_t";
        case TypeKind::Enum: return enums_[t.decl].name;
        case TypeKind::Record: return records_[t.decl].name;
        case TypeKind::Array:
            return name(t.target) + "[" + (t.count == 0 ? std::string() : std::to_string(t.count)) + "]";
        case TypeKind::Pointer: return (t.target_const ? "const " : "") + name(t.target) + "*";
        case TypeKind::Reference:
            return (t.target_const ? "const " : "") + name(t.target) + (t.count == 1 ? "&&" : "&");
    }
    return "<error>";
}

std::uint32_t TypeTable::cells(TypeRef type) const {
    const TypeInfo& t = types_[type];
    switch (t.kind) {
        case TypeKind::Error:
        case TypeKind::Void: return 0;
        case TypeKind::Array: return t.count * cells(t.target);
        case TypeKind::Record: return records_[t.decl].size;
        default: return 1;
    }
}

std::uint32_t TypeTable::byte_size(TypeRef type) const {
    const TypeInfo& t = types_[type];
    switch (t.kind) {
        case TypeKind::Error:
        case TypeKind::Void: return 0;
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::UChar: return 1;
        case TypeKind::Short:
        case TypeKind::UShort: return 2;
        case TypeKind::Int:
        case TypeKind::UInt:
        case TypeKind::Enum: return 4;
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::Double:
        case TypeKind::Nullptr:
        case TypeKind::Pointer:
        case TypeKind::Reference: return 8;
        case TypeKind::Array: return t.count * byte_size(t.target);
        case TypeKind::Record: return records_[t.decl].byte_size;
    }
    return 0;
}

std::uint32_t TypeTable::byte_align(TypeRef type) const {
    const TypeInfo& t = types_[type];
    switch (t.kind) {
        case TypeKind::Array: return byte_align(t.target);
        case TypeKind::Record: return records_[t.decl].byte_align;
        default: {
            const std::uint32_t size = byte_size(type);
            return size == 0 ? 1 : size;
        }
    }
}

std::uint32_t TypeTable::integer_bits(TypeKind k) {
    switch (k) {
        case TypeKind::Bool: return 1;
        case TypeKind::Char:
        case TypeKind::UChar: return 8;
        case TypeKind::Short:
        case TypeKind::UShort: return 16;
        case TypeKind::Int:
        case TypeKind::UInt:
        case TypeKind::Enum: return 32;
        case TypeKind::Long:
        case TypeKind::ULong: return 64;
        default: return 0;
    }
}

bool TypeTable::is_integral(TypeRef t) const {
    switch (kind(t)) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Int:
        case TypeKind::Long:
        case TypeKind::Short:
        case TypeKind::UChar:
        case TypeKind::UShort:
        case TypeKind::UInt:
        case TypeKind::ULong: return true;
        case TypeKind::Enum: return !enum_info(t).scoped;
        default: return false;
    }
}

bool TypeTable::is_arithmetic(TypeRef t) const {
    return is_integral(t) || kind(t) == TypeKind::Double;
}

bool TypeTable::is_scalar(TypeRef t) const {
    switch (kind(t)) {
        case TypeKind::Bool:
        case TypeKind::Char:
        case TypeKind::Int:
        case TypeKind::Long:
        case TypeKind::Double:
        case TypeKind::Short:
        case TypeKind::UChar:
        case TypeKind::UShort:
        case TypeKind::UInt:
        case TypeKind::ULong:
        case TypeKind::Nullptr:
        case TypeKind::Enum:
        case TypeKind::Pointer: return true;
        default: return false;
    }
}

}  // namespace cppi::sema
