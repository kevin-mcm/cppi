#pragma once

/// @file RecordInfo.hpp
/// @brief A struct or class: its fields, bases, member functions and layout.
///
/// Layout is measured in VM cells (one scalar per cell) and, for sizeof, in the
/// bytes a typical 64-bit compiler would use.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/TypeInfo.hpp"

#include "ast/Stmt.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cppi::sema {

/// A data member of a record.
struct FieldInfo {
    /// Field name.
    std::string name;
    /// Its type.
    TypeRef type = 0;
    /// Declared `const`.
    bool is_const = false;
    std::uint32_t offset = 0;  ///< in cells, from the start of the record
    /// Member access.
    ast::Access access = ast::Access::Public;
    const ast::VarDeclarator* default_init = nullptr;  ///< `int hp = 100;` (C++11)
    /// Where it is declared.
    SourceRange range;
    bool deref = false;  ///< a lambda's by-reference capture: the cell points to the variable
};

/// A direct base class of a record.
struct BaseInfo {
    std::uint32_t record = 0;  ///< index in the TypeTable's record list
    /// The base's type.
    TypeRef type = 0;
    std::uint32_t offset = 0;  ///< cells; for virtual bases, the offset in this class as a complete object
    /// Virtual inheritance.
    bool is_virtual = false;
    /// Inheritance access.
    ast::Access access = ast::Access::Public;
    /// Where it is written.
    SourceRange range;
};

/// One entry of a virtual table: which function runs for a virtual call,
/// and where the subobject it expects as `this` sits in the complete object.
struct VirtualSlot {
    /// Name of the virtual function.
    std::string name;
    /// The final overrider.
    std::uint32_t function = 0;
    std::uint32_t this_offset = 0;  ///< cells from the complete object to the overrider's class
    /// The overrider is pure virtual (calling it is undefined behavior).
    bool is_pure = false;
};

/// Forward declaration (see Scope.hpp).
class Scope;

/// Everything known about a struct or class: members, bases, layout and
/// virtual tables.
struct RecordInfo {
    /// Class name.
    std::string name;
    Scope* home = nullptr;     ///< the namespace it was declared in (argument-dependent lookup)
    Scope* statics = nullptr;  ///< static data members and static member functions
    /// Declared with `class` (private by default).
    bool is_class = false;
    /// The definition has been seen.
    bool complete = false;
    /// Declared `final`.
    bool is_final = false;
    /// Where it is declared.
    SourceRange range;

    std::vector<FieldInfo> fields;                                           ///< own fields only, in declaration order
    std::vector<BaseInfo> bases;                                             ///< direct bases, in declaration order
    std::map<std::string, std::vector<std::uint32_t>, std::less<>> methods;  ///< name -> function ids
    std::map<std::string, std::vector<std::uint32_t>, std::less<>> method_templates;  ///< name -> template ids
    std::vector<std::uint32_t> constructors;  ///< including an implicit copy constructor
    /// The implicit default constructor, written when `new` needs one (see
    /// DeclarationBinder::implicit_default_constructor). Not in `constructors`.
    std::optional<std::uint32_t> implicit_default_constructor;
    bool user_constructors = false;  ///< the class declares constructors itself
    /// The destructor, if any.
    std::optional<std::uint32_t> destructor;
    /// Copies are plain cell copies (no copy constructor runs anywhere inside).
    bool trivial_copy = true;
    /// Assignments are plain cell copies (no operator= runs anywhere inside).
    bool trivial_assign = true;

    /// Has a header cell (it, or a base, declares virtual functions or has
    /// virtual bases). The header lives at offset 0 of the subobject.
    bool has_header = false;
    /// Has a pure virtual function that is not overridden.
    bool is_abstract = false;

    std::uint32_t size = 0;       ///< cells, as a complete object (virtual bases included)
    std::uint32_t nv_size = 0;    ///< cells, as a base subobject (virtual bases excluded)
    std::uint32_t byte_size = 0;  ///< sizeof
    /// alignof
    std::uint32_t byte_align = 1;

    /// Virtual functions this class declares or overrides, in declaration
    /// order. A virtual call names (owner class, index in this list).
    std::vector<std::uint32_t> virtual_functions;

    /// Complete-object layout: where each virtual base sits.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> virtual_base_offsets;  ///< (record, offset)

    /// Complete-object dispatch: for each polymorphic subobject (its offset),
    /// the final overrider of each of its class's virtual functions.
    struct Subobject {
        /// Cells from the complete object to the subobject.
        std::uint32_t offset = 0;
        /// The subobject's class.
        std::uint32_t record = 0;
        /// Its virtual table.
        std::vector<VirtualSlot> slots;
    };
    /// Every polymorphic subobject.
    std::vector<Subobject> subobjects;
};

}  // namespace cppi::sema
