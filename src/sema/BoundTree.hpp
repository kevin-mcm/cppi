#pragma once

/// The bound tree: the program after name resolution and type checking.
/// Every expression knows its type and whether it denotes an object
/// (lvalue) or a value (rvalue); every conversion C++ performs implicitly is
/// an explicit node; every variable is a slot in a frame or in the globals.
/// Code generation is a straightforward walk that cannot fail.

#include "sema/TypeInfo.hpp"

#include <cppi/FunctionId.hpp>
#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cppi::sema {

struct BExpr;
struct BStmt;
using BExprPtr = std::unique_ptr<BExpr>;
using BStmtPtr = std::unique_ptr<BStmt>;

/// How an operation computes: the VM has separate checked integer
/// arithmetic for 32 and 64 bits, IEEE doubles, and pointers.
/// How the VM computes with a value: signed integers trap on overflow,
/// unsigned ones wrap around.
enum class ScalarKind : std::uint8_t { Int32, Int64, Double, Pointer, UInt32, UInt64 };

enum class BinOp : std::uint8_t {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Shl,
    Shr,
    BitAnd,
    BitOr,
    BitXor,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    PtrAdd,   ///< pointer + integer
    PtrSub,   ///< pointer - integer
    PtrDiff,  ///< pointer - pointer
};

enum class UnOp : std::uint8_t { Neg, BitNot, Not };

enum class Conv : std::uint8_t {
    IntToDouble,
    DoubleToInt32,
    DoubleToInt64,
    IntToBool,
    DoubleToBool,
    PtrToBool,
    Trunc32,   ///< long -> int (wraps, implementation-defined before C++20)
    Trunc8,    ///< -> char
    Trunc16,   ///< -> short
    TruncU8,   ///< -> unsigned char (modulo 2^8)
    TruncU16,  ///< -> unsigned short
    TruncU32,  ///< -> unsigned int
    DoubleToUInt32,
    DoubleToUInt64,
    ULongToDouble,
    Retype,      ///< same bits, different type (enum -> int)
    ArrayDecay,  ///< array lvalue -> pointer to its first element
    PtrOffset,   ///< derived* -> base*: add `offset` cells (null stays null)
};

// --- Expressions ----------------------------------------------------------------------

struct BConst {
    std::int64_t bits = 0;  ///< integers as is; doubles bit-cast
};

/// A named object. For references, the slot holds a pointer to the object.
struct BVar {
    std::uint32_t offset = 0;
    bool global = false;
    bool reference = false;
    std::uint32_t cells = 1;               ///< size of the object it denotes
    std::optional<std::int64_t> constant;  ///< const variable with a known value
    std::string name;                      ///< for messages
};

struct BDeref {
    BExprPtr pointer;
    std::uint32_t cells = 1;
};

struct BAddressOf {
    BExprPtr lvalue;
};

struct BMember {
    BExprPtr base;  ///< lvalue of the record
    std::uint32_t offset = 0;
    std::uint32_t cells = 1;
    bool is_base = false;  ///< a base subobject: pointers keep the whole object's bounds
};

struct BIndex {
    BExprPtr base;  ///< array lvalue, or pointer rvalue when base_is_pointer
    BExprPtr index;
    std::uint32_t elem_cells = 1;
    std::uint32_t bound = 0;  ///< known array size (0 = rely on the pointer's bounds)
    bool base_is_pointer = false;
};

struct BLoad {
    BExprPtr lvalue;
};

struct BUnary {
    UnOp op = UnOp::Neg;
    ScalarKind kind = ScalarKind::Int32;
    BExprPtr operand;
};

struct BBinary {
    BinOp op = BinOp::Add;
    ScalarKind kind = ScalarKind::Int32;  ///< kind of the operands
    BExprPtr lhs;
    BExprPtr rhs;
    std::uint32_t elem_cells = 1;  ///< pointer arithmetic
};

struct BLogical {
    bool is_and = true;
    BExprPtr lhs;
    BExprPtr rhs;
};

/// `target = value`, or `target op= value` computed in `kind` and converted
/// back with `store_conv`. Yields the target (an lvalue).
struct BAssign {
    std::optional<BinOp> op;
    ScalarKind kind = ScalarKind::Int32;
    BExprPtr target;
    BExprPtr value;
    std::vector<Conv> load_conv;     ///< compound: target's value -> kind
    std::vector<Conv> store_conv;    ///< compound: result -> target's type
    std::uint32_t record_cells = 0;  ///< non-zero: record copy assignment
    std::uint32_t elem_cells = 1;    ///< pointer += n
};

struct BIncDec {
    bool increment = true;
    bool prefix = true;
    ScalarKind kind = ScalarKind::Int32;
    BExprPtr target;
    std::vector<Conv> store_conv;  ///< char targets wrap
    std::uint32_t elem_cells = 1;
};

struct BConditional {
    BExprPtr condition;
    BExprPtr then_expr;
    BExprPtr else_expr;
};

struct BCall {
    std::uint32_t function = 0;
    std::vector<BExprPtr> args;                 ///< values; pointers for reference and record parameters
    std::optional<std::uint32_t> result_temp;   ///< frame slot receiving a returned record
    BExprPtr result_target;                     ///< or the object to build the result in directly (copy elision)
    std::optional<std::uint32_t> virtual_slot;  ///< dynamic dispatch through args[0] (this)
};

struct BHostCall {
    FunctionId function;
    std::vector<BExprPtr> args;
};

struct BIntrinsic {
    std::uint8_t which = 0;  ///< sema::Intrinsic
    std::vector<BExprPtr> args;
};

struct BConvert {
    Conv conv = Conv::Retype;
    BExprPtr operand;
    std::int64_t offset = 0;  ///< PtrOffset: static part
    std::uint32_t cells = 0;  ///< ArrayDecay: element cells
    /// PtrOffset through a virtual base: its record index; its position is
    /// read from the object's header at runtime, then `offset` is added.
    std::optional<std::uint32_t> virtual_base;
};

struct BComma {
    BExprPtr lhs;
    BExprPtr rhs;
};

/// A temporary object in the current frame, initialized by `init`
/// (binding `const int&` to a value, calls returning records).
struct BTemp {
    std::uint32_t offset = 0;
    std::uint32_t cells = 1;
    BExprPtr init;                  ///< scalar value to store
    std::vector<BStmt> init_stmts;  ///< or statements that construct it
};

/// `new T`, `new T(args)`, `new T[n]`. Yields a pointer to the new object.
struct BNew {
    std::uint32_t elem_cells = 1;
    BExprPtr count;                            ///< null for a single object
    bool zero = false;                         ///< value-initialize (`new int()`)
    BExprPtr scalar_init;                      ///< `new int(5)`
    std::optional<std::uint32_t> constructor;  ///< run on each element, `this` = the element
    std::vector<BExprPtr> ctor_args;           ///< single objects only
    std::optional<std::uint32_t> record;       ///< records with a header get it written
    /// Arrays of classes: each element is constructed in a loop that uses
    /// three frame cells starting here (pointer, index, count).
    std::optional<std::uint32_t> loop_temps;
};

struct BDelete {
    BExprPtr pointer;
    bool array = false;
    std::optional<std::uint32_t> destructor;  ///< function to run before freeing
    std::uint32_t elem_cells = 1;
    bool virtual_destructor = false;
    /// delete[] of objects with a destructor: two frame cells (pointer, index).
    std::optional<std::uint32_t> loop_temps;
};

struct BExpr {
    TypeRef type = 0;
    bool lvalue = false;
    bool is_const = false;  ///< lvalues: the object may not be modified
    /// An object about to expire (a temporary, or std::move(x)): binds to
    /// `T&&` and may be moved from.
    bool expiring = false;
    SourceRange range;
    std::variant<BConst, BVar, BDeref, BAddressOf, BMember, BIndex, BLoad, BUnary, BBinary, BLogical, BAssign, BIncDec,
                 BConditional, BCall, BHostCall, BIntrinsic, BConvert, BComma, BTemp, BNew, BDelete>
        node;
};

// --- Statements -----------------------------------------------------------------------

struct BExprStmt {
    BExpr expr;
};

/// Initializes a scalar object.
struct BStore {
    BExprPtr target;
    BExprPtr value;
};

struct BCopy {
    BExprPtr target;
    BExprPtr source;
    std::uint32_t cells = 0;
};

struct BZero {
    BExprPtr target;
    std::uint32_t cells = 0;
};

/// Marks a fresh object as uninitialized (reading it is undefined behavior).
struct BUninit {
    BExprPtr target;
    std::uint32_t cells = 0;
};

/// Writes the headers (dynamic type) of every polymorphic subobject of a
/// complete object of `record` at `target`.
struct BInitHeaders {
    BExprPtr target;
    std::uint32_t record = 0;
};

struct BBlock {
    std::vector<BStmt> statements;
    std::vector<BStmt> cleanup;  ///< destructor calls run on every exit, in order
};

struct BIf {
    BExpr condition;
    BStmtPtr then_stmt;
    BStmtPtr else_stmt;
    std::vector<BStmt> condition_cleanup;  ///< destroys temporaries of the condition, on both branches
};

enum class LoopKind : std::uint8_t { While, DoWhile, For };

struct BLoop {
    LoopKind kind = LoopKind::While;
    std::optional<BExpr> condition;  ///< absent: infinite `for (;;)`
    std::optional<BExpr> update;
    BStmtPtr body;
    std::vector<BStmt> condition_cleanup;  ///< after every evaluation of the condition
    std::vector<BStmt> update_cleanup;     ///< after every update
};

struct BBreak {};
struct BContinue {};

struct BReturn {
    std::optional<BExpr> value;
    std::vector<BStmt> cleanup;  ///< temporaries, destroyed once the value is computed
};

struct BSwitch {
    BExpr condition;
    std::vector<std::pair<std::int64_t, std::uint32_t>> labels;  ///< value -> section
    std::optional<std::uint32_t> default_section;
    std::vector<std::vector<BStmt>> sections;
    std::vector<BStmt> condition_cleanup;
};

/// `throw value;`: `object` builds a heap copy of the value (the exception
/// object) and yields a pointer to it. `throw;` (rethrow) has no object.
struct BThrow {
    BExprPtr object;
    std::uint32_t thrown = 0;  ///< BoundProgram::thrown: its type and destructor
};

/// `try { body } catch (...) { ... }`. Each handler is a block whose cleanup
/// ends the handling of the exception (BEndCatch), however the block is left.
struct BTry {
    BStmtPtr body;
    std::uint32_t table = 0;  ///< BoundProgram::catch_tables
    std::vector<BStmt> handlers;
};

/// The exception being handled is destroyed (unless it was rethrown).
struct BEndCatch {};

struct BStmt {
    SourceRange range;
    /// The first statement bound from one the player wrote: CostModel::Unit::Statement charges it.
    bool counted = false;
    std::variant<BExprStmt, BStore, BCopy, BZero, BUninit, BInitHeaders, BBlock, BIf, BLoop, BBreak, BContinue, BReturn,
                 BSwitch, BThrow, BTry, BEndCatch>
        node;
};

// --- Program ----------------------------------------------------------------------------

/// How one parameter arrives: a scalar value, or a pointer to a record the
/// callee copies into its frame (pass by value).
struct ParamSlot {
    std::uint32_t offset = 0;
    std::uint32_t cells = 1;
    bool copy_block = false;
};

/// A named local, for debuggers.
struct LocalDebug {
    std::string name;
    std::uint32_t offset = 0;
    TypeRef type = 0;
    bool reference = false;
    SourceRange range;  ///< where it is in scope
};

struct BoundFunction {
    std::string name;
    std::uint32_t frame_cells = 0;
    std::vector<ParamSlot> params;
    bool returns_value = false;  ///< scalar (or reference) result left on the stack
    bool is_script = false;
    bool defined = false;
    bool library = false;  ///< standard library code: no locations, hidden from debuggers
    bool throws = false;   ///< contains a throw: the program needs exception handlers
    BBlock body;
    SourceRange range;
    std::vector<LocalDebug> locals;
    /// Virtual dispatch: record index this function belongs to (complete-object vtables
    /// are emitted per record).
    std::optional<std::uint32_t> record;
};

}  // namespace cppi::sema
