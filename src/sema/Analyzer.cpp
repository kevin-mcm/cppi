/// @file Analyzer.cpp
/// @brief Implementation of Analyzer.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/Analyzer.hpp"

#include "sema/AnalysisContext.hpp"
#include "sema/DeclarationBinder.hpp"
#include "sema/ExpressionBinder.hpp"
#include "sema/InitializerBinder.hpp"
#include "sema/LibraryScope.hpp"
#include "sema/OverloadResolver.hpp"
#include "sema/StatementBinder.hpp"
#include "sema/TemplateEngine.hpp"
#include "sema/TypeResolver.hpp"

#include <iterator>
#include <utility>

namespace cppi::sema {

AnalysisResult Analyzer::analyze(const ast::TranslationUnit& unit, const ast::TranslationUnit* prelude) const {
    AnalysisResult result;
    AnalysisContext ctx(host_, options_, result.diagnostics);

    TypeResolver type_resolver(ctx);
    ExpressionBinder expressions(ctx);
    InitializerBinder initializers(ctx);
    OverloadResolver overloads(ctx);
    StatementBinder statements(ctx);
    DeclarationBinder declarations(ctx);
    TemplateEngine templates(ctx);
    ctx.type_resolver = &type_resolver;
    ctx.expressions = &expressions;
    ctx.initializers = &initializers;
    ctx.overloads = &overloads;
    ctx.statements = &statements;
    ctx.declarations = &declarations;
    ctx.templates = &templates;

    // Function 0 is the script: the top-level statements, in order.
    FunctionInfo script;
    script.name = "<script>";
    script.display = "<script>";
    script.return_type = TypeTable::kVoid;
    script.defined = true;
    declarations.add_function(std::move(script));
    ctx.program.functions.front().is_script = true;

    FunctionContext script_ctx;
    ctx.fn = &script_ctx;
    declarations.register_host();

    BBlock body;
    std::vector<BStmt>* out = &body.statements;
    if (prelude != nullptr) {
        const LibraryScope library(ctx, SourceRange{});
        for (const auto& statement : prelude->statements) {
            statements.bind(statement, out);
        }
    }
    for (const auto& statement : unit.statements) {
        statements.bind(statement, out);
    }
    declarations.finish(*out);

    BoundFunction& bound = ctx.program.functions.front();
    bound.body = std::move(body);
    bound.body.cleanup = std::move(declarations.global_cleanup());
    bound.frame_cells = script_ctx.frame.size();
    bound.defined = true;

    result.program = std::move(ctx.program);
    result.program.types = ctx.types_ptr;
    return result;
}

}  // namespace cppi::sema
