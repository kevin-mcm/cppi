#pragma once

/// @file BoundTree.hpp
/// @brief The bound tree: the program after name resolution and type checking.
///
/// Every expression knows its type and whether it denotes an object (lvalue) or
/// a value (rvalue); every conversion C++ performs implicitly is an explicit
/// node; every variable is a slot in a frame or in the globals. Code generation
/// is a straightforward walk that cannot fail.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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

/// Forward declaration: bound expressions nest.
struct BExpr;
/// Forward declaration: bound statements nest.
struct BStmt;
/// Owning pointer to a bound expression (null when absent).
using BExprPtr = std::unique_ptr<BExpr>;
/// Owning pointer to a bound statement (null when absent).
using BStmtPtr = std::unique_ptr<BStmt>;

/// How an operation computes: the VM has separate checked integer
/// arithmetic for 32 and 64 bits, IEEE doubles, and pointers.
/// How the VM computes with a value: signed integers trap on overflow,
/// unsigned ones wrap around.
enum class ScalarKind : std::uint8_t { Int32, Int64, Double, Pointer, UInt32, UInt64 };

/// Arithmetic, bitwise, comparison and pointer operators, after operand
/// conversions (`&&` and `||` are BLogical).
enum class BinOp : std::uint8_t {
    /// `+`
    Add,
    /// `-`
    Sub,
    /// `*`
    Mul,
    /// `/`
    Div,
    /// `%`
    Mod,
    /// `<<`
    Shl,
    /// `>>`
    Shr,
    /// `&`
    BitAnd,
    /// `|`
    BitOr,
    /// `^`
    BitXor,
    /// `==`
    Eq,
    /// `!=`
    Ne,
    /// `<`
    Lt,
    /// `>`
    Gt,
    /// `<=`
    Le,
    /// `>=`
    Ge,
    PtrAdd,   ///< pointer + integer
    PtrSub,   ///< pointer - integer
    PtrDiff,  ///< pointer - pointer
};

/// Unary operators: `-x`, `~x`, `!x` (`+x` and `*x`/`&x` need no operation).
enum class UnOp : std::uint8_t { Neg, BitNot, Not };

/// One implicit or explicit conversion step the VM performs on a value.
enum class Conv : std::uint8_t {
    /// signed integer -> double
    IntToDouble,
    /// double -> int (truncates; out of range is undefined behavior)
    DoubleToInt32,
    /// double -> long
    DoubleToInt64,
    /// integer -> bool
    IntToBool,
    /// double -> bool
    DoubleToBool,
    /// pointer -> bool
    PtrToBool,
    Trunc32,   ///< long -> int (wraps, implementation-defined before C++20)
    Trunc8,    ///< -> char
    Trunc16,   ///< -> short
    TruncU8,   ///< -> unsigned char (modulo 2^8)
    TruncU16,  ///< -> unsigned short
    TruncU32,  ///< -> unsigned int
    /// double -> unsigned int
    DoubleToUInt32,
    /// double -> unsigned long
    DoubleToUInt64,
    /// unsigned long -> double
    ULongToDouble,
    Retype,      ///< same bits, different type (enum -> int)
    ArrayDecay,  ///< array lvalue -> pointer to its first element
    PtrOffset,   ///< derived* -> base*: add `offset` cells (null stays null)
};

// --- Expressions ----------------------------------------------------------------------

/// A constant.
struct BConst {
    std::int64_t bits = 0;  ///< integers as is; doubles bit-cast
};

/// A named object. For references, the slot holds a pointer to the object.
struct BVar {
    /// Cell offset in the frame (or in the globals).
    std::uint32_t offset = 0;
    /// A global variable.
    bool global = false;
    /// A reference: the slot holds the referred object's address.
    bool reference = false;
    std::uint32_t cells = 1;               ///< size of the object it denotes
    std::optional<std::int64_t> constant;  ///< const variable with a known value
    std::string name;                      ///< for messages
};

/// `*pointer` as an lvalue.
struct BDeref {
    /// The pointer.
    BExprPtr pointer;
    /// Size of the pointed-to object.
    std::uint32_t cells = 1;
};

/// `&lvalue`: yields a pointer.
struct BAddressOf {
    /// The object.
    BExprPtr lvalue;
};

/// A field (or base subobject) of a record lvalue.
struct BMember {
    BExprPtr base;  ///< lvalue of the record
    /// Cells from the start of the record.
    std::uint32_t offset = 0;
    /// Size of the member.
    std::uint32_t cells = 1;
    bool is_base = false;  ///< a base subobject: pointers keep the whole object's bounds
};

/// `base[index]` as an lvalue.
struct BIndex {
    BExprPtr base;  ///< array lvalue, or pointer rvalue when base_is_pointer
    /// The index (an integer).
    BExprPtr index;
    /// Size of an element.
    std::uint32_t elem_cells = 1;
    std::uint32_t bound = 0;  ///< known array size (0 = rely on the pointer's bounds)
    /// `base` is a pointer rather than an array.
    bool base_is_pointer = false;
};

/// Reads the value of a scalar lvalue.
struct BLoad {
    /// The object read.
    BExprPtr lvalue;
};

/// A unary operation on a scalar.
struct BUnary {
    /// The operation.
    UnOp op = UnOp::Neg;
    /// How the operand is computed with.
    ScalarKind kind = ScalarKind::Int32;
    /// The operand.
    BExprPtr operand;
};

/// A binary operation on scalars (already converted to a common kind).
struct BBinary {
    /// The operation.
    BinOp op = BinOp::Add;
    ScalarKind kind = ScalarKind::Int32;  ///< kind of the operands
    /// Left operand.
    BExprPtr lhs;
    /// Right operand.
    BExprPtr rhs;
    std::uint32_t elem_cells = 1;  ///< pointer arithmetic
};

/// `lhs && rhs` or `lhs || rhs`, short-circuiting.
struct BLogical {
    /// `&&` (true) or `||` (false).
    bool is_and = true;
    /// Evaluated first.
    BExprPtr lhs;
    /// Evaluated only if needed.
    BExprPtr rhs;
};

/// `target = value`, or `target op= value` computed in `kind` and converted
/// back with `store_conv`. Yields the target (an lvalue).
struct BAssign {
    /// Compound operation; empty for plain `=`.
    std::optional<BinOp> op;
    /// How a compound assignment computes.
    ScalarKind kind = ScalarKind::Int32;
    /// The object assigned.
    BExprPtr target;
    /// The value assigned.
    BExprPtr value;
    std::vector<Conv> load_conv;     ///< compound: target's value -> kind
    std::vector<Conv> store_conv;    ///< compound: result -> target's type
    std::uint32_t record_cells = 0;  ///< non-zero: record copy assignment
    std::uint32_t elem_cells = 1;    ///< pointer += n
};

/// `++`/`--`, prefix or postfix, on a scalar lvalue.
struct BIncDec {
    /// `++` (true) or `--` (false).
    bool increment = true;
    /// Prefix: yields the object; postfix: yields the old value.
    bool prefix = true;
    /// How the target is computed with.
    ScalarKind kind = ScalarKind::Int32;
    /// The object modified.
    BExprPtr target;
    std::vector<Conv> store_conv;  ///< char targets wrap
    /// Step of a pointer target.
    std::uint32_t elem_cells = 1;
};

/// `condition ? then_expr : else_expr`.
struct BConditional {
    /// The condition (a bool).
    BExprPtr condition;
    /// Evaluated when true.
    BExprPtr then_expr;
    /// Evaluated when false.
    BExprPtr else_expr;
};

/// A call to a function of the program (player or library).
struct BCall {
    /// Callee id (BoundProgram::functions).
    std::uint32_t function = 0;
    std::vector<BExprPtr> args;                 ///< values; pointers for reference and record parameters
    std::optional<std::uint32_t> result_temp;   ///< frame slot receiving a returned record
    BExprPtr result_target;                     ///< or the object to build the result in directly (copy elision)
    std::optional<std::uint32_t> virtual_slot;  ///< dynamic dispatch through args[0] (this)
};

/// A call to a host function.
struct BHostCall {
    /// The host function.
    FunctionId function;
    /// Argument values, converted to the parameter types.
    std::vector<BExprPtr> args;
};

/// A call to a native helper of the prelude.
struct BIntrinsic {
    std::uint8_t which = 0;  ///< sema::Intrinsic
    /// Argument values.
    std::vector<BExprPtr> args;
};

/// A conversion of `operand`.
struct BConvert {
    /// The conversion.
    Conv conv = Conv::Retype;
    /// The value converted.
    BExprPtr operand;
    std::int64_t offset = 0;  ///< PtrOffset: static part
    std::uint32_t cells = 0;  ///< ArrayDecay: element cells
    /// PtrOffset through a virtual base: its record index; its position is
    /// read from the object's header at runtime, then `offset` is added.
    std::optional<std::uint32_t> virtual_base;
};

/// `lhs, rhs`: evaluates both, yields `rhs`.
struct BComma {
    /// Evaluated for its effects.
    BExprPtr lhs;
    /// The result.
    BExprPtr rhs;
};

/// A temporary object in the current frame, initialized by `init`
/// (binding `const int&` to a value, calls returning records).
struct BTemp {
    /// First frame cell of the temporary.
    std::uint32_t offset = 0;
    /// Its size.
    std::uint32_t cells = 1;
    BExprPtr init;                  ///< scalar value to store
    std::vector<BStmt> init_stmts;  ///< or statements that construct it
};

/// `new T`, `new T(args)`, `new T[n]`. Yields a pointer to the new object.
struct BNew {
    /// Size of one element.
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

/// `delete p` or `delete[] p`: runs destructors, then frees.
struct BDelete {
    /// The pointer freed.
    BExprPtr pointer;
    /// `delete[]`.
    bool array = false;
    std::optional<std::uint32_t> destructor;  ///< function to run before freeing
    /// Size of one element.
    std::uint32_t elem_cells = 1;
    /// The destructor is virtual: dispatched on the dynamic type.
    bool virtual_destructor = false;
    /// delete[] of objects with a destructor: two frame cells (pointer, index).
    std::optional<std::uint32_t> loop_temps;
};

/// A bound expression: its type, value category and node.
struct BExpr {
    /// The expression's type.
    TypeRef type = 0;
    /// Denotes an object (its address can be taken) rather than a value.
    bool lvalue = false;
    bool is_const = false;  ///< lvalues: the object may not be modified
    /// An object about to expire (a temporary, or std::move(x)): binds to
    /// `T&&` and may be moved from.
    bool expiring = false;
    /// Where it is in the source.
    SourceRange range;
    /// The node itself.
    std::variant<BConst, BVar, BDeref, BAddressOf, BMember, BIndex, BLoad, BUnary, BBinary, BLogical, BAssign, BIncDec,
                 BConditional, BCall, BHostCall, BIntrinsic, BConvert, BComma, BTemp, BNew, BDelete>
        node;
};

// --- Statements -----------------------------------------------------------------------

/// An expression evaluated for its effects.
struct BExprStmt {
    /// The expression.
    BExpr expr;
};

/// Initializes a scalar object.
struct BStore {
    /// The object initialized.
    BExprPtr target;
    /// Its value.
    BExprPtr value;
};

/// Copies `cells` cells from one object to another (trivial copies).
struct BCopy {
    /// Destination object.
    BExprPtr target;
    /// Source object.
    BExprPtr source;
    /// Size copied.
    std::uint32_t cells = 0;
};

/// Zero-initializes an object.
struct BZero {
    /// The object.
    BExprPtr target;
    /// Its size.
    std::uint32_t cells = 0;
};

/// Marks a fresh object as uninitialized (reading it is undefined behavior).
struct BUninit {
    /// The object.
    BExprPtr target;
    /// Its size.
    std::uint32_t cells = 0;
};

/// Writes the headers (dynamic type) of every polymorphic subobject of a
/// complete object of `record` at `target`.
struct BInitHeaders {
    /// The complete object.
    BExprPtr target;
    /// Its class.
    std::uint32_t record = 0;
};

/// A sequence of statements with the destructors to run when it is left.
struct BBlock {
    /// The statements.
    std::vector<BStmt> statements;
    std::vector<BStmt> cleanup;  ///< destructor calls run on every exit, in order
};

/// `if (condition) then_stmt else else_stmt`.
struct BIf {
    /// The condition (a bool).
    BExpr condition;
    /// Runs when true.
    BStmtPtr then_stmt;
    /// Runs when false; may be null.
    BStmtPtr else_stmt;
    std::vector<BStmt> condition_cleanup;  ///< destroys temporaries of the condition, on both branches
};

/// The loop forms: the condition is checked before (While, For) or after
/// (DoWhile) the body.
enum class LoopKind : std::uint8_t { While, DoWhile, For };

/// Any loop. Range `for` is lowered to a For.
struct BLoop {
    /// The loop form.
    LoopKind kind = LoopKind::While;
    std::optional<BExpr> condition;  ///< absent: infinite `for (;;)`
    /// `for` update; may be absent.
    std::optional<BExpr> update;
    /// The loop body.
    BStmtPtr body;
    std::vector<BStmt> condition_cleanup;  ///< after every evaluation of the condition
    std::vector<BStmt> update_cleanup;     ///< after every update
};

/// `break;`
struct BBreak {};
/// `continue;`
struct BContinue {};

/// `return;` or `return value;`.
struct BReturn {
    /// The returned value (scalars and references); absent otherwise.
    std::optional<BExpr> value;
    std::vector<BStmt> cleanup;  ///< temporaries, destroyed once the value is computed
};

/// `switch`: the condition selects a section through `labels`; execution
/// falls through from section to section.
struct BSwitch {
    /// The value switched on.
    BExpr condition;
    std::vector<std::pair<std::int64_t, std::uint32_t>> labels;  ///< value -> section
    /// Section of `default:`, if any.
    std::optional<std::uint32_t> default_section;
    /// Statements after each label, in source order.
    std::vector<std::vector<BStmt>> sections;
    /// Destroys temporaries of the condition.
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
    /// The protected block.
    BStmtPtr body;
    std::uint32_t table = 0;  ///< BoundProgram::catch_tables
    /// One block per catch clause, in order.
    std::vector<BStmt> handlers;
};

/// The exception being handled is destroyed (unless it was rethrown).
struct BEndCatch {};

/// A bound statement: its source range and node.
struct BStmt {
    /// Where it is in the source.
    SourceRange range;
    /// The first statement bound from one the player wrote: CostModel::Unit::Statement charges it.
    bool counted = false;
    /// The node itself.
    std::variant<BExprStmt, BStore, BCopy, BZero, BUninit, BInitHeaders, BBlock, BIf, BLoop, BBreak, BContinue, BReturn,
                 BSwitch, BThrow, BTry, BEndCatch>
        node;
};

// --- Program ----------------------------------------------------------------------------

/// How one parameter arrives: a scalar value, or a pointer to a record the
/// callee copies into its frame (pass by value).
struct ParamSlot {
    /// First frame cell of the parameter.
    std::uint32_t offset = 0;
    /// Its size.
    std::uint32_t cells = 1;
    /// Arrives as a pointer to a record to copy.
    bool copy_block = false;
};

/// A named local, for debuggers.
struct LocalDebug {
    /// Variable name.
    std::string name;
    /// First frame cell.
    std::uint32_t offset = 0;
    /// Its type.
    TypeRef type = 0;
    /// A reference: the slot holds an address.
    bool reference = false;
    SourceRange range;  ///< where it is in scope
};

/// A function ready for code generation.
struct BoundFunction {
    /// Name, for listings and call stacks.
    std::string name;
    /// Frame size: parameters, locals and temporaries.
    std::uint32_t frame_cells = 0;
    /// Parameters, in order.
    std::vector<ParamSlot> params;
    bool returns_value = false;  ///< scalar (or reference) result left on the stack
    /// The top-level statements of the program.
    bool is_script = false;
    /// Has a body (only defined functions can be called).
    bool defined = false;
    bool library = false;  ///< standard library code: no locations, hidden from debuggers
    bool throws = false;   ///< contains a throw: the program needs exception handlers
    /// The body.
    BBlock body;
    /// Where it is defined.
    SourceRange range;
    /// Parameters and locals, for debuggers.
    std::vector<LocalDebug> locals;
    /// Virtual dispatch: record index this function belongs to (complete-object vtables
    /// are emitted per record).
    std::optional<std::uint32_t> record;
};

}  // namespace cppi::sema
