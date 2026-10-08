/// @file CodeGenerator.cpp
/// @brief Implementation of CodeGenerator.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/CodeGenerator.hpp"

#include "codegen/Reachability.hpp"
#include "support/Overloaded.hpp"

#include <algorithm>
#include <bit>
#include <utility>
#include <variant>

#include "support/NumberFormat.hpp"

namespace cppi::detail {

using namespace cppi::sema;

namespace {

OpCode binary_op(BinOp op) {
    switch (op) {
        case BinOp::Add: return OpCode::Add;
        case BinOp::Sub: return OpCode::Sub;
        case BinOp::Mul: return OpCode::Mul;
        case BinOp::Div: return OpCode::Div;
        case BinOp::Mod: return OpCode::Mod;
        case BinOp::Shl: return OpCode::Shl;
        case BinOp::Shr: return OpCode::Shr;
        case BinOp::BitAnd: return OpCode::BitAnd;
        case BinOp::BitOr: return OpCode::BitOr;
        case BinOp::BitXor: return OpCode::BitXor;
        case BinOp::Eq: return OpCode::Eq;
        case BinOp::Ne: return OpCode::Ne;
        case BinOp::Lt: return OpCode::Lt;
        case BinOp::Gt: return OpCode::Gt;
        case BinOp::Le: return OpCode::Le;
        case BinOp::Ge: return OpCode::Ge;
        case BinOp::PtrAdd: return OpCode::PtrAdd;
        case BinOp::PtrSub: return OpCode::PtrSub;
        case BinOp::PtrDiff: return OpCode::PtrDiff;
    }
    return OpCode::Add;
}

bool is_pointer_op(BinOp op) {
    return op == BinOp::PtrAdd || op == BinOp::PtrSub || op == BinOp::PtrDiff;
}

}  // namespace

// =============================================================================
// Program
// =============================================================================

std::shared_ptr<const ProgramData> CodeGenerator::generate(const BoundProgram& program,
                                                           std::shared_ptr<const HostRegistry> host) {
    auto data = std::make_shared<ProgramData>();
    data->host = std::move(host);
    data->global_cells = program.global_cells;
    data->functions.resize(program.functions.size());
    CodeGenerator gen(*data, program);
    gen.build_metadata();
    const std::vector<bool> reachable = Reachability::compute(program);
    for (std::uint32_t id = 0; id < program.functions.size(); ++id) {
        gen.exceptions_ = gen.exceptions_ || (reachable[id] && program.functions[id].throws);
    }
    data->strings = program.strings;
    for (const ThrownType& t : program.thrown) {
        data->thrown.push_back(ThrownMeta{t.name, t.destructor, t.message_offset});
    }
    for (const auto& table : program.catch_tables) {
        auto& out = data->catch_tables.emplace_back();
        for (const CatchClauseInfo& clause : table) {
            out.push_back(CatchClauseMeta{0, clause.catch_all, clause.matches});
        }
    }
    for (std::uint32_t id = 0; id < program.functions.size(); ++id) {
        if (program.functions[id].defined && reachable[id]) {
            gen.function(id);
        } else {
            data->functions[id].entry = FunctionMeta::kNoCode;
        }
    }
    return data;
}

void CodeGenerator::build_metadata() {
    // Types, for debuggers and value rendering.
    out_.types.resize(types_.type_count());
    for (TypeRef t = 0; t < types_.type_count(); ++t) {
        TypeMeta& m = out_.types[t];
        m.name = types_.name(t);
        m.cells = types_.cells(t);
        switch (types_.kind(t)) {
            case TypeKind::Error:
            case TypeKind::Void: m.kind = ValueKind::Void; break;
            case TypeKind::Bool: m.kind = ValueKind::Bool; break;
            case TypeKind::Char: m.kind = ValueKind::Char; break;
            case TypeKind::Int: m.kind = ValueKind::Int; break;
            case TypeKind::Long:
            case TypeKind::Short:
            case TypeKind::UChar:
            case TypeKind::UShort:
            case TypeKind::UInt: m.kind = ValueKind::Long; break;
            case TypeKind::ULong: m.kind = ValueKind::ULong; break;
            case TypeKind::Double: m.kind = ValueKind::Double; break;
            case TypeKind::Nullptr: m.kind = ValueKind::Nullptr; break;
            case TypeKind::Enum: {
                m.kind = ValueKind::Enum;
                for (const auto& [name, value] : types_.enum_info(t).enumerators) {
                    m.enumerators.push_back(name);
                    m.enum_values.push_back(value);
                }
                break;
            }
            case TypeKind::Pointer:
            case TypeKind::Reference:
                m.kind = ValueKind::Pointer;
                m.element = types_.info(t).target;
                break;
            case TypeKind::Array:
                m.kind = ValueKind::Array;
                m.element = types_.info(t).target;
                m.count = types_.info(t).count;
                break;
            case TypeKind::Record: {
                m.kind = ValueKind::Record;
                auto flatten = [&](auto&& self, std::uint32_t record, std::uint32_t base) -> void {
                    const RecordInfo& r = types_.record_at(record);
                    for (const BaseInfo& b : r.bases) {
                        if (!b.is_virtual) {
                            self(self, b.record, base + b.offset);
                        }
                    }
                    for (const FieldInfo& f : r.fields) {
                        m.fields.push_back(TypeMeta::Field{f.name, base + f.offset, f.type});
                    }
                };
                flatten(flatten, types_.info(t).decl, 0);
                for (const auto& [v, off] : types_.record(t).virtual_base_offsets) {
                    flatten(flatten, v, off);
                }
                break;
            }
        }
    }

    // Records: headers and dispatch tables.
    out_.records.resize(types_.record_count());
    for (std::uint32_t r = 0; r < types_.record_count(); ++r) {
        const RecordInfo& info = types_.record_at(r);
        RecordMeta& m = out_.records[r];
        m.name = info.name;
        m.cells = info.size;
        m.virtual_bases = info.virtual_base_offsets;
        for (const auto& sub : info.subobjects) {
            m.header_offsets.push_back(sub.offset);
            RecordMeta::Subobject s;
            s.offset = sub.offset;
            s.record = sub.record;
            for (const auto& slot : sub.slots) {
                s.slots.push_back(RecordMeta::Slot{slot.function, slot.this_offset});
            }
            m.subobjects.push_back(std::move(s));
        }
    }

    // Functions and their variables.
    for (std::uint32_t id = 0; id < program_.functions.size(); ++id) {
        const BoundFunction& fn = program_.functions[id];
        FunctionMeta& m = out_.functions[id];
        m.name = fn.name;
        m.frame_cells = fn.frame_cells;
        m.returns_value = fn.returns_value;
        m.is_pure = !fn.defined;
        m.record = fn.record;
        m.library = fn.library;
        for (const ParamSlot& p : fn.params) {
            m.params.push_back(ParamMeta{p.offset, p.cells, p.copy_block});
        }
        for (const LocalDebug& l : fn.locals) {
            m.locals.push_back(VariableMeta{l.name, l.offset, l.type, l.reference, l.range});
        }
    }
    for (const GlobalDebug& g : program_.globals) {
        out_.globals.push_back(VariableMeta{g.name, g.offset, g.type, false, {}});
    }
}

void CodeGenerator::function(std::uint32_t id) {
    const BoundFunction& fn = program_.functions[id];
    current_ = id;
    labels_.clear();
    fixups_.clear();
    cleanups_.clear();
    targets_.clear();
    depth_ = 0;
    out_.functions[id].entry = static_cast<std::uint32_t>(out_.code.size());
    end_label_ = new_label();
    shapes_.clear();

    block(fn.body);
    bind_label(end_label_);
    const SourceRange end{fn.range.end, fn.range.end};
    SourceRange last = end;
    if (!fn.body.statements.empty()) {
        const BStmt& tail = fn.body.statements.back();
        const SourceLocation at = std::holds_alternative<BExprStmt>(tail.node)
                                      ? std::get<BExprStmt>(tail.node).expr.range.end
                                      : tail.range.end;
        last = SourceRange{at, at};
    }
    if (fn.is_script) {
        emit(OpCode::Halt, last);
    } else if (fn.returns_value) {
        emit(OpCode::FlowOffEnd, end, id);
    } else {
        emit(OpCode::Ret, end, 0, 0);
    }
    patch();
    thread_jumps(out_.functions[id].entry);
}

// =============================================================================
// Statements
// =============================================================================

void CodeGenerator::statements(const std::vector<BStmt>& list) {
    for (const BStmt& s : list) {
        statement(s);
    }
}

void CodeGenerator::block(const BBlock& b) {
    const SourceRange none{};
    // With exceptions around, a scope with cleanup registers a pad that runs
    // it when an exception unwinds through, then keeps unwinding.
    const bool handler = exceptions_ && !b.cleanup.empty();
    const std::uint32_t pad = handler ? new_label() : 0;
    if (handler) {
        jump(OpCode::PushHandler, pad, none);
    }
    cleanups_.push_back(Scope{&b.cleanup, handler});
    statements(b.statements);
    cleanups_.pop_back();
    if (handler) {
        emit(OpCode::PopHandler, none);
    }
    statements(b.cleanup);
    if (handler) {
        const std::uint32_t after = new_label();
        jump(OpCode::Jump, after, none);
        bind_label(pad);
        statements(b.cleanup);
        emit(OpCode::Resume, none);
        bind_label(after);
    }
}

void CodeGenerator::emit_cleanups(std::size_t down_to) {
    // Inner scopes first. Cleanup statements never jump, so generating them
    // while the scope list is unchanged is safe.
    for (std::size_t i = cleanups_.size(); i-- > down_to;) {
        if (cleanups_[i].handler) {
            emit(OpCode::PopHandler, SourceRange{});
        }
        if (cleanups_[i].cleanup != nullptr) {
            statements(*cleanups_[i].cleanup);
        }
    }
}

void CodeGenerator::try_statement(const BTry& t, SourceRange range) {
    const std::uint32_t dispatch = new_label();
    const std::uint32_t end = new_label();
    jump(OpCode::PushHandler, dispatch, range);
    cleanups_.push_back(Scope{nullptr, true});
    statement(*t.body);
    cleanups_.pop_back();
    emit(OpCode::PopHandler, range);
    jump(OpCode::Jump, end, range);
    bind_label(dispatch);
    emit(OpCode::CatchDispatch, range, t.table);
    for (std::size_t i = 0; i < t.handlers.size(); ++i) {
        out_.catch_tables[t.table][i].pc = static_cast<std::uint32_t>(out_.code.size());
        statement(t.handlers[i]);
        jump(OpCode::Jump, end, range);
    }
    bind_label(end);
}

void CodeGenerator::statement(const BStmt& s) {
    const SourceRange range = s.range;
    const std::size_t start = out_.code.size();
    std::visit(Overloaded{
                   [&](const BExprStmt& e) { discard(e.expr); },
                   [&](const BStore& st) {
                       if (const auto* v = std::get_if<BVar>(&st.target->node); v != nullptr && !v->reference) {
                           value(*st.value);
                           emit(v->global ? OpCode::StoreGlobal : OpCode::StoreLocal, range, v->offset);
                           adjust(-1);
                           return;
                       }
                       address(*st.target);
                       value(*st.value);
                       emit(OpCode::Store, range);
                       adjust(-2);
                   },
                   [&](const BCopy& c) {
                       address(*c.target);
                       address(*c.source);
                       emit_wide(OpCode::Copy, range, 0, c.cells);
                       adjust(-2);
                   },
                   [&](const BZero& z) {
                       address(*z.target);
                       emit_wide(OpCode::Zero, range, 0, z.cells);
                       adjust(-1);
                   },
                   [&](const BUninit& u) {
                       address(*u.target);
                       emit_wide(OpCode::Uninit, range, 0, u.cells);
                       adjust(-1);
                   },
                   [&](const BInitHeaders& h) {
                       address(*h.target);
                       emit(OpCode::InitHeaders, range, h.record);
                       adjust(-1);
                   },
                   [&](const BBlock& b) { block(b); },
                   [&](const BIf& i) {
                       if (const auto* c = std::get_if<BConst>(&i.condition.node)) {
                           if (c->bits != 0) {
                               statement(*i.then_stmt);
                           } else if (i.else_stmt) {
                               statement(*i.else_stmt);
                           }
                           return;
                       }
                       const std::uint32_t else_label = new_label();
                       const std::uint32_t end = new_label();
                       const std::size_t condition = out_.code.size();
                       branch(i.condition, false, else_label, range);
                       count_from(condition);
                       statements(i.condition_cleanup);
                       statement(*i.then_stmt);
                       if (i.else_stmt || !i.condition_cleanup.empty()) {
                           jump(OpCode::Jump, end, range);
                       }
                       bind_label(else_label);
                       statements(i.condition_cleanup);
                       if (i.else_stmt) {
                           statement(*i.else_stmt);
                       }
                       bind_label(end);
                   },
                   [&](const BLoop& l) { loop(l, range); },
                   [&](const BBreak&) {
                       const BreakTarget& t = targets_.back();
                       emit_cleanups(t.scope_depth);
                       jump(OpCode::Jump, t.break_label, range);
                   },
                   [&](const BContinue&) {
                       for (auto it = targets_.rbegin(); it != targets_.rend(); ++it) {
                           if (it->continue_label) {
                               emit_cleanups(it->scope_depth);
                               jump(OpCode::Jump, *it->continue_label, range);
                               return;
                           }
                       }
                   },
                   [&](const BReturn& r) { return_statement(r, range); },
                   [&](const BSwitch& sw) { switch_statement(sw, range); },
                   [&](const BThrow& t) {
                       if (t.object) {
                           value(*t.object);
                           emit(OpCode::Throw, range, t.thrown);
                           adjust(-1);
                       } else {
                           emit(OpCode::Rethrow, range);
                       }
                   },
                   [&](const BTry& t) { try_statement(t, range); },
                   [&](const BEndCatch&) {
                       emit(OpCode::EndCatch, range);
                       emit(OpCode::FreeCaught, range);
                   },
               },
               s.node);
    if (s.counted) {
        count_from(start);
    }
}

void CodeGenerator::loop(const BLoop& l, SourceRange range) {
    const std::uint32_t top = new_label();
    const std::uint32_t next = new_label();
    const std::uint32_t exit = new_label();  // condition false: clean up its temporaries
    const std::uint32_t end = new_label();
    const bool cleanup = !l.condition_cleanup.empty();
    targets_.push_back(BreakTarget{end, next, cleanups_.size()});
    if (l.kind != LoopKind::DoWhile && !cleanup) {
        // The condition goes after the body, so each iteration ends with one
        // conditional jump back instead of a jump to a test at the top:
        // `goto check; top: body; next: update; check: if (cond) goto top;`
        const std::uint32_t check = new_label();
        if (l.condition) {
            jump(OpCode::Jump, check, l.condition->range);
        }
        bind_label(top);
        statement(*l.body);
        bind_label(next);
        if (l.update) {
            discard(*l.update);
            statements(l.update_cleanup);
        }
        bind_label(check);
        const std::size_t condition = out_.code.size();
        if (l.condition) {
            branch(*l.condition, true, top, l.condition->range);
        } else {
            jump(OpCode::Jump, top, range);  // `for (;;)`: each iteration still costs one
        }
        count_from(condition);
        bind_label(end);
        targets_.pop_back();
        return;
    }
    bind_label(top);
    if (l.kind != LoopKind::DoWhile && l.condition) {
        const std::size_t condition = out_.code.size();
        value(*l.condition);
        jump(OpCode::JumpIfFalse, cleanup ? exit : end, l.condition->range);
        count_from(condition);
        adjust(-1);
        statements(l.condition_cleanup);
    }
    statement(*l.body);
    bind_label(next);
    if (l.kind == LoopKind::DoWhile && l.condition) {
        const BExpr& condition = *l.condition;
        const std::size_t condition_start = out_.code.size();
        if (cleanup) {
            value(condition);
            const std::uint32_t again = new_label();
            jump(OpCode::JumpIfTrue, again, condition.range);
            adjust(-1);
            statements(l.condition_cleanup);
            jump(OpCode::Jump, end, range);
            bind_label(again);
            statements(l.condition_cleanup);
            jump(OpCode::Jump, top, range);
        } else {
            branch(condition, true, top, condition.range);
        }
        count_from(condition_start);
    } else {
        if (l.update) {
            discard(*l.update);
            statements(l.update_cleanup);
        }
        jump(OpCode::Jump, top, range);  // only loops with a condition get here
        if (cleanup) {
            bind_label(exit);
            statements(l.condition_cleanup);
        }
    }
    bind_label(end);
    targets_.pop_back();
}

void CodeGenerator::switch_statement(const BSwitch& s, SourceRange range) {
    std::vector<std::uint32_t> sections(s.sections.size());
    for (auto& label : sections) {
        label = new_label();
    }
    const std::uint32_t end = new_label();
    const std::size_t condition = out_.code.size();
    value(s.condition);
    count_from(condition);
    // Dispatch: compare against each label, keeping the value on the stack.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> landings;  // (landing label, section)
    for (const auto& [constant, section] : s.labels) {
        emit(OpCode::Dup, range);
        adjust(1);
        push_constant(constant, s.condition.type, range);
        emit(OpCode::Eq, range, 0, 0);
        adjust(-1);
        const std::uint32_t landing = new_label();
        jump(OpCode::JumpIfTrue, landing, range);
        adjust(-1);
        landings.emplace_back(landing, section);
    }
    emit(OpCode::Pop, range);
    adjust(-1);
    statements(s.condition_cleanup);
    jump(OpCode::Jump, s.default_section ? sections[*s.default_section] : end, range);
    for (const auto& [landing, section] : landings) {
        bind_label(landing);
        adjust(1);  // the value is still on the stack here
        emit(OpCode::Pop, range);
        adjust(-1);
        statements(s.condition_cleanup);
        jump(OpCode::Jump, sections[section], range);
    }
    targets_.push_back(BreakTarget{end, std::nullopt, cleanups_.size()});
    for (std::size_t i = 0; i < s.sections.size(); ++i) {
        bind_label(sections[i]);
        statements(s.sections[i]);
    }
    targets_.pop_back();
    bind_label(end);
}

void CodeGenerator::return_statement(const BReturn& r, SourceRange range) {
    const BoundFunction& fn = program_.functions[current_];
    if (fn.is_script) {
        statements(r.cleanup);
        emit_cleanups(1);  // block scopes; the script's own cleanup runs at its end
        jump(OpCode::Jump, end_label_, range);
        return;
    }
    if (r.value) {
        value(*r.value);
    }
    statements(r.cleanup);
    emit_cleanups(0);
    emit(OpCode::Ret, range, 0, r.value ? 1 : 0);
    if (r.value) {
        adjust(-1);
    }
}

// =============================================================================
// Expressions
// =============================================================================

void CodeGenerator::discard(const BExpr& e) {
    if (const auto* a = std::get_if<BAssign>(&e.node)) {
        assign(*a, e, Mode::Discard);
        return;
    }
    if (const auto* i = std::get_if<BIncDec>(&e.node)) {
        inc_dec(*i, e, Mode::Discard);
        return;
    }
    if (const auto* c = std::get_if<BComma>(&e.node)) {
        discard(*c->lhs);
        discard(*c->rhs);
        return;
    }
    if (const auto* c = std::get_if<BCall>(&e.node)) {
        call(*c, e);
        if (program_.functions[c->function].returns_value) {
            emit(OpCode::Pop, e.range);
            adjust(-1);
        }
        return;
    }
    if (const auto* d = std::get_if<BDeref>(&e.node); d != nullptr && std::holds_alternative<BCall>(d->pointer->node)) {
        discard(*d->pointer);
        return;
    }
    if (const auto* t = std::get_if<BTemp>(&e.node)) {
        if (t->init) {
            discard(*t->init);
        }
        statements(t->init_stmts);
        return;
    }
    if (types_.is_void(e.type) && !e.lvalue) {
        value(e);
        return;
    }
    if (e.lvalue) {
        address(e);
        emit(OpCode::Pop, e.range);
        adjust(-1);
        return;
    }
    value(e);
    emit(OpCode::Pop, e.range);
    adjust(-1);
}

void CodeGenerator::value(const BExpr& e) {
    const SourceRange range = e.range;
    if (e.lvalue) {
        // Scalars reach here only through BLoad; this covers records used as values.
        address(e);
        return;
    }
    if (fold(e)) {
        return;
    }
    std::visit(Overloaded{
                   [&](const BConst& c) { push_constant(c.bits, e.type, range); },
                   [&](const BLoad& l) {
                       const BExpr& lv = *l.lvalue;
                       if (const auto* v = std::get_if<BVar>(&lv.node); v != nullptr && !v->reference) {
                           emit(v->global ? OpCode::LoadGlobal : OpCode::LoadLocal, range, v->offset);
                           adjust(1);
                           return;
                       }
                       if (const auto* a = std::get_if<BAssign>(&lv.node); a != nullptr && a->record_cells == 0) {
                           assign(*a, lv, Mode::Value);
                           return;
                       }
                       address(lv);
                       emit(OpCode::Load, range);
                   },
                   [&](const BAddressOf& a) { address(*a.lvalue); },
                   [&](const BUnary& u) {
                       value(*u.operand);
                       switch (u.op) {
                           case UnOp::Neg: emit(OpCode::Neg, range, 0, kind_of(u.kind)); break;
                           case UnOp::BitNot: emit(OpCode::BitNot, range, 0, kind_of(u.kind)); break;
                           case UnOp::Not: emit(OpCode::Not, range); break;
                       }
                   },
                   [&](const BBinary& b) {
                       value(*b.lhs);
                       value(*b.rhs);
                       if (is_pointer_op(b.op)) {
                           emit_wide(binary_op(b.op), range, 0, b.elem_cells);
                       } else {
                           emit(binary_op(b.op), range, 0, kind_of(b.kind));
                       }
                       adjust(-1);
                   },
                   [&](const BLogical& l) {
                       const std::uint32_t short_circuit = new_label();
                       const std::uint32_t end = new_label();
                       value(*l.lhs);
                       jump(l.is_and ? OpCode::JumpIfFalse : OpCode::JumpIfTrue, short_circuit, range);
                       adjust(-1);
                       value(*l.rhs);
                       jump(OpCode::Jump, end, range);
                       adjust(-1);
                       bind_label(short_circuit);
                       push_constant(l.is_and ? 0 : 1, TypeTable::kBool, range);
                       bind_label(end);
                   },
                   [&](const BAssign& a) { assign(a, e, Mode::Value); },
                   [&](const BIncDec& i) { inc_dec(i, e, Mode::Value); },
                   [&](const BConditional& c) {
                       const std::uint32_t else_label = new_label();
                       const std::uint32_t end = new_label();
                       branch(*c.condition, false, else_label, range);
                       value(*c.then_expr);
                       jump(OpCode::Jump, end, range);
                       adjust(-1);
                       bind_label(else_label);
                       value(*c.else_expr);
                       bind_label(end);
                   },
                   [&](const BCall& c) { call(c, e); },
                   [&](const BHostCall& h) {
                       for (const auto& a : h.args) {
                           value(*a);
                       }
                       emit(OpCode::CallHost, range, h.function.value, static_cast<std::uint16_t>(h.args.size()));
                       adjust(-static_cast<long>(h.args.size()));
                       if (!types_.is_void(e.type)) {
                           adjust(1);
                       }
                   },
                   [&](const BIntrinsic& in) {
                       for (const auto& a : in.args) {
                           value(*a);
                       }
                       emit(OpCode::Intrinsic, range, in.which, static_cast<std::uint16_t>(in.args.size()));
                       adjust(-static_cast<long>(in.args.size()));
                       if (!types_.is_void(e.type)) {
                           adjust(1);
                       }
                   },
                   [&](const BConvert& c) {
                       if (c.conv == Conv::ArrayDecay) {
                           address(*c.operand);
                           return;
                       }
                       value(*c.operand);
                       if (c.conv == Conv::PtrOffset) {
                           emit_wide(OpCode::PtrOffset, range,
                                     static_cast<std::uint32_t>(static_cast<std::int32_t>(c.offset)),
                                     c.virtual_base ? *c.virtual_base + 1 : 0);
                           return;
                       }
                       conversion(c.conv, range);
                   },
                   [&](const BComma& c) {
                       discard(*c.lhs);
                       value(*c.rhs);
                   },
                   [&](const BNew& n) { new_object(n, e); },
                   [&](const BDelete& d) { delete_object(d, e); },
                   [&](const auto&) { address(e); },
               },
               e.node);
}

void CodeGenerator::branch(const BExpr& e, bool when, std::uint32_t label, SourceRange range) {
    if (const auto* l = std::get_if<BLogical>(&e.node)) {
        // `a && b` is false as soon as `a` is; `a || b` true as soon as `a` is.
        if (l->is_and != when) {
            branch(*l->lhs, when, label, e.range);
            branch(*l->rhs, when, label, e.range);
        } else {
            const std::uint32_t skip = new_label();
            branch(*l->lhs, !when, skip, e.range);
            branch(*l->rhs, when, label, e.range);
            bind_label(skip);
        }
        return;
    }
    if (const auto* u = std::get_if<BUnary>(&e.node); u != nullptr && u->op == UnOp::Not) {
        branch(*u->operand, !when, label, range);
        return;
    }
    if (const auto* c = std::get_if<BConst>(&e.node)) {
        if ((c->bits != 0) == when) {
            jump(OpCode::Jump, label, range);
        }
        return;
    }
    if (const auto bits = folded(e)) {
        if ((*bits != 0) == when) {
            jump(OpCode::Jump, label, range);
        }
        return;
    }
    if (const auto* b = std::get_if<BBinary>(&e.node)) {
        const OpCode op = binary_op(b->op);
        if (op >= OpCode::Eq && op <= OpCode::Ge) {
            // Compare and jump in one instruction.
            value(*b->lhs);
            value(*b->rhs);
            const auto cmp = static_cast<unsigned>(op) - static_cast<unsigned>(OpCode::Eq);
            const std::uint32_t at =
                emit(OpCode::JumpCompare, range, 0, static_cast<std::uint16_t>(kind_of(b->kind) | (cmp << 4U)));
            out_.code[at].reserved = when ? 1 : 0;
            fixups_.emplace_back(at, label);
            adjust(-2);
            return;
        }
    }
    value(e);
    jump(when ? OpCode::JumpIfTrue : OpCode::JumpIfFalse, label, range);
    adjust(-1);
}

bool CodeGenerator::fold(const BExpr& e) {
    const auto bits = folded(e);
    if (bits) {
        push_constant(*bits, e.type, e.range);
    }
    return bits.has_value();
}

std::optional<std::int64_t> CodeGenerator::folded(const BExpr& e) const {
    if (std::holds_alternative<BConst>(e.node) || e.range.begin.line != e.range.end.line || !types_.is_scalar(e.type) ||
        types_.kind(e.type) == TypeKind::Pointer || types_.kind(e.type) == TypeKind::Nullptr || !constant_shape(e)) {
        return std::nullopt;
    }
    const auto v = evaluator_.eval(e);
    if (!v) {
        return std::nullopt;
    }
    return v->is_double ? std::bit_cast<std::int64_t>(v->d) : v->i;
}

bool CodeGenerator::constant_shape(const BExpr& e) const {
    if (const auto it = shapes_.find(&e); it != shapes_.end()) {
        return it->second;
    }
    const bool shape = std::visit(
        Overloaded{
            [](const BConst&) { return true; },
            [](const BLoad& l) {
                const auto* v = std::get_if<BVar>(&l.lvalue->node);
                return v != nullptr && v->constant.has_value();
            },
            [&](const BUnary& u) { return constant_shape(*u.operand); },
            [&](const BBinary& b) { return constant_shape(*b.lhs) && constant_shape(*b.rhs); },
            [&](const BLogical& l) { return constant_shape(*l.lhs) && constant_shape(*l.rhs); },
            [&](const BConditional& c) {
                return constant_shape(*c.condition) && constant_shape(*c.then_expr) && constant_shape(*c.else_expr);
            },
            [&](const BConvert& c) { return c.conv != Conv::PtrOffset && constant_shape(*c.operand); },
            [](const auto&) { return false; },
        },
        e.node);
    shapes_.emplace(&e, shape);
    return shape;
}

void CodeGenerator::load_variable(const BVar& v, SourceRange range) {
    emit(v.global ? OpCode::LoadGlobal : OpCode::LoadLocal, range, v.offset);
    adjust(1);
}

void CodeGenerator::store_variable(const BVar& v, SourceRange range) {
    emit(v.global ? OpCode::StoreGlobal : OpCode::StoreLocal, range, v.offset);
    adjust(-1);
}

void CodeGenerator::address(const BExpr& e) {
    const SourceRange range = e.range;
    std::visit(Overloaded{
                   [&](const BVar& v) {
                       if (v.reference) {
                           emit(v.global ? OpCode::LoadGlobal : OpCode::LoadLocal, range, v.offset);
                       } else {
                           emit_wide(v.global ? OpCode::GlobalAddr : OpCode::LocalAddr, range, v.offset, v.cells);
                       }
                       adjust(1);
                   },
                   [&](const BDeref& d) { value(*d.pointer); },
                   [&](const BMember& m) {
                       address(*m.base);
                       // Base subobjects keep the bounds of the whole object (high bit).
                       emit_wide(OpCode::Member, range, m.offset | (m.is_base ? 0x80000000U : 0U), m.cells);
                   },
                   [&](const BIndex& i) {
                       if (i.base_is_pointer) {
                           value(*i.base);
                       } else {
                           address(*i.base);
                       }
                       value(*i.index);
                       emit_wide(OpCode::Index, range, i.bound, i.elem_cells);
                       adjust(-1);
                   },
                   [&](const BAssign& a) { assign(a, e, Mode::Address); },
                   [&](const BIncDec& i) { inc_dec(i, e, Mode::Address); },
                   [&](const BConditional& c) {
                       const std::uint32_t else_label = new_label();
                       const std::uint32_t end = new_label();
                       value(*c.condition);
                       jump(OpCode::JumpIfFalse, else_label, range);
                       adjust(-1);
                       address(*c.then_expr);
                       jump(OpCode::Jump, end, range);
                       adjust(-1);
                       bind_label(else_label);
                       address(*c.else_expr);
                       bind_label(end);
                   },
                   [&](const BComma& c) {
                       discard(*c.lhs);
                       address(*c.rhs);
                   },
                   [&](const BCall& c) {
                       call(c, e);
                       if (c.result_target) {
                           address(*c.result_target);
                       } else if (c.result_temp) {
                           emit_wide(OpCode::LocalAddr, range, *c.result_temp, types_.cells(e.type));
                           adjust(1);
                       }
                   },
                   [&](const BTemp& t) {
                       if (t.init) {
                           emit_wide(OpCode::LocalAddr, range, t.offset, t.cells);
                           adjust(1);
                           emit(OpCode::Dup, range);
                           adjust(1);
                           value(*t.init);
                           emit(OpCode::Store, range);
                           adjust(-2);
                           return;
                       }
                       statements(t.init_stmts);
                       emit_wide(OpCode::LocalAddr, range, t.offset, t.cells);
                       adjust(1);
                   },
                   [&](const auto&) { value(e); },
               },
               e.node);
}

void CodeGenerator::assign(const BAssign& a, const BExpr& e, Mode mode) {
    const SourceRange range = e.range;
    if (a.record_cells > 0) {
        address(*a.target);
        if (mode == Mode::Address) {
            emit(OpCode::Dup, range);
            adjust(1);
        }
        address(*a.value);
        emit_wide(OpCode::Copy, range, 0, a.record_cells);
        adjust(-2);
        return;
    }
    if (!a.op) {
        const auto* v = std::get_if<BVar>(&a.target->node);
        if (v != nullptr && !v->reference && mode != Mode::Address) {
            value(*a.value);
            if (mode == Mode::Value) {
                emit(OpCode::Dup, range);
                adjust(1);
            }
            emit(v->global ? OpCode::StoreGlobal : OpCode::StoreLocal, range, v->offset);
            adjust(-1);
            return;
        }
        address(*a.target);
        if (mode == Mode::Address) {
            emit(OpCode::Dup, range);
            adjust(1);
        }
        value(*a.value);
        emit(mode == Mode::Value ? OpCode::StoreKeep : OpCode::Store, range);
        adjust(mode == Mode::Value ? -1 : -2);
        return;
    }
    // Compound: read, compute, convert back, write. A plain variable is read
    // and written directly; anything else through its address, kept below.
    const auto* v = std::get_if<BVar>(&a.target->node);
    const bool direct = v != nullptr && !v->reference && mode != Mode::Address;
    if (direct) {
        load_variable(*v, range);
    } else {
        address(*a.target);
        if (mode == Mode::Address) {
            emit(OpCode::Dup, range);
            adjust(1);
        }
        emit(OpCode::Dup, range);
        adjust(1);
        emit(OpCode::Load, range);
    }
    conversions(a.load_conv, range);
    value(*a.value);
    if (is_pointer_op(*a.op)) {
        emit_wide(binary_op(*a.op), range, 0, a.elem_cells);
    } else {
        emit(binary_op(*a.op), range, 0, kind_of(a.kind));
    }
    adjust(-1);
    conversions(a.store_conv, range);
    if (direct) {
        if (mode == Mode::Value) {
            emit(OpCode::Dup, range);
            adjust(1);
        }
        store_variable(*v, range);
        return;
    }
    emit(mode == Mode::Value ? OpCode::StoreKeep : OpCode::Store, range);
    adjust(mode == Mode::Value ? -1 : -2);
}

void CodeGenerator::inc_dec(const BIncDec& i, const BExpr& e, Mode mode) {
    const SourceRange range = e.range;
    const bool pointer = i.kind == ScalarKind::Pointer;
    auto step = [&]() {
        if (pointer) {
            push_constant(1, TypeTable::kLong, range);
            emit_wide(i.increment ? OpCode::PtrAdd : OpCode::PtrSub, range, 0, i.elem_cells);
        } else if (i.kind == ScalarKind::Double) {
            push_constant(std::bit_cast<std::int64_t>(1.0), TypeTable::kDouble, range);
            emit(i.increment ? OpCode::Add : OpCode::Sub, range, 0, kind_of(i.kind));
        } else {
            push_constant(1, i.kind == ScalarKind::Int64 ? TypeTable::kLong : TypeTable::kInt, range);
            emit(i.increment ? OpCode::Add : OpCode::Sub, range, 0, kind_of(i.kind));
        }
        adjust(-1);
        conversions(i.store_conv, range);
    };
    const auto* var = std::get_if<BVar>(&i.target->node);
    if (var != nullptr && !var->reference && mode == Mode::Discard && i.store_conv.empty() &&
        (i.kind == ScalarKind::Int32 || i.kind == ScalarKind::Int64)) {
        // `i++;` on an int or long variable: one instruction.
        const std::uint32_t at = emit(OpCode::Increment, range, var->offset, kind_of(i.kind));
        out_.code[at].reserved = static_cast<std::uint8_t>((i.increment ? 0U : 1U) | (var->global ? 2U : 0U));
        return;
    }
    if (const auto* v = var; v != nullptr && !v->reference && mode != Mode::Address) {
        // A variable is read and written directly, without its address.
        load_variable(*v, range);
        if (mode == Mode::Value && !i.prefix) {
            emit(OpCode::Dup, range);  // the old value is the result
            adjust(1);
        }
        step();
        if (mode == Mode::Value && i.prefix) {
            emit(OpCode::Dup, range);
            adjust(1);
        }
        store_variable(*v, range);
        return;
    }
    address(*i.target);
    if (mode == Mode::Value && !i.prefix) {
        // [a] -> [old] with *a = old + 1
        emit(OpCode::Dup, range);
        adjust(1);
        emit(OpCode::Load, range);
        emit(OpCode::Swap, range);
        emit(OpCode::Over, range);
        adjust(1);
        step();
        emit(OpCode::Store, range);
        adjust(-2);
        return;
    }
    if (mode == Mode::Address) {
        emit(OpCode::Dup, range);
        adjust(1);
    }
    emit(OpCode::Dup, range);
    adjust(1);
    emit(OpCode::Load, range);
    step();
    emit(mode == Mode::Value ? OpCode::StoreKeep : OpCode::Store, range);
    adjust(mode == Mode::Value ? -1 : -2);
}

void CodeGenerator::call(const BCall& c, const BExpr& e) {
    const SourceRange range = e.range;
    const BoundFunction& callee = program_.functions[c.function];
    std::size_t index = 0;
    long pushed = 0;
    const bool method = callee.record.has_value();
    auto result_slot = [&]() {
        if (c.result_target) {
            address(*c.result_target);
        } else {
            emit_wide(OpCode::LocalAddr, range, *c.result_temp, types_.cells(e.type));
            adjust(1);
        }
        ++pushed;
    };
    const bool has_result = c.result_temp.has_value() || c.result_target != nullptr;
    for (const auto& a : c.args) {
        if (has_result && index == (method ? 1U : 0U)) {
            result_slot();
        }
        value(*a);
        ++index;
        ++pushed;
    }
    if (has_result && index == (method ? 1U : 0U)) {
        result_slot();
    }
    if (c.virtual_slot) {
        const std::uint32_t at = emit(OpCode::CallVirtual, range, c.function, static_cast<std::uint16_t>(pushed));
        out_.code[at].reserved = static_cast<std::uint8_t>(*c.virtual_slot);
    } else {
        emit(OpCode::Call, range, c.function, static_cast<std::uint16_t>(pushed));
    }
    adjust(-pushed);
    if (callee.returns_value) {
        adjust(1);
    }
}

void CodeGenerator::new_object(const BNew& n, const BExpr& e) {
    const SourceRange range = e.range;
    if (n.count && n.loop_temps) {
        // new T[n] for classes: allocate, then construct every element.
        const std::uint32_t tmp_p = *n.loop_temps;
        const std::uint32_t tmp_i = tmp_p + 1;
        const std::uint32_t tmp_n = tmp_p + 2;
        value(*n.count);
        emit(OpCode::Dup, range);
        adjust(1);
        emit(OpCode::StoreLocal, range, tmp_n);
        adjust(-1);
        emit_wide(OpCode::New, range, 1U | (n.zero ? 2U : 0U), n.elem_cells);
        emit(OpCode::StoreLocal, range, tmp_p);
        adjust(-1);
        push_constant(0, TypeTable::kLong, range);
        emit(OpCode::StoreLocal, range, tmp_i);
        adjust(-1);
        const std::uint32_t top = new_label();
        const std::uint32_t done = new_label();
        bind_label(top);
        emit(OpCode::LoadLocal, range, tmp_i);
        emit(OpCode::LoadLocal, range, tmp_n);
        adjust(2);
        emit(OpCode::Lt, range, 0, 1);
        adjust(-1);
        jump(OpCode::JumpIfFalse, done, range);
        adjust(-1);
        emit(OpCode::LoadLocal, range, tmp_p);
        emit(OpCode::LoadLocal, range, tmp_i);
        adjust(2);
        emit_wide(OpCode::Index, range, 0, n.elem_cells);
        adjust(-1);
        if (n.record && !out_.records[*n.record].header_offsets.empty()) {
            emit(OpCode::Dup, range);
            adjust(1);
            emit(OpCode::InitHeaders, range, *n.record);
            adjust(-1);
        }
        if (n.constructor) {
            emit(OpCode::Call, range, *n.constructor, 1);
        } else {
            emit(OpCode::Pop, range);
        }
        adjust(-1);
        emit(OpCode::LoadLocal, range, tmp_i);
        adjust(1);
        push_constant(1, TypeTable::kLong, range);
        emit(OpCode::Add, range, 0, 1);
        adjust(-1);
        emit(OpCode::StoreLocal, range, tmp_i);
        adjust(-1);
        jump(OpCode::Jump, top, range);
        bind_label(done);
        emit(OpCode::LoadLocal, range, tmp_p);
        adjust(1);
        return;
    }
    std::uint32_t flags = 0;
    if (n.count) {
        value(*n.count);
        flags |= 1;
    } else {
        adjust(1);
    }
    if (n.zero) {
        flags |= 2;
    }
    emit_wide(OpCode::New, range, flags, n.elem_cells);
    if (n.record && !out_.records[*n.record].header_offsets.empty()) {
        emit(OpCode::Dup, range);
        adjust(1);
        emit(OpCode::InitHeaders, range, *n.record);
        adjust(-1);
    }
    if (n.scalar_init) {
        emit(OpCode::Dup, range);
        adjust(1);
        value(*n.scalar_init);
        emit(OpCode::Store, range);
        adjust(-2);
    }
    if (n.constructor) {
        emit(OpCode::Dup, range);
        adjust(1);
        for (const auto& a : n.ctor_args) {
            value(*a);
        }
        const auto argc = static_cast<std::uint16_t>(1 + n.ctor_args.size());
        emit(OpCode::Call, range, *n.constructor, argc);
        adjust(-static_cast<long>(argc));
    }
}

void CodeGenerator::delete_object(const BDelete& d, const BExpr& e) {
    const SourceRange range = e.range;
    const std::uint32_t is_null = new_label();
    const std::uint32_t end = new_label();
    value(*d.pointer);
    emit(OpCode::Dup, range);
    adjust(1);
    emit(OpCode::PtrToBool, range);
    jump(OpCode::JumpIfFalse, is_null, range);
    adjust(-1);
    std::uint16_t flags = d.array ? 1 : 0;
    if (d.destructor && d.array && d.loop_temps) {
        // delete[]: destroy the elements in reverse order, then free.
        const std::uint32_t tmp_p = *d.loop_temps;
        const std::uint32_t tmp_i = tmp_p + 1;
        emit(OpCode::StoreLocal, range, tmp_p);
        adjust(-1);
        emit(OpCode::LoadLocal, range, tmp_p);
        adjust(1);
        emit_wide(OpCode::BlockCount, range, 0, d.elem_cells);
        emit(OpCode::StoreLocal, range, tmp_i);
        adjust(-1);
        const std::uint32_t top = new_label();
        const std::uint32_t done = new_label();
        bind_label(top);
        emit(OpCode::LoadLocal, range, tmp_i);
        adjust(1);
        push_constant(0, TypeTable::kLong, range);
        emit(OpCode::Gt, range, 0, 1);
        adjust(-1);
        jump(OpCode::JumpIfFalse, done, range);
        adjust(-1);
        emit(OpCode::LoadLocal, range, tmp_i);
        adjust(1);
        push_constant(1, TypeTable::kLong, range);
        emit(OpCode::Sub, range, 0, 1);
        adjust(-1);
        emit(OpCode::StoreLocal, range, tmp_i);
        adjust(-1);
        emit(OpCode::LoadLocal, range, tmp_p);
        emit(OpCode::LoadLocal, range, tmp_i);
        adjust(2);
        emit_wide(OpCode::Index, range, 0, d.elem_cells);
        adjust(-1);
        emit(OpCode::Call, range, *d.destructor, 1);
        adjust(-1);
        jump(OpCode::Jump, top, range);
        bind_label(done);
        emit(OpCode::LoadLocal, range, tmp_p);
        adjust(1);
    } else if (d.destructor) {
        emit(OpCode::Dup, range);
        adjust(1);
        const auto owner = program_.functions[*d.destructor].record;
        std::optional<std::uint32_t> slot;
        if (d.virtual_destructor && owner) {
            const auto& own = types_.record_at(*owner).virtual_functions;
            for (std::uint32_t i = 0; i < own.size(); ++i) {
                if (own[i] == *d.destructor) {
                    slot = i;
                }
            }
        }
        if (slot) {
            const std::uint32_t at = emit(OpCode::CallVirtual, range, *d.destructor, 1);
            out_.code[at].reserved = static_cast<std::uint8_t>(*slot);
            flags |= 2;
        } else {
            emit(OpCode::Call, range, *d.destructor, 1);
        }
        adjust(-1);
    } else if (d.virtual_destructor) {
        flags |= 2;
    }
    emit(OpCode::Delete, range, 0, flags);
    adjust(-1);
    jump(OpCode::Jump, end, range);
    bind_label(is_null);
    adjust(1);
    emit(OpCode::Pop, range);
    adjust(-1);
    bind_label(end);
}

void CodeGenerator::conversions(const std::vector<Conv>& list, SourceRange range) {
    for (const Conv c : list) {
        conversion(c, range);
    }
}

void CodeGenerator::conversion(Conv conv, SourceRange range) {
    switch (conv) {
        case Conv::IntToDouble: emit(OpCode::IntToDouble, range); break;
        case Conv::DoubleToInt32: emit(OpCode::DoubleToInt, range, 0, 0); break;
        case Conv::DoubleToInt64: emit(OpCode::DoubleToInt, range, 0, 1); break;
        case Conv::IntToBool: emit(OpCode::ToBool, range); break;
        case Conv::DoubleToBool: emit(OpCode::DoubleToBool, range); break;
        case Conv::PtrToBool: emit(OpCode::PtrToBool, range); break;
        case Conv::Trunc32: emit(OpCode::Trunc32, range); break;
        case Conv::Trunc8: emit(OpCode::Trunc8, range); break;
        case Conv::Trunc16: emit(OpCode::Trunc16, range); break;
        case Conv::TruncU8: emit(OpCode::TruncU8, range); break;
        case Conv::TruncU16: emit(OpCode::TruncU16, range); break;
        case Conv::TruncU32: emit(OpCode::TruncU32, range); break;
        case Conv::DoubleToUInt32: emit(OpCode::DoubleToInt, range, 0, 2); break;
        case Conv::DoubleToUInt64: emit(OpCode::DoubleToInt, range, 0, 3); break;
        case Conv::ULongToDouble: emit(OpCode::ULongToDouble, range); break;
        case Conv::Retype:  // same bits, another type: nothing to do at runtime
        case Conv::ArrayDecay:
        case Conv::PtrOffset: break;  // decay and offset: handled by the caller
    }
}

// =============================================================================
// Emission
// =============================================================================

std::uint16_t CodeGenerator::kind_of(ScalarKind kind) const {
    switch (kind) {
        case ScalarKind::Int32: return 0;
        case ScalarKind::Int64: return 1;
        case ScalarKind::Double: return 2;
        case ScalarKind::Pointer: return 3;
        case ScalarKind::UInt32: return 4;
        case ScalarKind::UInt64: return 5;
    }
    return 0;
}

std::uint32_t CodeGenerator::emit(OpCode op, SourceRange range, std::uint32_t operand, std::uint16_t argc) {
    const auto at = static_cast<std::uint32_t>(out_.code.size());
    out_.code.push_back(Instruction{op, 0, argc, operand});
    out_.locations.push_back(program_.functions[current_].library ? SourceRange{} : range);
    out_.statement_starts.push_back(false);
    return at;
}

std::uint32_t CodeGenerator::emit_wide(OpCode op, SourceRange range, std::uint32_t operand, std::uint32_t wide) {
    Instruction ins{op, 0, 0, operand};
    ins.set_wide(wide);
    const auto at = static_cast<std::uint32_t>(out_.code.size());
    out_.code.push_back(ins);
    out_.locations.push_back(program_.functions[current_].library ? SourceRange{} : range);
    out_.statement_starts.push_back(false);
    return at;
}

std::string CodeGenerator::constant_text(std::int64_t bits, TypeRef type) const {
    switch (types_.kind(type)) {
        case TypeKind::Bool: return std::string(bits != 0 ? "true" : "false") + " : bool";
        case TypeKind::Char: return std::to_string(bits) + " : char";
        case TypeKind::ULong: return std::to_string(static_cast<std::uint64_t>(bits)) + " : unsigned long";
        case TypeKind::Double: return NumberFormat::shortest(std::bit_cast<double>(bits)) + " : double";
        case TypeKind::Enum: {
            const auto* name = types_.enum_info(type).enumerator_name(bits);
            return (name != nullptr ? *name : std::to_string(bits)) + " : " + types_.name(type);
        }
        case TypeKind::Pointer:
        case TypeKind::Nullptr: return "nullptr : " + types_.name(type);
        default: return std::to_string(bits) + " : " + types_.name(type);
    }
}

void CodeGenerator::push_constant(std::int64_t bits, TypeRef type, SourceRange range) {
    emit(OpCode::PushConst, range, constants_.intern(bits, constant_text(bits, type)));
    adjust(1);
}

void CodeGenerator::adjust(long delta) {
    depth_ += delta;
    out_.max_stack = std::max(out_.max_stack, static_cast<std::size_t>(std::max(depth_, 0L)));
}

std::uint32_t CodeGenerator::new_label() {
    labels_.push_back(-1);
    return static_cast<std::uint32_t>(labels_.size() - 1);
}

void CodeGenerator::bind_label(std::uint32_t label) {
    labels_[label] = static_cast<std::int64_t>(out_.code.size());
}

void CodeGenerator::jump(OpCode op, std::uint32_t label, SourceRange range) {
    const std::uint32_t at = emit(op, range, 0);
    fixups_.emplace_back(at, label);
}

void CodeGenerator::patch() {
    for (const auto& [at, label] : fixups_) {
        out_.code[at].operand = static_cast<std::uint32_t>(labels_[label]);
    }
}

void CodeGenerator::count_from(std::size_t start) {
    if (start < out_.code.size() && !program_.functions[current_].library && out_.locations[start].begin.line != 0) {
        out_.statement_starts[start] = true;
    }
}

void CodeGenerator::thread_jumps(std::uint32_t entry) {
    auto& code = out_.code;
    const auto& locations = out_.locations;
    const auto is_jump = [](OpCode op) {
        return op == OpCode::Jump || op == OpCode::JumpIfFalse || op == OpCode::JumpIfTrue || op == OpCode::JumpCompare;
    };
    for (std::size_t at = entry; at < code.size(); ++at) {
        if (!is_jump(code[at].op)) {
            continue;
        }
        // Only through instructions on the jump's own line: a debugger
        // stepping line by line must still stop where it used to.
        // Nor past the start of a statement, which the statement cost unit charges.
        const auto same_line = [&](std::size_t other) {
            return other < code.size() && locations[other].begin.line == locations[at].begin.line &&
                   !out_.statement_starts[other];
        };
        // Follow unconditional jumps (a bounded number: `for (;;) {}` jumps to itself).
        std::uint32_t target = code[at].operand;
        for (int hops = 0; hops < 8 && same_line(target) && code[target].op == OpCode::Jump; ++hops) {
            target = code[target].operand;
        }
        code[at].operand = target;
        // `jump end` where `end: return` returns right away (same stack: nothing runs in between).
        if (code[at].op == OpCode::Jump && same_line(target) &&
            (code[target].op == OpCode::Ret || code[target].op == OpCode::Halt)) {
            code[at] = code[target];
        }
    }
}
}  // namespace cppi::detail
