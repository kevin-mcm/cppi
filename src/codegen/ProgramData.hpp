#pragma once

/// @file ProgramData.hpp
/// @brief The compiled, immutable contents of a Program.
///
/// Design notes:
///  * Source locations live in a parallel array, so they cost nothing in the
///    hot loop but are always available for diagnostics and debuggers.
///  * The maximum operand-stack depth is computed at compile time; the VM
///    reserves it once.
///  * Functions, records and debug information are plain tables the VM and
///    debuggers index directly.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/Instruction.hpp"

#include <cppi/HostRegistry.hpp>
#include <cppi/SourceRange.hpp>
#include <cppi/Value.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cppi::detail {

/// One entry of the constant table.
struct ConstantEntry {
    /// The value's bits.
    std::int64_t bits = 0;
    std::string text;  ///< "East : Direction", for listings
};

/// Where one parameter lives in its function's frame.
struct ParamMeta {
    /// First cell, from the start of the frame.
    std::uint32_t offset = 0;
    /// Size in cells.
    std::uint32_t cells = 1;
    bool copy_block = false;  ///< argument is a pointer to a record copied into the frame
};

/// How the VM renders a value (debuggers, traces).
enum class ValueKind : std::uint8_t {
    /// No value.
    Void,
    /// `bool`
    Bool,
    /// `char`
    Char,
    /// `int`
    Int,
    /// `long`, `short` and unsigned types up to 32 bits.
    Long,
    /// `unsigned long`
    ULong,
    /// Floating point.
    Double,
    /// Enumerations: rendered by enumerator name.
    Enum,
    /// Pointers and references.
    Pointer,
    /// Arrays: rendered element by element.
    Array,
    /// Structs and classes: rendered field by field.
    Record,
    /// `std::nullptr_t`
    Nullptr
};

/// What the VM knows about a type, for rendering values.
struct TypeMeta {
    /// How values of this type are rendered.
    ValueKind kind = ValueKind::Void;
    std::string name;  ///< "int", "Point", "int*"...
    /// Size in cells.
    std::uint32_t cells = 1;
    std::uint32_t element = 0;             ///< arrays: element type index
    std::uint32_t count = 0;               ///< arrays
    std::vector<std::string> enumerators;  ///< enums (value = position, or see enum_values)
    /// Value of each enumerator, parallel to `enumerators`.
    std::vector<std::int64_t> enum_values;
    /// A field of a record, at its offset in the complete object.
    struct Field {
        /// Field name.
        std::string name;
        /// First cell, from the start of the object.
        std::uint32_t offset = 0;
        /// Index in ProgramData::types.
        std::uint32_t type = 0;
    };
    std::vector<Field> fields;  ///< records: every field, bases flattened
};

/// A variable, for debuggers.
struct VariableMeta {
    /// Variable name.
    std::string name;
    /// First cell in its frame (or in the globals).
    std::uint32_t offset = 0;
    std::uint32_t type = 0;  ///< index in ProgramData::types
    /// A reference: the cell holds the address of the referred object.
    bool reference = false;
    SourceRange scope;  ///< where it is visible
};

/// A function of the program.
struct FunctionMeta {
    static constexpr std::uint32_t kNoCode = 0xFFFFFFFF;  ///< never called: no code generated

    /// Name, for call stacks and listings.
    std::string name;
    /// Index of its first instruction, or kNoCode.
    std::uint32_t entry = 0;
    /// Size of its frame: parameters, locals and temporaries.
    std::uint32_t frame_cells = 0;
    /// Parameters, in order.
    std::vector<ParamMeta> params;
    /// Returns something other than void.
    bool returns_value = false;
    /// Pure virtual: calling it is undefined behavior.
    bool is_pure = false;
    std::optional<std::uint32_t> record;  ///< member functions: the class that declares it
    bool library = false;                 ///< standard library: free by default, hidden from debuggers
    /// Parameters and locals, for debuggers.
    std::vector<VariableMeta> locals;
};

/// A class, as the VM needs it for virtual calls and object headers.
struct RecordMeta {
    /// Class name.
    std::string name;
    /// Size of a complete object, in cells.
    std::uint32_t cells = 0;
    std::vector<std::uint32_t> header_offsets;  ///< polymorphic subobjects of a complete object
    /// One virtual table entry: the final overrider and how to adjust `this`.
    struct Slot {
        /// The function called.
        std::uint32_t function = 0;
        /// Cells from the subobject to the overrider's class.
        std::uint32_t this_offset = 0;
    };
    /// A polymorphic subobject of a complete object and its virtual table.
    struct Subobject {
        /// First cell, from the start of the complete object.
        std::uint32_t offset = 0;
        /// The subobject's class.
        std::uint32_t record = 0;
        /// Its virtual table.
        std::vector<Slot> slots;
    };
    /// Every polymorphic subobject.
    std::vector<Subobject> subobjects;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> virtual_bases;  ///< (record, offset)
};

/// A type some `throw` uses.
struct ThrownMeta {
    /// Type name, for diagnostics.
    std::string name;
    /// Destructor to run when a handler is done with the exception, if any.
    std::optional<std::uint32_t> destructor;
    std::optional<std::uint32_t> message_offset;  ///< std::exception: cell holding the message's char*
};

/// One catch clause: where its handler starts and which thrown types it takes.
struct CatchClauseMeta {
    /// First instruction of the handler.
    std::uint32_t pc = 0;
    /// `catch (...)`.
    bool catch_all = false;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> matches;  ///< (thrown index, cells to the caught base)
};

/// Everything a compiled program is made of.
struct ProgramData {
    /// The bytecode of every function, back to back.
    std::vector<Instruction> code;
    std::vector<SourceRange> locations;  ///< parallel to `code`
    /// Parallel to `code`: the instruction starts a statement of the player's,
    /// or the evaluation of a condition (what CostModel::Unit::Statement charges).
    std::vector<bool> statement_starts;
    /// The constant table (PushConst operands index it).
    std::vector<ConstantEntry> constants;
    /// Deepest operand stack any function can reach.
    std::size_t max_stack = 0;
    /// The host registry the program was compiled against.
    std::shared_ptr<const HostRegistry> host;

    std::vector<FunctionMeta> functions;  ///< [0] is the script
    /// Classes, by record id.
    std::vector<RecordMeta> records;
    /// Size of the global area, in cells.
    std::uint32_t global_cells = 0;
    /// Global variables, for debuggers.
    std::vector<VariableMeta> globals;
    /// Every type, by type index.
    std::vector<TypeMeta> types;
    /// Types thrown by some `throw`, by thrown index.
    std::vector<ThrownMeta> thrown;
    std::vector<std::pair<std::uint32_t, std::string>> strings;  ///< global offset -> text (string literals)
    std::vector<std::vector<CatchClauseMeta>> catch_tables;      ///< one per try statement
};

}  // namespace cppi::detail
