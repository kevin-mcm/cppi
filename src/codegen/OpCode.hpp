#pragma once

/// @file OpCode.hpp
/// @brief The instruction set of cppi's stack machine.
///
/// Operands are described in Instruction.hpp: `operand` (32 bits) and `wide`
/// (24 bits: reserved+argc).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>
#include <string_view>

namespace cppi::detail {

/// One operation of the stack machine. In the comments, `[a b] -> [c]` shows
/// the top of the operand stack before and after (the top is on the right).
enum class OpCode : std::uint8_t {
    // Stack
    PushConst,  ///< push constants[operand]
    Pop,        ///< [a] -> []
    Dup,        ///< [a] -> [a a]
    Swap,       ///< [a b] -> [b a]
    Over,       ///< [a b] -> [a b a]
    // Addresses and memory
    LocalAddr,    ///< push &frame[operand], an object of `wide` cells
    GlobalAddr,   ///< push &globals[operand], an object of `wide` cells
    LoadLocal,    ///< push frame[operand]
    StoreLocal,   ///< frame[operand] = pop
    LoadGlobal,   ///< push globals[operand]
    StoreGlobal,  ///< globals[operand] = pop
    Load,         ///< [ptr] -> [*ptr]
    Store,        ///< [ptr value] -> []
    StoreKeep,    ///< [ptr value] -> [value]
    Uninit,       ///< [ptr]: mark `wide` cells uninitialized
    Zero,         ///< [ptr]: zero `wide` cells
    Copy,         ///< [dst src]: copy `wide` cells
    Member,       ///< [ptr] -> [ptr + offset]; operand high bit: base subobject (keep bounds); wide = cells
    Index,        ///< [ptr i] -> [&ptr[i]] for elements of `wide` cells; operand = known bound (0: pointer bounds)
    PtrAdd,       ///< [ptr n] -> [ptr + n * wide]
    PtrSub,       ///< [ptr n] -> [ptr - n * wide]
    PtrDiff,      ///< [p q] -> [(p - q) / wide]
    PtrOffset,    ///< [ptr] -> [ptr + (int32)operand]; wide = virtual base record + 1 (0: none)
    // Arithmetic (argc: 0 int, 1 long, 2 double, 3 pointer, 4 unsigned int, 5 unsigned long)
    Add,     ///< [a b] -> [a + b]
    Sub,     ///< [a b] -> [a - b]
    Mul,     ///< [a b] -> [a * b]
    Div,     ///< [a b] -> [a / b]
    Mod,     ///< [a b] -> [a % b]
    Neg,     ///< [a] -> [-a]
    Shl,     ///< [a b] -> [a << b]
    Shr,     ///< [a b] -> [a >> b]
    BitAnd,  ///< [a b] -> [a & b]
    BitOr,   ///< [a b] -> [a | b]
    BitXor,  ///< [a b] -> [a ^ b]
    BitNot,  ///< [a] -> [~a]
    Eq,      ///< [a b] -> [a == b]
    Ne,      ///< [a b] -> [a != b]
    Lt,      ///< [a b] -> [a < b]
    Gt,      ///< [a b] -> [a > b]
    Le,      ///< [a b] -> [a <= b]
    Ge,      ///< [a b] -> [a >= b]
    Not,     ///< [a] -> [!a]
    /// ++ or -- of a variable: cell `operand` (a local, or a global if reserved bit 1)
    /// += 1, or -= 1 if reserved bit 0; argc: 0 int, 1 long (overflow is checked).
    Increment,
    // Conversions
    IntToDouble,    ///< [signed integer] -> [double]
    ULongToDouble,  ///< [unsigned long] -> [double]
    DoubleToInt,    ///< argc: 0 int, 1 long, 2 unsigned int, 3 unsigned long
    ToBool,         ///< [integer] -> [bool]
    DoubleToBool,   ///< [double] -> [bool]
    PtrToBool,      ///< [pointer] -> [bool]
    Trunc32,        ///< [integer] -> [int32], sign-extended
    Trunc8,         ///< [integer] -> [int8], sign-extended
    Trunc16,        ///< [integer] -> [int16], sign-extended
    TruncU8,        ///< [integer] -> [uint8]
    TruncU16,       ///< [integer] -> [uint16]
    TruncU32,       ///< [integer] -> [uint32]
    // Control flow
    Jump,         ///< pc = operand
    JumpIfFalse,  ///< if (!pop) pc = operand
    JumpIfTrue,   ///< if (pop) pc = operand
    /// [a b] if ((a <cmp> b) == reserved) pc = operand; argc: kind (as for Eq) | (cmp - Eq) << 4.
    /// A comparison and the conditional jump that uses it, in one instruction.
    JumpCompare,
    Call,         ///< call functions[operand] with argc stack arguments
    CallVirtual,  ///< like Call, dispatched on the first argument; reserved = slot
    Ret,          ///< argc = 1: return the top of the stack
    CallHost,     ///< call host function `operand` with `argc` arguments
    Intrinsic,    ///< native helper `operand` (sema::Intrinsic) with `argc` arguments
    // Objects
    New,          ///< operand bit 0: array (count on the stack), bit 1: zero; wide = element cells
    Delete,       ///< [ptr]; argc bit 0: array form, bit 1: free the complete object found from its header
    InitHeaders,  ///< [ptr]: write the headers of a complete object of record `operand`
    BlockCount,   ///< [ptr] -> [elements of `wide` cells in the heap block starting at ptr]
    FlowOffEnd,   ///< end of a value-returning function reached: undefined behavior
    // Exceptions
    PushHandler,    ///< exceptions unwinding through here jump to `operand` (a catch dispatch or a cleanup)
    PopHandler,     ///< leaves the innermost handler's scope normally
    Throw,          ///< [ptr]: throw the exception object at ptr, of thrown type `operand`
    Rethrow,        ///< `throw;`: the exception being handled again
    Resume,         ///< end of a cleanup pad: keep unwinding
    CatchDispatch,  ///< jump to the clause of catch table `operand` that takes the exception, or keep unwinding
    EndCatch,       ///< the handled exception's destructor runs (unless it was rethrown)
    FreeCaught,     ///< ...and its memory is freed
    Halt,           ///< end of program
};

/// Mnemonic of an opcode, as the disassembler prints it.
[[nodiscard]] std::string_view to_string(OpCode op) noexcept;

}  // namespace cppi::detail
