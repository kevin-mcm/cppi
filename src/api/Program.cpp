/// @file Program.cpp
/// @brief Implementation of Program.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Program.hpp>

#include "codegen/Disassembler.hpp"
#include "codegen/ProgramData.hpp"

namespace cppi {

std::size_t Program::instruction_count() const noexcept {
    return data_->code.size();
}

std::size_t Program::max_stack_depth() const noexcept {
    return data_->max_stack;
}

const HostRegistry& Program::host() const noexcept {
    return *data_->host;
}

std::string Program::disassemble() const {
    return detail::Disassembler(*data_).listing();
}

}  // namespace cppi
