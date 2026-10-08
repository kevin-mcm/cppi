#pragma once

/// @file AnalysisContext.hpp
/// @brief Everything the analyzer's collaborators share while binding one
/// program (Mediator: binders reach each other through here, not directly).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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

/// Forward declarations of the collaborators that share the context.
class DeclarationBinder;
class ExpressionBinder;
class InitializerBinder;
class OverloadResolver;
class StatementBinder;
class TemplateEngine;
class TypeResolver;

/// Shared state of one analysis: the host, the options, the diagnostics, the
/// type table and every table the binders fill. The binders reach each other
/// through the pointers at the end.
class AnalysisContext {
public:
    /// @param host_registry What the host exposes to player code.
    /// @param level_options Standard and locked features.
    /// @param diagnostics   Where diagnostics are appended.
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

    /// Reports a diagnostic, moving it to the player's code when it comes from
    /// the standard library or an implicit member function.
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

    /// What the host exposes.
    const HostRegistry& host;
    /// Standard and locked features.
    const Options& options;
    /// Collects diagnostics (see report()).
    detail::DiagnosticSink sink;
    /// The type table, shared with the bound program.
    std::shared_ptr<TypeTable> types_ptr;
    /// `*types_ptr`.
    TypeTable& types;
    /// Questions about base classes.
    ClassHierarchy hierarchy;
    /// The conversions C++ applies on its own.
    ImplicitConversions conversions;
    /// Evaluates constant expressions.
    ConstEvaluator constants;
    /// Decides which language features may be used.
    FeatureGate features;
    /// The scopes visible at the current point.
    SymbolTable symbols;
    std::deque<FunctionInfo> functions;  ///< [0] is the script (a deque: references stay valid)
    /// The program being built.
    BoundProgram program;
    /// Runs constexpr functions at compile time.
    ConstexprInterpreter interpreter;
    /// The function whose body is being bound; null outside functions.
    FunctionContext* fn = nullptr;
    std::set<std::uint32_t> called;  ///< user functions some call targets (undefined ones are errors)
    int library_depth = 0;           ///< > 0 while binding the standard library prelude
    SourceRange library_site;        ///< the player's code that caused the current library binding
    /// The library template instance that code used ("std::sort<P>"), if any.
    std::string library_entry;
    /// While binding an implicit member function (no lines of its own): the
    /// player's code that needed it, where its errors are reported.
    SourceRange implicit_site;

    /// Binds expressions.
    ExpressionBinder* expressions = nullptr;
    /// Binds statements.
    StatementBinder* statements = nullptr;
    /// Binds declarations.
    DeclarationBinder* declarations = nullptr;
    /// Lowers initialization.
    InitializerBinder* initializers = nullptr;
    /// Resolves written types.
    TypeResolver* type_resolver = nullptr;
    /// Picks overloads.
    OverloadResolver* overloads = nullptr;
    /// Instantiates templates.
    TemplateEngine* templates = nullptr;
};

}  // namespace cppi::sema
