#pragma once

/// Native operations the standard library prelude is built on. Their names
/// start with `__cppi_`, which player code cannot declare.

#include "sema/TypeTable.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cppi::sema {

enum class Intrinsic : std::uint8_t {
    WriteLong,    ///< void __cppi_write_long(long)
    WriteDouble,  ///< void __cppi_write_double(double)
    WriteChar,    ///< void __cppi_write_char(char)
    WriteString,  ///< void __cppi_write_string(const char*)
    CheckIndex,   ///< void __cppi_check_index(long index, long size): out-of-bounds if outside
    BadAccess,    ///< void __cppi_bad_access(bool ok): reading an empty std::optional when !ok
    WriteULong,   ///< void __cppi_write_ulong(unsigned long)
    Caught,       ///< pointer to the exception being handled (typed by the catch clause)
    Count,
};

class Intrinsics {
public:
    struct Info {
        std::string_view name;
        TypeRef result = TypeTable::kVoid;
        std::vector<TypeRef> params;
    };

    /// Signature of an intrinsic; `types` builds the pointer types.
    [[nodiscard]] static Info info(Intrinsic which, TypeTable& types) {
        switch (which) {
            case Intrinsic::WriteLong: return {"__cppi_write_long", TypeTable::kVoid, {TypeTable::kLong}};
            case Intrinsic::WriteDouble: return {"__cppi_write_double", TypeTable::kVoid, {TypeTable::kDouble}};
            case Intrinsic::WriteChar: return {"__cppi_write_char", TypeTable::kVoid, {TypeTable::kChar}};
            case Intrinsic::WriteString:
                return {"__cppi_write_string", TypeTable::kVoid, {types.pointer_to(TypeTable::kChar, true)}};
            case Intrinsic::CheckIndex:
                return {"__cppi_check_index", TypeTable::kVoid, {TypeTable::kLong, TypeTable::kLong}};
            case Intrinsic::BadAccess: return {"__cppi_bad_access", TypeTable::kVoid, {TypeTable::kBool}};
            case Intrinsic::WriteULong: return {"__cppi_write_ulong", TypeTable::kVoid, {TypeTable::kULong}};
            case Intrinsic::Caught: return {"__cppi_caught", TypeTable::kVoid, {}};
            case Intrinsic::Count: break;
        }
        return {};
    }
};

}  // namespace cppi::sema
