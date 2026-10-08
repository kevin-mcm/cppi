/// @file CompilerPipeline.cpp
/// @brief Implementation of CompilerPipeline.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "pipeline/CompilerPipeline.hpp"

#include "codegen/CodeGenerator.hpp"
#include "library/Prelude.hpp"
#include "parse/Parser.hpp"
#include "sema/Analyzer.hpp"

#include <utility>

namespace cppi::detail {

CompilerOutput CompilerPipeline::compile(std::string_view source) const {
    // Creating a tree-sitter parser is cheap but not free; reuse one per thread.
    thread_local parse::Parser parser;
    // The standard library is parsed once per thread and shared by every compile.
    thread_local const parse::ParseResult prelude = parser.parse(library::Prelude::source());

    CompilerOutput output;
    auto parsed = parser.parse(source);
    if (!parsed.ok()) {
        output.diagnostics = std::move(parsed.diagnostics);
        return output;
    }

    auto analysis = sema::Analyzer(*host_, options_).analyze(parsed.unit, prelude.ok() ? &prelude.unit : nullptr);
    const bool has_errors = analysis.has_errors();
    output.diagnostics = std::move(analysis.diagnostics);  // warnings travel with a successful compile too
    if (has_errors) {
        return output;
    }

    output.program = CodeGenerator::generate(analysis.program, host_);
    return output;
}

}  // namespace cppi::detail
