/// @file Execution.cpp
/// @brief Implementation of Execution.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Execution.hpp>

#include "vm/Vm.hpp"

#include <utility>

namespace cppi {

Execution::Execution(std::unique_ptr<detail::Vm> vm) noexcept : vm_(std::move(vm)) {}
Execution::Execution(Execution&&) noexcept = default;
Execution& Execution::operator=(Execution&&) noexcept = default;
Execution::~Execution() = default;

RunStatus Execution::step() {
    return vm_->step();
}
RunStatus Execution::step_line() {
    return vm_->step_line();
}
RunResult Execution::run() {
    return vm_->run();
}
void Execution::set_breakpoint(std::uint32_t line) {
    vm_->set_breakpoint(line);
}
void Execution::clear_breakpoint(std::uint32_t line) {
    vm_->clear_breakpoint(line);
}
void Execution::clear_breakpoints() {
    vm_->clear_breakpoints();
}
std::vector<StackFrame> Execution::call_stack() const {
    return vm_->call_stack();
}
std::vector<Variable> Execution::globals() const {
    return vm_->globals();
}
bool Execution::finished() const noexcept {
    return vm_->finished();
}
RunStatus Execution::status() const noexcept {
    return vm_->status();
}
std::uint64_t Execution::operations() const noexcept {
    return vm_->operations();
}
std::vector<LineOperations> Execution::line_operations() const {
    return vm_->line_operations();
}
SourceRange Execution::current_location() const noexcept {
    return vm_->current_location();
}
const std::vector<Diagnostic>& Execution::diagnostics() const noexcept {
    return vm_->diagnostics();
}
const std::string& Execution::output() const noexcept {
    return vm_->output();
}

}  // namespace cppi
