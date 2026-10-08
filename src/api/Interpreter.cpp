/// @file Interpreter.cpp
/// @brief Implementation of Interpreter.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Interpreter.hpp>

#include "pipeline/CompilerPipeline.hpp"
#include "vm/Vm.hpp"

#include <utility>

namespace cppi {

Interpreter::Interpreter(HostRegistry host, Options options)
    : host_(std::make_shared<const HostRegistry>(std::move(host))), options_(options) {}

CompileResult Interpreter::compile(std::string_view source) const {
    auto output = detail::CompilerPipeline(host_, options_).compile(source);
    CompileResult result;
    result.diagnostics = std::move(output.diagnostics);
    if (output.program) {
        result.program = Program(std::move(output.program));
    }
    return result;
}

Execution Interpreter::start(const Program& program, const RunOptions& options) const {
    return Execution(std::make_unique<detail::Vm>(program.data_, options));
}

RunResult Interpreter::run(const Program& program, const RunOptions& options) const {
    detail::Vm vm(program.data_, options);
    return vm.run();
}

}  // namespace cppi
