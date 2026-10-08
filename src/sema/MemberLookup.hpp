#pragma once

/// Finds a member (field or member functions) by name in a class and its
/// bases, the way C++ name lookup does: a name in the class hides names in
/// its bases; the same name in two different base subobjects is ambiguous.

#include "sema/ClassHierarchy.hpp"
#include "sema/TypeTable.hpp"

#include "ast/Stmt.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cppi::sema {

struct MemberResult {
    enum class Kind : std::uint8_t { None, Field, Methods, Ambiguous };
    Kind kind = Kind::None;
    std::uint32_t owner = 0;                              ///< record that declares it
    const FieldInfo* field = nullptr;                     ///< Field
    const std::vector<std::uint32_t>* methods = nullptr;  ///< Methods: function ids
    ast::Access access = ast::Access::Public;             ///< effective access through the inheritance path
};

class MemberLookup {
public:
    MemberLookup(const TypeTable& types, const ClassHierarchy& hierarchy) noexcept
        : types_(types), hierarchy_(hierarchy) {}

    [[nodiscard]] MemberResult find(std::uint32_t record, std::string_view name) const;

    /// Every field and method name reachable from `record` (suggestions).
    [[nodiscard]] std::vector<std::string> names(std::uint32_t record) const;

    /// Can code inside `context` (a member function of that record, or none)
    /// use a member of `owner` with `access`?
    [[nodiscard]] bool accessible(ast::Access access, std::uint32_t owner, std::optional<std::uint32_t> context) const;

private:
    void search(std::uint32_t record, std::string_view name, ast::Access path_access,
                std::vector<MemberResult>& found) const;

    const TypeTable& types_;
    const ClassHierarchy& hierarchy_;
};

}  // namespace cppi::sema
