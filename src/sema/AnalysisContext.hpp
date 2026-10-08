#pragma once

/// Everything the analyzer's collaborators share while binding one program
/// (Mediator: binders reach each other through here, not directly).

#include "sema/AnalysisResult.hpp"
#include "sema/ClassHierarchy.hpp"
#include "sema/ConstEvaluator.hpp"
#include "sema/ConstexprInterpreter.hpp"
#include "sema/FeatureGate.hpp"
#include "sema/FunctionContext.hpp"
#include "sema/FunctionInfo.hpp"
#include "sema/ImplicitConversions.hpp"
#include "sema/SymbolTable.hpp"
#include "sema/TypeTable.hpp"
#include "support/DiagnosticSink.hpp"

#include <cppi/HostRegistry.hpp>
#include <cppi/Options.hpp>

#include <deque>
#include <memory>
#include <set>
#include <vector>

namespace cppi::sema {

class DeclarationBinder;
class ExpressionBinder;
class InitializerBinder;
class OverloadResolver;
class StatementBinder;
class TemplateEngine;
class TypeResolver;

class AnalysisContext {
public:
    AnalysisContext(const HostRegistry& host_registry, const Options& level_options,
                    std::vector<Diagnostic>& diagnostics)
        : host(host_registry),
          options(level_options),
          sink(diagnostics),
          types_ptr(std::make_shared<TypeTable>()),
          types(*types_ptr),
          hierarchy(types),
          conversions(types, hierarchy),
          constants(types),
          features(options, sink),
          interpreter(constants, types, functions, program) {
        constants.attach(&interpreter);
    }
    AnalysisContext(const AnalysisContext&) = delete;
    AnalysisContext& operator=(const AnalysisContext&) = delete;
    AnalysisContext(AnalysisContext&&) = delete;
    AnalysisContext& operator=(AnalysisContext&&) = delete;
    ~AnalysisContext() = default;

    void report(Diagnostic d) {
        // Problems inside the standard library prelude are reported where the
        // player's code used it.
        if (library_depth > 0 && library_site.begin.line != 0) {
            d.range = library_site;
        } else if (d.range.begin.line == 0 && implicit_site.begin.line != 0) {
            d.range = implicit_site;
        }
        sink.report(std::move(d));
    }

    /// Cells in the current function's frame.
    [[nodiscard]] std::uint32_t allocate_local(std::uint32_t cells) const { return fn->frame.allocate(cells); }

    const HostRegistry& host;
    const Options& options;
    detail::DiagnosticSink sink;
    std::shared_ptr<TypeTable> types_ptr;
    TypeTable& types;
    ClassHierarchy hierarchy;
    ImplicitConversions conversions;
    ConstEvaluator constants;
    FeatureGate features;
    SymbolTable symbols;
    std::deque<FunctionInfo> functions;  ///< [0] is the script (a deque: references stay valid)
    BoundProgram program;
    ConstexprInterpreter interpreter;
    FunctionContext* fn = nullptr;
    std::set<std::uint32_t> called;  ///< user functions some call targets (undefined ones are errors)
    int library_depth = 0;           ///< > 0 while binding the standard library prelude
    SourceRange library_site;        ///< the player's code that caused the current library binding
    std::string library_entry;
    /// While binding an implicit member function (no lines of its own): the
    /// player's code that needed it, where its errors are reported.
    SourceRange implicit_site;  ///< the library template instance that code used ("std::sort<P>"), if any

    ExpressionBinder* expressions = nullptr;
    StatementBinder* statements = nullptr;
    DeclarationBinder* declarations = nullptr;
    InitializerBinder* initializers = nullptr;
    TypeResolver* type_resolver = nullptr;
    OverloadResolver* overloads = nullptr;
    TemplateEngine* templates = nullptr;
};

}  // namespace cppi::sema
