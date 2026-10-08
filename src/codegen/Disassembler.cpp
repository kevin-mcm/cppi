/// @file Disassembler.cpp
/// @brief Implementation of Disassembler.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/Disassembler.hpp"

namespace cppi::detail {

namespace {

const char* kind_name(std::uint16_t kind) {
    switch (kind) {
        case 1: return "long";
        case 2: return "double";
        case 3: return "pointer";
        default: return "int";
    }
}

}  // namespace

std::string Disassembler::listing() const {
    std::string out;
    for (std::size_t i = 0; i < program_.code.size(); ++i) {
        for (std::size_t f = 1; f < program_.functions.size(); ++f) {
            if (program_.functions[f].entry == i && !program_.functions[f].name.empty()) {
                out += "      ; " + program_.functions[f].name + "\n";
            }
        }
        const auto& ins = program_.code[i];
        const auto& loc = program_.locations[i].begin;
        // "0003     2:6    PUSH_CONST  East : Direction"
        out += pad_left(std::to_string(i), 4, '0') + "  " + pad_left(std::to_string(loc.line), 4, ' ') + ":" +
               pad_right(std::to_string(loc.column), 3) + "  " + pad_right(std::string(to_string(ins.op)), 12) +
               operand_text(ins);
        while (!out.empty() && out.back() == ' ') {
            out.pop_back();
        }
        out += '\n';
    }
    return out;
}

std::string Disassembler::operand_text(const Instruction& ins) const {
    const auto& host = *program_.host;
    switch (ins.op) {
        case OpCode::PushConst: return program_.constants[ins.operand].text;
        case OpCode::CallHost:
            return host.function(FunctionId{ins.operand}).name + " (" + std::to_string(ins.argc) + " args)";
        case OpCode::Call:
        case OpCode::CallVirtual:
            return program_.functions[ins.operand].name + " (" + std::to_string(ins.argc) + " args)" +
                   (ins.op == OpCode::CallVirtual ? " slot " + std::to_string(ins.reserved) : "");
        case OpCode::LocalAddr:
        case OpCode::GlobalAddr: return "[" + std::to_string(ins.operand) + "] x" + std::to_string(ins.wide());
        case OpCode::LoadLocal:
        case OpCode::StoreLocal:
        case OpCode::LoadGlobal:
        case OpCode::StoreGlobal: return "[" + std::to_string(ins.operand) + "]";
        case OpCode::Increment:
            return std::string((ins.reserved & 2U) != 0 ? "global" : "") + "[" + std::to_string(ins.operand) + "] " +
                   ((ins.reserved & 1U) != 0 ? "-1 " : "+1 ") + kind_name(ins.argc);
        case OpCode::Member: return "+" + std::to_string(ins.operand & 0x7FFFFFFFU);
        case OpCode::Index:
            return ins.operand == 0 ? "x" + std::to_string(ins.wide())
                                    : "x" + std::to_string(ins.wide()) + " of " + std::to_string(ins.operand);
        case OpCode::Uninit:
        case OpCode::Zero:
        case OpCode::Copy:
        case OpCode::PtrAdd:
        case OpCode::PtrSub:
        case OpCode::PtrDiff: return std::to_string(ins.wide());
        case OpCode::Add:
        case OpCode::Sub:
        case OpCode::Mul:
        case OpCode::Div:
        case OpCode::Mod:
        case OpCode::Neg:
        case OpCode::Eq:
        case OpCode::Ne:
        case OpCode::Lt:
        case OpCode::Gt:
        case OpCode::Le:
        case OpCode::Ge: return kind_name(ins.argc);
        case OpCode::Jump:
        case OpCode::JumpIfFalse:
        case OpCode::JumpIfTrue: return "-> " + pad_left(std::to_string(ins.operand), 4, '0');
        case OpCode::JumpCompare: {
            const auto cmp = static_cast<OpCode>(static_cast<unsigned>(OpCode::Eq) + (ins.argc >> 4U));
            return std::string(ins.reserved != 0 ? "if " : "if !") + std::string(to_string(cmp)) + " " +
                   kind_name(static_cast<std::uint16_t>(ins.argc & 0xFU)) + " -> " +
                   pad_left(std::to_string(ins.operand), 4, '0');
        }
        case OpCode::InitHeaders:
            return ins.operand < program_.records.size() ? program_.records[ins.operand].name : std::string();
        default: return {};
    }
}

std::string Disassembler::pad_left(std::string text, std::size_t width, char fill) {
    if (text.size() < width) {
        text.insert(0, width - text.size(), fill);
    }
    return text;
}

std::string Disassembler::pad_right(std::string text, std::size_t width) {
    if (text.size() < width) {
        text.append(width - text.size(), ' ');
    }
    return text;
}

}  // namespace cppi::detail
