#pragma once

/// What a name means at some point of the program.

#include "sema/TypeInfo.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

class Scope;

/// A function a call may resolve to: one registered by the host, or one the
/// player wrote (an index in the FunctionTable).
struct Callee {
    bool host = false;
    std::uint32_t id = 0;
    bool intrinsic = false;  ///< `id` is a sema::Intrinsic
    friend bool operator==(const Callee&, const Callee&) = default;
};

enum class SymbolKind : std::uint8_t {
    Variable,
    Functions,  ///< an overload set
    Constant,   ///< enumerator
    Type,       ///< struct, class, enum or alias
    Template,
    Namespace,
    Poisoned,  ///< its declaration failed: stay silent about later uses
};

struct Symbol {
    SymbolKind kind = SymbolKind::Poisoned;
    SourceRange range;

    // Variable
    TypeRef type = 0;  ///< Variable: object type (referent for references); Constant/Type: the type
    bool is_const = false;
    bool global = false;
    bool reference = false;
    std::uint32_t offset = 0;
    std::optional<std::int64_t> constant;  ///< value of a const variable with a constant initializer

    // Constant (enumerator)
    std::int64_t value = 0;

    // Functions
    std::vector<Callee> functions;

    // Template: function templates of this name (an overload set) or one class template
    std::vector<std::uint32_t> templates;

    // Namespace: its members; class Type: its static members
    Scope* scope = nullptr;

    // Static data member: the class it belongs to, and its access (ast::Access)
    std::optional<std::uint32_t> member_of;
    std::uint8_t member_access = 0;
};

}  // namespace cppi::sema
