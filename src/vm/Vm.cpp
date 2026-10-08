#include "vm/Vm.hpp"

#include "sema/Intrinsics.hpp"
#include "support/CheckedArithmetic.hpp"
#include "support/DiagnosticFactory.hpp"
#include "support/NumberFormat.hpp"

#include <cppi/ExecutionObserver.hpp>
#include <cppi/HostCall.hpp>

#include <algorithm>
#include <bit>
#include <limits>
#include <span>
#include <utility>

namespace cppi::detail {

namespace {

constexpr std::int64_t kInt32Min = std::numeric_limits<std::int32_t>::min();
constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();

double as_double(std::int64_t bits) {
    return std::bit_cast<double>(bits);
}
std::int64_t from_double(double d) {
    return std::bit_cast<std::int64_t>(d);
}

}  // namespace

Vm::Vm(std::shared_ptr<const ProgramData> program, const RunOptions& options)
    : program_(std::move(program)),
      code_(program_->code.data()),
      options_(options),
      budget_(options.budget),
      invoker_(*program_->host),
      memory_(program_->global_cells),
      pc_(program_->functions.front().entry) {
    stack_.reserve(program_->max_stack + 8);
    // String literals live in static storage from the start.
    for (const auto& [offset, text] : program_->strings) {
        const std::uint32_t address = memory_.globals_address(offset);
        for (std::size_t i = 0; i <= text.size(); ++i) {
            memory_.cell(address + static_cast<std::uint32_t>(i)) =
                i < text.size() ? static_cast<signed char>(text[i]) : 0;
            memory_.set_initialized(address + static_cast<std::uint32_t>(i), true);
        }
    }
    const auto base = memory_.push_frame(program_->functions.front().frame_cells);
    frames_.push_back(Frame{0, base.value_or(Memory::kStackBase), 0});
}

SourceRange Vm::current_location() const noexcept {
    if (program_->locations.empty()) {
        return {};
    }
    return here();
}

SourceRange Vm::here() const noexcept {
    return located(pc_);
}

SourceRange Vm::located(std::size_t pc) const noexcept {
    const auto& locations = program_->locations;
    const std::size_t at = std::min(pc, locations.size() - 1);
    if (locations[at].begin.line != 0 || frames_.empty()) {
        return locations[at];
    }
    // Library code and the member functions the compiler writes (implicit
    // copies, assignments and default constructors) have no locations: report
    // the innermost call from a line of player code.
    for (std::size_t i = frames_.size(); i-- > 1;) {
        if (!program_->functions[frames_[i - 1].function].library) {
            const std::size_t call = std::min<std::size_t>(frames_[i].return_pc - 1, locations.size() - 1);
            if (locations[call].begin.line != 0) {
                return locations[call];
            }
        }
    }
    return locations[at];
}

void Vm::fail(Diagnostic diagnostic) {
    switch (diagnostic.code) {
        case DiagCode::BudgetExhausted: status_ = RunStatus::BudgetExhausted; break;
        case DiagCode::HostError: status_ = RunStatus::HostError; break;
        default: status_ = RunStatus::RuntimeError; break;
    }
    diagnostics_.push_back(std::move(diagnostic));
}

VariableInspector Vm::inspector() const {
    std::vector<VariableInspector::FrameView> views;
    for (std::size_t i = frames_.size(); i-- > 0;) {
        const Frame& f = frames_[i];
        if (program_->functions[f.function].library) {
            continue;  // debuggers show the player's code only
        }
        // The innermost frame is where execution is; the others are at their calls.
        const std::size_t at = i + 1 == frames_.size() ? pc_ : frames_[i + 1].return_pc - 1;
        views.push_back(VariableInspector::FrameView{
            f.function, f.base, program_->locations[std::min(at, program_->locations.size() - 1)]});
    }
    return {*program_, memory_, std::move(views)};
}

std::optional<std::string> Vm::variable_at(std::uint32_t address) const {
    return inspector().describe(address);
}

std::vector<StackFrame> Vm::call_stack() const {
    return inspector().call_stack();
}

std::vector<Variable> Vm::globals() const {
    return inspector().globals();
}

void Vm::fault(Memory::Fault f, std::int64_t pointer) {
    const SourceRange at = here();
    switch (f) {
        case Memory::Fault::None: return;
        case Memory::Fault::Null: fail(DiagnosticFactory::undefined_behavior(DiagCode::NullDereference, at)); return;
        case Memory::Fault::OutOfBounds:
        case Memory::Fault::Dangling: fail(DiagnosticFactory::undefined_behavior(DiagCode::InvalidPointer, at)); return;
        case Memory::Fault::UseAfterFree:
            fail(DiagnosticFactory::undefined_behavior(DiagCode::UseAfterFree, at));
            return;
        case Memory::Fault::Uninitialized: {
            std::vector<DiagArg> args;
            if (auto name = variable_at(PackedPointer::address(pointer))) {
                args.push_back({"name", *name});
            }
            fail(DiagnosticFactory::undefined_behavior(DiagCode::UninitializedRead, at, std::move(args)));
            return;
        }
        case Memory::Fault::DoubleFree: fail(DiagnosticFactory::undefined_behavior(DiagCode::DoubleFree, at)); return;
        case Memory::Fault::InvalidDelete:
            fail(DiagnosticFactory::undefined_behavior(DiagCode::InvalidDelete, at));
            return;
        case Memory::Fault::OutOfMemory: fail(DiagnosticFactory::out_of_memory(at, Memory::kHeapLimit)); return;
    }
}

std::uint64_t Vm::cost_of(const Instruction& ins) const noexcept {
    // Instructions without a source location are the standard library's
    // (or the setup of string literals): they cost library_instruction.
    std::uint64_t cost = 0;
    if (program_->locations[pc_].begin.line == 0) {
        cost = options_.cost.library_instruction;
    } else if (options_.cost.unit == CostModel::Unit::Statement) {
        // Only the first instruction of each statement or condition is charged.
        cost = program_->statement_starts[pc_] ? options_.cost.statement : 0;
    } else {
        cost = options_.cost.instruction;
    }
    if (ins.op == OpCode::CallHost) {
        cost += program_->host->function(FunctionId{ins.operand}).cost;
    }
    return cost;
}

void Vm::finish() {
    status_ = RunStatus::Completed;
    const std::size_t leaks = memory_.live_blocks();
    if (leaks > 0) {
        diagnostics_.push_back(DiagnosticFactory::memory_leak(current_location(), static_cast<std::int64_t>(leaks)));
    }
}

RunStatus Vm::step() {
    if (status_ == RunStatus::Paused) {
        status_ = RunStatus::Running;
    }
    if (status_ != RunStatus::Running) {
        return status_;
    }
    const Instruction& ins = code_[pc_];

    if (ins.op == OpCode::Halt) {  // reaching the end is free
        finish();
        return status_;
    }

    // Charge before executing: an action either happens completely or not at all.
    const std::uint64_t cost = cost_of(ins);
    if (!budget_.try_charge(cost)) {
        fail(DiagnosticFactory::budget_exhausted(here(), budget_.limit()));
        return status_;
    }
    if (cost != 0) {
        const std::uint32_t line = here().begin.line;
        if (line >= line_operations_.size()) {
            line_operations_.resize(line + 1);
        }
        line_operations_[line] += cost;
    }

    const std::size_t executed = pc_;
    last_line_ = program_->locations[executed].begin.line;
    execute(ins);

    if (status_ == RunStatus::Running && options_.observer != nullptr) {
        options_.observer->on_step(program_->locations[executed], budget_.used());
    }
    return status_;
}

RunResult Vm::run() {
    bool first = true;  // continuing from a breakpoint must not stop at it again
    while (true) {
        if (!first && status_ == RunStatus::Running && !breakpoints_.empty()) {
            const std::uint32_t line = program_->locations[pc_].begin.line;
            if (line != 0 && line != last_line_ && breakpoints_.contains(line)) {
                status_ = RunStatus::Paused;
                break;
            }
        }
        first = false;
        if (step() != RunStatus::Running) {
            break;
        }
    }
    return RunResult{status_, budget_.used(), diagnostics_, output_, line_operations()};
}

std::vector<LineOperations> Vm::line_operations() const {
    std::vector<LineOperations> out;
    for (std::uint32_t line = 0; line < line_operations_.size(); ++line) {
        if (line_operations_[line] != 0) {
            out.push_back(LineOperations{line, line_operations_[line]});
        }
    }
    return out;
}

RunStatus Vm::step_line() {
    const std::uint32_t start = here().begin.line;
    do {
        if (step() != RunStatus::Running) {
            break;
        }
        // "Step into": stop when the player's code reaches another line
        // (library code has no lines of its own and is stepped over).
    } while (program_->locations[pc_].begin.line == 0 || program_->locations[pc_].begin.line == start);
    return status_;
}

// =============================================================================
// Execution
// =============================================================================

void Vm::execute(const Instruction& ins) {
    std::size_t next = pc_ + 1;
    const Frame& frame = frames_.back();
    switch (ins.op) {
        case OpCode::PushConst: push(program_->constants[ins.operand].bits); break;
        case OpCode::Pop: stack_.pop_back(); break;
        case OpCode::Dup: push(stack_.back()); break;
        case OpCode::Swap: std::swap(stack_[stack_.size() - 1], stack_[stack_.size() - 2]); break;
        case OpCode::Over: push(stack_[stack_.size() - 2]); break;

        case OpCode::LocalAddr: {
            const std::uint32_t address = frame.base + ins.operand;
            push(PackedPointer::pack(address, address, address + std::max<std::uint32_t>(1, ins.wide())));
            break;
        }
        case OpCode::GlobalAddr: {
            const std::uint32_t address = memory_.globals_address(ins.operand);
            push(PackedPointer::pack(address, address, address + std::max<std::uint32_t>(1, ins.wide())));
            break;
        }
        case OpCode::LoadLocal:
        case OpCode::LoadGlobal: {
            const std::uint32_t address =
                ins.op == OpCode::LoadLocal ? frame.base + ins.operand : memory_.globals_address(ins.operand);
            if (!memory_.initialized(address)) {
                fault(Memory::Fault::Uninitialized, PackedPointer::pack(address, address, address + 1));
                return;
            }
            push(memory_.cell(address));
            break;
        }
        case OpCode::StoreLocal:
        case OpCode::StoreGlobal: {
            const std::uint32_t address =
                ins.op == OpCode::StoreLocal ? frame.base + ins.operand : memory_.globals_address(ins.operand);
            memory_.cell(address) = pop();
            memory_.set_initialized(address, true);
            break;
        }
        case OpCode::Increment: {
            const bool global = (ins.reserved & 2U) != 0;
            const std::uint32_t address = global ? memory_.globals_address(ins.operand) : frame.base + ins.operand;
            if (!memory_.initialized(address)) {
                fault(Memory::Fault::Uninitialized, PackedPointer::pack(address, address, address + 1));
                return;
            }
            std::int64_t r = 0;
            const std::int64_t delta = (ins.reserved & 1U) != 0 ? -1 : 1;
            if (!CheckedArithmetic::add(memory_.cell(address), delta, r) ||
                (ins.argc == 0 && !CheckedArithmetic::fits_int32(r))) {
                fail(DiagnosticFactory::integer_overflow(here(), ins.argc == 0 ? "int" : "long"));
                return;
            }
            memory_.cell(address) = r;
            break;
        }
        case OpCode::Load: {
            const std::int64_t p = pop();
            std::int64_t value = 0;
            if (const auto f = memory_.load(p, value); f != Memory::Fault::None) {
                fault(f, p);
                return;
            }
            push(value);
            break;
        }
        case OpCode::Store:
        case OpCode::StoreKeep: {
            const std::int64_t value = pop();
            const std::int64_t p = pop();
            if (const auto f = memory_.store(p, value); f != Memory::Fault::None) {
                fault(f, p);
                return;
            }
            if (ins.op == OpCode::StoreKeep) {
                push(value);
            }
            break;
        }
        case OpCode::Uninit:
        case OpCode::Zero: {
            const std::int64_t p = pop();
            if (const auto f = memory_.fill(p, ins.wide(), ins.op == OpCode::Zero); f != Memory::Fault::None) {
                fault(f, p);
                return;
            }
            break;
        }
        case OpCode::Copy: {
            const std::int64_t src = pop();
            const std::int64_t dst = pop();
            if (const auto f = memory_.copy(dst, src, ins.wide()); f != Memory::Fault::None) {
                fault(f, src);
                return;
            }
            break;
        }
        case OpCode::Member: {
            const std::int64_t p = pop();
            if (p == 0) {
                fault(Memory::Fault::Null);
                return;
            }
            const bool base = (ins.operand & 0x80000000U) != 0;
            const std::uint32_t address = PackedPointer::address(p) + (ins.operand & 0x7FFFFFFFU);
            push(base ? PackedPointer::with_address(p, address)
                      : PackedPointer::pack(address, address, address + std::max<std::uint32_t>(1, ins.wide())));
            break;
        }
        case OpCode::Index: {
            const std::int64_t i = pop();
            const std::int64_t p = pop();
            if (p == 0) {
                fault(Memory::Fault::Null);
                return;
            }
            const std::int64_t elem = std::max<std::uint32_t>(1, ins.wide());
            const std::int64_t begin = PackedPointer::begin(p);
            const std::int64_t end = PackedPointer::end(p);
            const std::int64_t address = PackedPointer::address(p) + i * elem;
            const std::int64_t size = ins.operand != 0 ? ins.operand : (end - begin) / elem;
            const std::int64_t index = ins.operand != 0 ? i : (address - begin) / elem;
            if (i < -(std::int64_t{1} << 30) || i > (std::int64_t{1} << 30) || address < begin ||
                address + elem > end || (ins.operand != 0 && (i < 0 || i >= ins.operand))) {
                fail(DiagnosticFactory::out_of_bounds(here(), index, size));
                return;
            }
            push(PackedPointer::with_address(p, static_cast<std::uint32_t>(address)));
            break;
        }
        case OpCode::PtrAdd:
        case OpCode::PtrSub: {
            const std::int64_t n = pop();
            const std::int64_t p = pop();
            const std::int64_t delta = (ins.op == OpCode::PtrAdd ? n : -n) * std::max<std::uint32_t>(1, ins.wide());
            if (p == 0) {
                if (delta != 0) {
                    fault(Memory::Fault::Null);
                    return;
                }
                push(0);
                break;
            }
            const std::int64_t address = std::int64_t{PackedPointer::address(p)} + delta;
            if (address <= 0 || address >= PackedPointer::kLimit) {
                fail(DiagnosticFactory::undefined_behavior(DiagCode::InvalidPointer, here()));
                return;
            }
            push(PackedPointer::with_address(p, static_cast<std::uint32_t>(address)));
            break;
        }
        case OpCode::PtrDiff: {
            const std::int64_t q = pop();
            const std::int64_t p = pop();
            push((std::int64_t{PackedPointer::address(p)} - PackedPointer::address(q)) /
                 std::max<std::uint32_t>(1, ins.wide()));
            break;
        }
        case OpCode::PtrOffset: {
            const std::int64_t p = pop();
            if (p == 0) {
                push(0);
                break;
            }
            const auto offset = static_cast<std::int32_t>(ins.operand);
            if (ins.wide() == 0) {
                const std::int64_t address = std::int64_t{PackedPointer::address(p)} + offset;
                if (address <= 0 || address >= PackedPointer::kLimit) {
                    fail(DiagnosticFactory::undefined_behavior(DiagCode::InvalidPointer, here()));
                    return;
                }
                push(PackedPointer::with_address(p, static_cast<std::uint32_t>(address)));
                break;
            }
            // Through a virtual base: its position depends on the complete object.
            std::int64_t header = 0;
            if (const auto f = memory_.load(p, header); f != Memory::Fault::None) {
                fault(f, p);
                return;
            }
            const auto complete = static_cast<std::uint32_t>(static_cast<std::uint64_t>(header) >> 32);
            const auto sub = static_cast<std::uint32_t>(header & 0xFFFFFFFF);
            const std::uint32_t full = PackedPointer::address(p) - sub;
            const RecordMeta& record = program_->records.at(complete);
            std::uint32_t vbase = 0;
            for (const auto& [r, off] : record.virtual_bases) {
                if (r == ins.wide() - 1) {
                    vbase = off;
                }
            }
            const std::uint32_t address = full + vbase + static_cast<std::uint32_t>(offset);
            push(PackedPointer::pack(address, full, full + record.cells));
            break;
        }

        case OpCode::Add:
        case OpCode::Sub:
        case OpCode::Mul:
        case OpCode::Div:
        case OpCode::Mod:
        case OpCode::Shl:
        case OpCode::Shr:
        case OpCode::BitAnd:
        case OpCode::BitOr:
        case OpCode::BitXor: arithmetic(ins); break;
        case OpCode::Neg: {
            const std::int64_t a = pop();
            if (ins.argc == 2) {
                push(from_double(-as_double(a)));
            } else if (ins.argc >= 4) {  // unsigned: wraps
                const std::uint64_t r = 0 - static_cast<std::uint64_t>(a);
                push(static_cast<std::int64_t>(ins.argc == 5 ? r : r & 0xFFFFFFFFU));
            } else if ((ins.argc == 0 && a == kInt32Min) || (ins.argc == 1 && a == kInt64Min)) {
                fail(DiagnosticFactory::integer_overflow(here(), ins.argc == 0 ? "int" : "long"));
                return;
            } else {
                push(-a);
            }
            break;
        }
        case OpCode::BitNot: {
            const std::int64_t r = ~pop();
            push(ins.argc == 4 ? r & 0xFFFFFFFF : r);
            break;
        }
        case OpCode::Eq:
        case OpCode::Ne:
        case OpCode::Lt:
        case OpCode::Gt:
        case OpCode::Le:
        case OpCode::Ge: compare(ins); break;
        case OpCode::Not: push(pop() == 0 ? 1 : 0); break;

        case OpCode::IntToDouble: push(from_double(static_cast<double>(pop()))); break;
        case OpCode::ULongToDouble: push(from_double(static_cast<double>(static_cast<std::uint64_t>(pop())))); break;
        case OpCode::DoubleToInt: {
            const double d = as_double(pop());
            static constexpr const char* names[] = {"int", "long", "unsigned int", "unsigned long"};
            bool fits = false;
            switch (ins.argc) {
                case 0: fits = d > -2147483649.0 && d < 2147483648.0; break;
                case 1: fits = d >= -9223372036854775808.0 && d < 9223372036854775808.0; break;
                case 2: fits = d > -1.0 && d < 4294967296.0; break;
                default: fits = d > -1.0 && d < 18446744073709551616.0; break;
            }
            if (!fits) {
                fail(DiagnosticFactory::integer_overflow(here(), names[ins.argc & 3U]));
                return;
            }
            push(ins.argc == 3 ? static_cast<std::int64_t>(static_cast<std::uint64_t>(d))
                               : static_cast<std::int64_t>(d));
            break;
        }
        case OpCode::ToBool:
        case OpCode::PtrToBool: push(pop() != 0 ? 1 : 0); break;
        case OpCode::DoubleToBool: push(as_double(pop()) != 0.0 ? 1 : 0); break;
        case OpCode::Trunc32: push(static_cast<std::int32_t>(static_cast<std::uint32_t>(pop() & 0xFFFFFFFF))); break;
        case OpCode::Trunc8: push(static_cast<std::int8_t>(static_cast<std::uint8_t>(pop() & 0xFF))); break;
        case OpCode::Trunc16: push(static_cast<std::int16_t>(static_cast<std::uint16_t>(pop() & 0xFFFF))); break;
        case OpCode::TruncU8: push(pop() & 0xFF); break;
        case OpCode::TruncU16: push(pop() & 0xFFFF); break;
        case OpCode::TruncU32: push(pop() & 0xFFFFFFFF); break;

        case OpCode::Jump: next = ins.operand; break;
        case OpCode::JumpIfFalse:
            if (pop() == 0) {
                next = ins.operand;
            }
            break;
        case OpCode::JumpIfTrue:
            if (pop() != 0) {
                next = ins.operand;
            }
            break;
        case OpCode::JumpCompare: {
            const std::int64_t b = pop();
            const std::int64_t a = pop();
            const auto cmp = static_cast<OpCode>(static_cast<unsigned>(OpCode::Eq) + (ins.argc >> 4U));
            if (compare_values(cmp, static_cast<std::uint16_t>(ins.argc & 0xFU), a, b) == (ins.reserved != 0)) {
                next = ins.operand;
            }
            break;
        }
        case OpCode::Call:
            pc_ = next;
            call(ins.operand, ins.argc);
            return;
        case OpCode::CallVirtual:
            pc_ = next;
            call_virtual(ins);
            return;
        case OpCode::Ret: ret(ins.argc); return;
        case OpCode::Intrinsic:
            intrinsic(ins);
            if (status_ != RunStatus::Running) {
                return;
            }
            break;
        case OpCode::CallHost:
            call_host(ins);
            if (status_ != RunStatus::Running) {
                return;
            }
            break;

        case OpCode::New: {
            const bool array = (ins.operand & 1U) != 0;
            const bool zero = (ins.operand & 2U) != 0;
            const std::int64_t count = array ? pop() : 1;
            const std::int64_t elem = std::max<std::uint32_t>(1, ins.wide());
            if (count < 0 || count > Memory::kHeapLimit / elem) {
                fail(DiagnosticFactory::out_of_memory(here(), static_cast<std::uint64_t>(count * elem)));
                return;
            }
            std::int64_t pointer = 0;
            if (const auto f = memory_.allocate(static_cast<std::uint32_t>(count * elem), zero, array, pointer);
                f != Memory::Fault::None) {
                fault(f);
                return;
            }
            push(pointer);
            break;
        }
        case OpCode::Delete: {
            const std::int64_t p = pop();
            const bool array = (ins.argc & 1U) != 0;
            std::uint32_t address = PackedPointer::address(p);
            if ((ins.argc & 2U) != 0) {
                std::int64_t header = 0;
                if (const auto f = memory_.load(p, header); f == Memory::Fault::None) {
                    address -= static_cast<std::uint32_t>(header & 0xFFFFFFFF);
                }
            } else if (address != PackedPointer::begin(p)) {
                fault(Memory::Fault::InvalidDelete);
                return;
            }
            if (const auto f = memory_.release(address, array); f != Memory::Fault::None) {
                fault(f);
                return;
            }
            break;
        }
        case OpCode::InitHeaders: {
            const std::int64_t p = pop();
            const RecordMeta& record = program_->records.at(ins.operand);
            if (const auto f = memory_.check(p, std::max<std::uint32_t>(1, record.cells)); f != Memory::Fault::None) {
                fault(f, p);
                return;
            }
            const std::uint32_t address = PackedPointer::address(p);
            for (const std::uint32_t offset : record.header_offsets) {
                memory_.cell(address + offset) = static_cast<std::int64_t>((std::uint64_t{ins.operand} << 32) | offset);
                memory_.set_initialized(address + offset, true);
            }
            break;
        }
        case OpCode::BlockCount: {
            const std::int64_t p = pop();
            const auto cells = memory_.block_cells(PackedPointer::address(p));
            if (!cells) {
                fault(Memory::Fault::InvalidDelete);
                return;
            }
            push(*cells / std::max<std::uint32_t>(1, ins.wide()));
            break;
        }
        case OpCode::FlowOffEnd:
            fail(DiagnosticFactory::flow_off_end(here(), program_->functions[ins.operand].name));
            return;

        case OpCode::PushHandler: handlers_.push_back(Handler{ins.operand, frames_.size(), stack_.size()}); break;
        case OpCode::PopHandler: handlers_.pop_back(); break;
        case OpCode::Throw: {
            const std::int64_t object = pop();
            if (in_flight_) {
                fail(DiagnosticFactory::exception_during_unwind(here(), program_->thrown[ins.operand].name));
                return;
            }
            in_flight_ = Exception{ins.operand, object, object, false, here()};
            unwind();
            return;
        }
        case OpCode::Rethrow:
            if (caught_.empty()) {
                fail(DiagnosticFactory::no_active_exception(here()));
                return;
            }
            if (in_flight_) {
                fail(DiagnosticFactory::exception_during_unwind(here(), program_->thrown[caught_.back().thrown].name));
                return;
            }
            caught_.back().rethrown = true;  // its handler ends without destroying it
            in_flight_ = caught_.back();
            in_flight_->rethrown = false;
            in_flight_->where = here();
            unwind();
            return;
        case OpCode::Resume: unwind(); return;
        case OpCode::CatchDispatch: dispatch(ins.operand); return;
        case OpCode::EndCatch: {
            const Exception& e = caught_.back();
            const auto& destructor = program_->thrown[e.thrown].destructor;
            if (!e.rethrown && destructor) {
                pc_ = next;
                push(e.object);
                call(*destructor, 1);
                return;
            }
            break;
        }
        case OpCode::FreeCaught: {
            const Exception e = caught_.back();
            caught_.pop_back();
            if (!e.rethrown) {
                (void)memory_.release(PackedPointer::address(e.object), false);
            }
            break;
        }
        case OpCode::Halt: finish(); return;
    }
    pc_ = next;
}

void Vm::arithmetic(const Instruction& ins) {
    const std::int64_t b = pop();
    const std::int64_t a = pop();
    const SourceRange at = here();
    if (ins.argc == 2) {
        const double x = as_double(a);
        const double y = as_double(b);
        double r = 0;
        switch (ins.op) {
            case OpCode::Add: r = x + y; break;
            case OpCode::Sub: r = x - y; break;
            case OpCode::Mul: r = x * y; break;
            case OpCode::Div: r = x / y; break;
            default: break;
        }
        push(from_double(r));
        return;
    }
    if (ins.argc >= 4) {
        unsigned_arithmetic(ins, a, b);
        return;
    }
    const bool wide = ins.argc == 1;
    const char* type = wide ? "long" : "int";
    std::int64_t r = 0;
    bool ok = true;
    switch (ins.op) {
        case OpCode::Add: ok = CheckedArithmetic::add(a, b, r); break;
        case OpCode::Sub: ok = CheckedArithmetic::sub(a, b, r); break;
        case OpCode::Mul: ok = CheckedArithmetic::mul(a, b, r); break;
        case OpCode::Div:
        case OpCode::Mod:
            if (b == 0) {
                fail(DiagnosticFactory::undefined_behavior(DiagCode::DivisionByZero, at));
                return;
            }
            if (b == -1 && a == (wide ? kInt64Min : kInt32Min)) {
                ok = false;
                break;
            }
            r = ins.op == OpCode::Div ? a / b : a % b;
            break;
        case OpCode::Shl:
        case OpCode::Shr: {
            const std::int64_t width = wide ? 64 : 32;
            if (b < 0 || b >= width) {
                fail(DiagnosticFactory::invalid_shift(at, b));
                return;
            }
            if (ins.op == OpCode::Shr) {
                r = a >> b;
            } else if (wide) {
                r = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) << b);
            } else {
                r = static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << b);
            }
            break;
        }
        case OpCode::BitAnd: r = a & b; break;
        case OpCode::BitOr: r = a | b; break;
        case OpCode::BitXor: r = a ^ b; break;
        default: break;
    }
    if (!ok || (!wide && !CheckedArithmetic::fits_int32(r))) {
        fail(DiagnosticFactory::integer_overflow(at, type));
        return;
    }
    push(r);
}

void Vm::unsigned_arithmetic(const Instruction& ins, std::int64_t a, std::int64_t b) {
    // Unsigned arithmetic never overflows: it wraps modulo 2^bits.
    const bool wide = ins.argc == 5;
    const auto x = static_cast<std::uint64_t>(a);
    const auto y = static_cast<std::uint64_t>(b);
    std::uint64_t r = 0;
    switch (ins.op) {
        case OpCode::Add: r = x + y; break;
        case OpCode::Sub: r = x - y; break;
        case OpCode::Mul: r = x * y; break;
        case OpCode::Div:
        case OpCode::Mod:
            if (y == 0) {
                fail(DiagnosticFactory::undefined_behavior(DiagCode::DivisionByZero, here()));
                return;
            }
            r = ins.op == OpCode::Div ? x / y : x % y;
            break;
        case OpCode::Shl:
        case OpCode::Shr:
            if (b < 0 || b >= (wide ? 64 : 32)) {
                fail(DiagnosticFactory::invalid_shift(here(), b));
                return;
            }
            r = ins.op == OpCode::Shl ? x << y : x >> y;
            break;
        case OpCode::BitAnd: r = x & y; break;
        case OpCode::BitOr: r = x | y; break;
        case OpCode::BitXor: r = x ^ y; break;
        default: break;
    }
    push(static_cast<std::int64_t>(wide ? r : r & 0xFFFFFFFFU));
}

void Vm::compare(const Instruction& ins) {
    const std::int64_t b = pop();
    const std::int64_t a = pop();
    push(compare_values(ins.op, ins.argc, a, b) ? 1 : 0);
}

bool Vm::compare_values(OpCode op, std::uint16_t kind, std::int64_t a, std::int64_t b) noexcept {
    const auto apply = [op](auto x, auto y) {
        switch (op) {
            case OpCode::Eq: return x == y;
            case OpCode::Ne: return x != y;
            case OpCode::Lt: return x < y;
            case OpCode::Gt: return x > y;
            case OpCode::Le: return x <= y;
            case OpCode::Ge: return x >= y;
            default: return false;
        }
    };
    switch (kind) {
        case 2: return apply(as_double(a), as_double(b));
        case 3: return apply(PackedPointer::address(a), PackedPointer::address(b));
        case 5: return apply(static_cast<std::uint64_t>(a), static_cast<std::uint64_t>(b));  // unsigned long
        default: return apply(a, b);
    }
}

// =============================================================================
// Calls
// =============================================================================

void Vm::call(std::uint32_t function, std::uint16_t argc) {
    const FunctionMeta& fn = program_->functions[function];
    const SourceRange at = located(pc_ - 1);
    if (fn.is_pure) {
        fail(DiagnosticFactory::undefined_behavior(DiagCode::PureVirtualCall, at, {{"function", fn.name}}));
        return;
    }
    if (frames_.size() >= options_.max_call_depth) {
        fail(DiagnosticFactory::stack_overflow(at, static_cast<std::uint32_t>(frames_.size())));
        return;
    }
    const auto base = memory_.push_frame(fn.frame_cells);
    if (!base) {
        fail(DiagnosticFactory::stack_overflow(at, static_cast<std::uint32_t>(frames_.size())));
        return;
    }
    for (std::size_t i = argc; i-- > 0;) {
        const std::int64_t value = pop();
        if (i >= fn.params.size()) {
            continue;
        }
        const ParamMeta& p = fn.params[i];
        const std::uint32_t address = *base + p.offset;
        if (p.copy_block) {
            const std::int64_t dst = PackedPointer::pack(address, address, address + p.cells);
            if (const auto f = memory_.copy(dst, value, p.cells); f != Memory::Fault::None) {
                memory_.pop_frame(*base);
                pc_ -= 1;
                fault(f, value);
                return;
            }
        } else {
            memory_.cell(address) = value;
            memory_.set_initialized(address, true);
        }
    }
    frames_.push_back(Frame{function, *base, pc_});
    pc_ = fn.entry;
}

void Vm::call_virtual(const Instruction& ins) {
    const std::size_t index = stack_.size() - ins.argc;
    const std::int64_t self = stack_[index];
    std::int64_t header = 0;
    if (const auto f = memory_.load(self, header); f != Memory::Fault::None) {
        pc_ -= 1;
        fault(f == Memory::Fault::Uninitialized ? Memory::Fault::Dangling : f, self);
        return;
    }
    const auto complete = static_cast<std::uint32_t>(static_cast<std::uint64_t>(header) >> 32);
    const auto sub = static_cast<std::uint32_t>(header & 0xFFFFFFFF);
    if (complete >= program_->records.size()) {
        pc_ -= 1;
        fault(Memory::Fault::Dangling, self);
        return;
    }
    const RecordMeta& record = program_->records[complete];
    const std::uint32_t full = PackedPointer::address(self) - sub;
    // Subobjects that share a header (a class and its first base) are told
    // apart by the class that declares the called function.
    const auto owner = program_->functions[ins.operand].record;
    for (const auto& s : record.subobjects) {
        if (s.offset != sub || (owner && s.record != *owner) || ins.reserved >= s.slots.size()) {
            continue;
        }
        const auto& slot = s.slots[ins.reserved];
        const std::uint32_t address = full + slot.this_offset;
        stack_[index] = PackedPointer::pack(address, full, full + record.cells);
        call(slot.function, ins.argc);
        return;
    }
    // No dispatch table entry: call the statically chosen function.
    call(ins.operand, ins.argc);
}

void Vm::ret(std::uint16_t argc) {
    const std::int64_t value = argc != 0 ? pop() : 0;
    const Frame frame = frames_.back();
    frames_.pop_back();
    if (frames_.empty()) {
        finish();
        return;
    }
    memory_.pop_frame(frame.base);
    pc_ = frame.return_pc;
    if (argc != 0) {
        push(value);
    }
}

// =============================================================================
// Exceptions
// =============================================================================

void Vm::unwind() {
    if (!in_flight_) {
        return;  // every throw sets it before unwinding
    }
    if (handlers_.empty()) {
        const Exception e = *in_flight_;
        fail(DiagnosticFactory::uncaught_exception(e.where, program_->thrown[e.thrown].name,
                                                   exception_message(e.thrown, e.object)));
        return;
    }
    // Jump to the innermost handler: a cleanup pad (destructors) or a catch dispatch.
    const Handler h = handlers_.back();
    handlers_.pop_back();
    if (frames_.size() > h.frames) {
        memory_.pop_frame(frames_[h.frames].base);
        frames_.resize(h.frames);
    }
    stack_.resize(h.stack);
    pc_ = h.pc;
}

void Vm::dispatch(std::uint32_t table) {
    if (!in_flight_) {
        return;
    }
    const Exception thrown = *in_flight_;
    for (const CatchClauseMeta& clause : program_->catch_tables[table]) {
        std::optional<std::int64_t> view;
        if (clause.catch_all) {
            view = thrown.object;
        }
        for (const auto& [index, offset] : clause.matches) {
            if (!view && index == thrown.thrown) {
                view = PackedPointer::with_address(thrown.object, PackedPointer::address(thrown.object) + offset);
            }
        }
        if (view) {
            Exception caught = thrown;
            caught.view = *view;
            caught_.push_back(caught);
            in_flight_.reset();
            pc_ = clause.pc;
            return;
        }
    }
    unwind();  // no clause takes it: keep looking outward
}

std::optional<std::string> Vm::exception_message(std::uint32_t thrown, std::int64_t object) const {
    const auto& offset = program_->thrown[thrown].message_offset;
    if (!offset) {
        return std::nullopt;
    }
    std::int64_t text = 0;
    if (memory_.load(PackedPointer::with_address(object, PackedPointer::address(object) + *offset), text) !=
            Memory::Fault::None ||
        text == 0) {
        return std::nullopt;
    }
    std::string out;
    for (std::uint32_t i = 0; i < 200; ++i) {
        std::int64_t c = 0;
        if (memory_.load(PackedPointer::with_address(text, PackedPointer::address(text) + i), c) !=
                Memory::Fault::None ||
            c == 0) {
            break;
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

void Vm::write(std::string_view text) {
    output_.append(text);
    if (options_.observer != nullptr) {
        options_.observer->on_output(text);
    }
}

void Vm::intrinsic(const Instruction& ins) {
    switch (static_cast<sema::Intrinsic>(ins.operand)) {
        case sema::Intrinsic::WriteLong: write(std::to_string(pop())); break;
        case sema::Intrinsic::WriteULong: write(std::to_string(static_cast<std::uint64_t>(pop()))); break;
        case sema::Intrinsic::WriteDouble: write(NumberFormat::shortest(std::bit_cast<double>(pop()))); break;
        case sema::Intrinsic::WriteChar: {
            const char c = static_cast<char>(pop());
            write(std::string_view(&c, 1));
            break;
        }
        case sema::Intrinsic::WriteString: {
            std::int64_t p = pop();
            std::string text;
            for (std::size_t i = 0; i < 1U << 16; ++i) {
                std::int64_t c = 0;
                if (const auto f = memory_.load(p, c); f != Memory::Fault::None) {
                    fault(f, p);
                    return;
                }
                if (c == 0) {
                    break;
                }
                text.push_back(static_cast<char>(c));
                p = PackedPointer::with_address(p, PackedPointer::address(p) + 1);
            }
            write(text);
            break;
        }
        case sema::Intrinsic::CheckIndex: {
            const std::int64_t size = pop();
            const std::int64_t index = pop();
            if (index < 0 || index >= size) {
                fail(DiagnosticFactory::out_of_bounds(here(), index, size));
            }
            break;
        }
        case sema::Intrinsic::Caught: push(caught_.empty() ? 0 : caught_.back().view); break;
        case sema::Intrinsic::BadAccess:
            if (pop() == 0) {
                fail(DiagnosticFactory::undefined_behavior(DiagCode::BadAccess, here(), {{"what", "std::optional"}}));
            }
            break;
        case sema::Intrinsic::Count: break;
    }
}

void Vm::call_host(const Instruction& ins) {
    const auto& fn = program_->host->function(FunctionId{ins.operand});
    const std::size_t argc = ins.argc;
    const SourceRange location = here();

    std::vector<Value> values(argc);
    for (std::size_t i = 0; i < argc; ++i) {
        const std::int64_t raw = stack_[stack_.size() - argc + i];
        const TypeId type = fn.params[i];
        values[i] = type == types::Bool ? Value::from_bool(raw != 0) : Value::from_raw(type, raw);
    }
    const std::span<const Value> args(values.data(), argc);

    HostCall call(fn.name, args, location);
    if (options_.observer != nullptr) {
        options_.observer->on_host_call(call);
    }
    const Value result = invoker_.invoke(fn, call);
    if (call.failed()) {
        fail(DiagnosticFactory::host_error(location, fn.name, call.error(), call.detail()));
        return;
    }
    stack_.resize(stack_.size() - argc);
    if (fn.result != types::Void) {
        push(result.raw());
    }
}

}  // namespace cppi::detail
