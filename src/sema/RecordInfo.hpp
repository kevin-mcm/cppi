#pragma once

/// A struct or class: its fields, bases, member functions and layout.
/// Layout is measured in VM cells (one scalar per cell) and, for sizeof, in
/// the bytes a typical 64-bit compiler would use.

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

struct FieldInfo {
    std::string name;
    TypeRef type = 0;
    bool is_const = false;
    std::uint32_t offset = 0;  ///< in cells, from the start of the record
    ast::Access access = ast::Access::Public;
    const ast::VarDeclarator* default_init = nullptr;  ///< `int hp = 100;` (C++11)
    SourceRange range;
    bool deref = false;  ///< a lambda's by-reference capture: the cell points to the variable
};

struct BaseInfo {
    std::uint32_t record = 0;  ///< index in the TypeTable's record list
    TypeRef type = 0;
    std::uint32_t offset = 0;  ///< cells; for virtual bases, the offset in this class as a complete object
    bool is_virtual = false;
    ast::Access access = ast::Access::Public;
    SourceRange range;
};

/// One entry of a virtual table: which function runs for a virtual call,
/// and where the subobject it expects as `this` sits in the complete object.
struct VirtualSlot {
    std::string name;
    std::uint32_t function = 0;
    std::uint32_t this_offset = 0;  ///< cells from the complete object to the overrider's class
    bool is_pure = false;
};

class Scope;

struct RecordInfo {
    std::string name;
    Scope* home = nullptr;     ///< the namespace it was declared in (argument-dependent lookup)
    Scope* statics = nullptr;  ///< static data members and static member functions
    bool is_class = false;
    bool complete = false;
    bool is_final = false;
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
    std::optional<std::uint32_t> destructor;
    /// Copies are plain cell copies (no copy constructor runs anywhere inside).
    bool trivial_copy = true;
    /// Assignments are plain cell copies (no operator= runs anywhere inside).
    bool trivial_assign = true;

    /// Has a header cell (it, or a base, declares virtual functions or has
    /// virtual bases). The header lives at offset 0 of the subobject.
    bool has_header = false;
    bool is_abstract = false;

    std::uint32_t size = 0;       ///< cells, as a complete object (virtual bases included)
    std::uint32_t nv_size = 0;    ///< cells, as a base subobject (virtual bases excluded)
    std::uint32_t byte_size = 0;  ///< sizeof
    std::uint32_t byte_align = 1;

    /// Virtual functions this class declares or overrides, in declaration
    /// order. A virtual call names (owner class, index in this list).
    std::vector<std::uint32_t> virtual_functions;

    /// Complete-object layout: where each virtual base sits.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> virtual_base_offsets;  ///< (record, offset)

    /// Complete-object dispatch: for each polymorphic subobject (its offset),
    /// the final overrider of each of its class's virtual functions.
    struct Subobject {
        std::uint32_t offset = 0;
        std::uint32_t record = 0;
        std::vector<VirtualSlot> slots;
    };
    std::vector<Subobject> subobjects;
};

}  // namespace cppi::sema
