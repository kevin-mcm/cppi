#pragma once

/// The compiled, immutable contents of a Program.
///
/// Design notes:
///  * Source locations live in a parallel array, so they cost nothing in the
///    hot loop but are always available for diagnostics and debuggers.
///  * The maximum operand-stack depth is computed at compile time; the VM
///    reserves it once.
///  * Functions, records and debug information are plain tables the VM and
///    debuggers index directly.

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

struct ConstantEntry {
    std::int64_t bits = 0;
    std::string text;  ///< "East : Direction", for listings
};

struct ParamMeta {
    std::uint32_t offset = 0;
    std::uint32_t cells = 1;
    bool copy_block = false;  ///< argument is a pointer to a record copied into the frame
};

/// How the VM renders a value (debuggers, traces).
enum class ValueKind : std::uint8_t {
    Void,
    Bool,
    Char,
    Int,
    Long,
    ULong,
    Double,
    Enum,
    Pointer,
    Array,
    Record,
    Nullptr
};

struct TypeMeta {
    ValueKind kind = ValueKind::Void;
    std::string name;  ///< "int", "Point", "int*"...
    std::uint32_t cells = 1;
    std::uint32_t element = 0;             ///< arrays: element type index
    std::uint32_t count = 0;               ///< arrays
    std::vector<std::string> enumerators;  ///< enums (value = position, or see enum_values)
    std::vector<std::int64_t> enum_values;
    struct Field {
        std::string name;
        std::uint32_t offset = 0;
        std::uint32_t type = 0;
    };
    std::vector<Field> fields;  ///< records: every field, bases flattened
};

struct VariableMeta {
    std::string name;
    std::uint32_t offset = 0;
    std::uint32_t type = 0;  ///< index in ProgramData::types
    bool reference = false;
    SourceRange scope;  ///< where it is visible
};

struct FunctionMeta {
    static constexpr std::uint32_t kNoCode = 0xFFFFFFFF;  ///< never called: no code generated

    std::string name;
    std::uint32_t entry = 0;
    std::uint32_t frame_cells = 0;
    std::vector<ParamMeta> params;
    bool returns_value = false;
    bool is_pure = false;
    std::optional<std::uint32_t> record;  ///< member functions: the class that declares it
    bool library = false;                 ///< standard library: free by default, hidden from debuggers
    std::vector<VariableMeta> locals;
};

struct RecordMeta {
    std::string name;
    std::uint32_t cells = 0;
    std::vector<std::uint32_t> header_offsets;  ///< polymorphic subobjects of a complete object
    struct Slot {
        std::uint32_t function = 0;
        std::uint32_t this_offset = 0;
    };
    struct Subobject {
        std::uint32_t offset = 0;
        std::uint32_t record = 0;
        std::vector<Slot> slots;
    };
    std::vector<Subobject> subobjects;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> virtual_bases;  ///< (record, offset)
};

/// A type some `throw` uses.
struct ThrownMeta {
    std::string name;
    std::optional<std::uint32_t> destructor;
    std::optional<std::uint32_t> message_offset;  ///< std::exception: cell holding the message's char*
};

/// One catch clause: where its handler starts and which thrown types it takes.
struct CatchClauseMeta {
    std::uint32_t pc = 0;
    bool catch_all = false;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> matches;  ///< (thrown index, cells to the caught base)
};

struct ProgramData {
    std::vector<Instruction> code;
    std::vector<SourceRange> locations;  ///< parallel to `code`
    /// Parallel to `code`: the instruction starts a statement of the player's,
    /// or the evaluation of a condition (what CostModel::Unit::Statement charges).
    std::vector<bool> statement_starts;
    std::vector<ConstantEntry> constants;
    std::size_t max_stack = 0;
    std::shared_ptr<const HostRegistry> host;

    std::vector<FunctionMeta> functions;  ///< [0] is the script
    std::vector<RecordMeta> records;
    std::uint32_t global_cells = 0;
    std::vector<VariableMeta> globals;
    std::vector<TypeMeta> types;
    std::vector<ThrownMeta> thrown;
    std::vector<std::pair<std::uint32_t, std::string>> strings;  ///< global offset -> text (string literals)
    std::vector<std::vector<CatchClauseMeta>> catch_tables;      ///< one per try statement
};

}  // namespace cppi::detail
