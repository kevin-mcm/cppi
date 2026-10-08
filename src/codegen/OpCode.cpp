/// @file OpCode.cpp
/// @brief Implementation of OpCode.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/OpCode.hpp"

namespace cppi::detail {

std::string_view to_string(OpCode op) noexcept {
    switch (op) {
        case OpCode::PushConst: return "PUSH_CONST";
        case OpCode::Pop: return "POP";
        case OpCode::Dup: return "DUP";
        case OpCode::Swap: return "SWAP";
        case OpCode::Over: return "OVER";
        case OpCode::LocalAddr: return "LOCAL_ADDR";
        case OpCode::GlobalAddr: return "GLOBAL_ADDR";
        case OpCode::LoadLocal: return "LOAD_LOCAL";
        case OpCode::StoreLocal: return "STORE_LOCAL";
        case OpCode::LoadGlobal: return "LOAD_GLOBAL";
        case OpCode::StoreGlobal: return "STORE_GLOBAL";
        case OpCode::Load: return "LOAD";
        case OpCode::Store: return "STORE";
        case OpCode::StoreKeep: return "STORE_KEEP";
        case OpCode::Uninit: return "UNINIT";
        case OpCode::Zero: return "ZERO";
        case OpCode::Copy: return "COPY";
        case OpCode::Member: return "MEMBER";
        case OpCode::Index: return "INDEX";
        case OpCode::PtrAdd: return "PTR_ADD";
        case OpCode::PtrSub: return "PTR_SUB";
        case OpCode::PtrDiff: return "PTR_DIFF";
        case OpCode::PtrOffset: return "PTR_OFFSET";
        case OpCode::Add: return "ADD";
        case OpCode::Sub: return "SUB";
        case OpCode::Mul: return "MUL";
        case OpCode::Div: return "DIV";
        case OpCode::Mod: return "MOD";
        case OpCode::Neg: return "NEG";
        case OpCode::Shl: return "SHL";
        case OpCode::Shr: return "SHR";
        case OpCode::BitAnd: return "BIT_AND";
        case OpCode::BitOr: return "BIT_OR";
        case OpCode::BitXor: return "BIT_XOR";
        case OpCode::BitNot: return "BIT_NOT";
        case OpCode::Eq: return "EQ";
        case OpCode::Ne: return "NE";
        case OpCode::Lt: return "LT";
        case OpCode::Gt: return "GT";
        case OpCode::Le: return "LE";
        case OpCode::Ge: return "GE";
        case OpCode::Not: return "NOT";
        case OpCode::Increment: return "INC";
        case OpCode::IntToDouble: return "INT_TO_DOUBLE";
        case OpCode::DoubleToInt: return "DOUBLE_TO_INT";
        case OpCode::ToBool: return "TO_BOOL";
        case OpCode::DoubleToBool: return "DOUBLE_TO_BOOL";
        case OpCode::PtrToBool: return "PTR_TO_BOOL";
        case OpCode::Trunc32: return "TRUNC32";
        case OpCode::Trunc8: return "TRUNC8";
        case OpCode::Trunc16: return "TRUNC16";
        case OpCode::TruncU8: return "TRUNCU8";
        case OpCode::TruncU16: return "TRUNCU16";
        case OpCode::TruncU32: return "TRUNCU32";
        case OpCode::ULongToDouble: return "ULONG_TO_DOUBLE";
        case OpCode::PushHandler: return "PUSH_HANDLER";
        case OpCode::PopHandler: return "POP_HANDLER";
        case OpCode::Throw: return "THROW";
        case OpCode::Rethrow: return "RETHROW";
        case OpCode::Resume: return "RESUME";
        case OpCode::CatchDispatch: return "CATCH_DISPATCH";
        case OpCode::EndCatch: return "END_CATCH";
        case OpCode::FreeCaught: return "FREE_CAUGHT";
        case OpCode::Jump: return "JUMP";
        case OpCode::JumpIfFalse: return "JUMP_IF_FALSE";
        case OpCode::JumpIfTrue: return "JUMP_IF_TRUE";
        case OpCode::JumpCompare: return "JUMP_CMP";
        case OpCode::Call: return "CALL";
        case OpCode::CallVirtual: return "CALL_VIRTUAL";
        case OpCode::Ret: return "RET";
        case OpCode::CallHost: return "CALL_HOST";
        case OpCode::Intrinsic: return "INTRINSIC";
        case OpCode::New: return "NEW";
        case OpCode::Delete: return "DELETE";
        case OpCode::InitHeaders: return "INIT_HEADERS";
        case OpCode::BlockCount: return "BLOCK_COUNT";
        case OpCode::FlowOffEnd: return "FLOW_OFF_END";
        case OpCode::Halt: return "HALT";
    }
    return "???";
}

}  // namespace cppi::detail
