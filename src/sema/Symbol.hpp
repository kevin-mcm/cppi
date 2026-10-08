#pragma once

/// @file Symbol.hpp
/// @brief What a name means at some point of the program.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/TypeInfo.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace cppi::sema {

/// Forward declaration (see Scope.hpp).
class Scope;

/// A function a call may resolve to: one registered by the host, or one the
/// player wrote (an index in the FunctionTable).
struct Callee {
    /// A host function (`id` is a FunctionId value).
    bool host = false;
    /// Function id, host function id or intrinsic.
    std::uint32_t id = 0;
    bool intrinsic = false;  ///< `id` is a sema::Intrinsic
    friend bool operator==(const Callee&, const Callee&) = default;
};

/// What kind of entity a name denotes.
enum class SymbolKind : std::uint8_t {
    /// A variable (or static data member).
    Variable,
    Functions,  ///< an overload set
    Constant,   ///< enumerator
    Type,       ///< struct, class, enum or alias
    /// Function templates or a class template.
    Template,
    /// A namespace.
    Namespace,
    Poisoned,  ///< its declaration failed: stay silent about later uses
};

/// What a name means: the fields used depend on `kind`.
struct Symbol {
    /// The kind of entity.
    SymbolKind kind = SymbolKind::Poisoned;
    /// Where it is declared.
    SourceRange range;

    // Variable
    TypeRef type = 0;  ///< Variable: object type (referent for references); Constant/Type: the type
    /// Variable: declared const.
    bool is_const = false;
    /// Variable: a global (offset in the global area).
    bool global = false;
    /// Variable: a reference (the slot holds an address).
    bool reference = false;
    /// Variable: first cell in its frame or the global area.
    std::uint32_t offset = 0;
    std::optional<std::int64_t> constant;  ///< value of a const variable with a constant initializer

    // Constant (enumerator)
    /// Constant: the enumerator's value.
    std::int64_t value = 0;

    // Functions
    /// Functions: the overload set.
    std::vector<Callee> functions;

    // Template: function templates of this name (an overload set) or one class template
    /// Template: template ids.
    std::vector<std::uint32_t> templates;

    // Namespace: its members; class Type: its static members
    /// Namespace: its scope; Type (class): its static members.
    Scope* scope = nullptr;

    // Static data member: the class it belongs to, and its access (ast::Access)
    /// Static data member: its class.
    std::optional<std::uint32_t> member_of;
    /// Static data member: its access (an ast::Access).
    std::uint8_t member_access = 0;
};

}  // namespace cppi::sema
