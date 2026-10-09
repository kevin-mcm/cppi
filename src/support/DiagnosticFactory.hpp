#pragma once

/// @file DiagnosticFactory.hpp
/// @brief Factory for every diagnostic the interpreter can emit.
///
/// Argument names and their order are part of the public contract
/// (docs/diagnostics.md), so they are spelled out in exactly one place.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Diagnostic.hpp>
#include <cppi/Feature.hpp>
#include <cppi/Severity.hpp>
#include <cppi/SourceRange.hpp>
#include <cppi/Standard.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppi::detail {

/// Builds every diagnostic: one function per DiagCode, taking its arguments.
class DiagnosticFactory {
public:
    // --- 1xx syntax -------------------------------------------------------
    /// DiagCode::SyntaxError (E0100); args: none.
    [[nodiscard]] static Diagnostic syntax_error(SourceRange range);
    /// `line_end`: the line whose end the token is missing from (0 if it goes mid-line).
    [[nodiscard]] static Diagnostic missing_token(SourceRange range, std::string_view token,
                                                  std::uint32_t line_end = 0);
    /// DiagCode::NestingTooDeep (E0102); args: limit.
    [[nodiscard]] static Diagnostic nesting_too_deep(SourceRange range, std::uint32_t limit);

    // --- 2xx semantic -----------------------------------------------------
    /// DiagCode::UnknownFunction (E0200); args: name, [suggestion].
    [[nodiscard]] static Diagnostic unknown_function(SourceRange range, const std::string& name,
                                                     const std::optional<std::string>& suggestion);
    /// DiagCode::UnknownIdentifier (E0201); args: name, [suggestion].
    [[nodiscard]] static Diagnostic unknown_identifier(SourceRange range, const std::string& name,
                                                       const std::optional<std::string>& suggestion);
    /// DiagCode::ArgumentCountMismatch (E0202); args: function, expected, actual.
    [[nodiscard]] static Diagnostic argument_count_mismatch(SourceRange range, const std::string& function,
                                                            std::size_t expected, std::size_t actual);
    /// DiagCode::ArgumentTypeMismatch (E0203); args: function, index (1-based), expected, actual.
    [[nodiscard]] static Diagnostic argument_type_mismatch(SourceRange range, const std::string& function,
                                                           std::size_t index, const std::string& expected,
                                                           const std::string& actual);
    /// DiagCode::NotCallable (E0204); args: name.
    [[nodiscard]] static Diagnostic not_callable(SourceRange range, const std::string& name);
    /// DiagCode::IntegerOutOfRange (E0205); args: literal.
    [[nodiscard]] static Diagnostic integer_out_of_range(SourceRange range, const std::string& literal);
    /// DiagCode::FunctionNotCalled (E0206); args: name (a function used without `()`).
    [[nodiscard]] static Diagnostic function_not_called(SourceRange range, const std::string& name, Severity severity);
    /// DiagCode::ExpressionHasNoEffect (E0207); warning; args: none.
    [[nodiscard]] static Diagnostic expression_has_no_effect(SourceRange range);
    /// DiagCode::Redefinition (E0208); args: name.
    [[nodiscard]] static Diagnostic redefinition(SourceRange range, const std::string& name);
    /// DiagCode::NotAssignable (E0209); args: [name].
    [[nodiscard]] static Diagnostic not_assignable(SourceRange range, const std::optional<std::string>& name);
    /// DiagCode::InvalidOperands (E0210); args: op, left, [right].
    [[nodiscard]] static Diagnostic invalid_operands(SourceRange range, std::string_view op, const std::string& left,
                                                     const std::optional<std::string>& right);
    /// DiagCode::CannotConvert (E0211); args: from, to.
    [[nodiscard]] static Diagnostic cannot_convert(SourceRange range, const std::string& from, const std::string& to);
    /// DiagCode::MisplacedJump (E0212); args: statement ("break", "continue").
    [[nodiscard]] static Diagnostic misplaced_jump(SourceRange range, std::string_view statement);
    /// DiagCode::ReturnTypeMismatch (E0213); args: function, expected, actual.
    [[nodiscard]] static Diagnostic return_type_mismatch(SourceRange range, const std::string& function,
                                                         const std::string& expected, const std::string& actual);
    /// DiagCode::UnknownType (E0214); args: name, [suggestion].
    [[nodiscard]] static Diagnostic unknown_type(SourceRange range, const std::string& name,
                                                 const std::optional<std::string>& suggestion);
    /// DiagCode::InvalidArraySize (E0215); args: [size].
    [[nodiscard]] static Diagnostic invalid_array_size(SourceRange range, std::optional<std::int64_t> size);
    /// DiagCode::NotConstant (E0216); args: none.
    [[nodiscard]] static Diagnostic not_constant(SourceRange range);
    /// DiagCode::DuplicateCase (E0217); args: value.
    [[nodiscard]] static Diagnostic duplicate_case(SourceRange range, std::int64_t value);
    /// DiagCode::NoMember (E0218); args: type, member, [suggestion].
    [[nodiscard]] static Diagnostic no_member(SourceRange range, const std::string& type, const std::string& member,
                                              const std::optional<std::string>& suggestion);
    /// DiagCode::NotSubscriptable (E0219); args: type.
    [[nodiscard]] static Diagnostic not_subscriptable(SourceRange range, const std::string& type);
    /// DiagCode::TooManyInitializers (E0220); args: expected, actual.
    [[nodiscard]] static Diagnostic too_many_initializers(SourceRange range, std::size_t expected, std::size_t actual);
    /// DiagCode::UninitializedConst (E0221); args: name.
    [[nodiscard]] static Diagnostic uninitialized_const(SourceRange range, const std::string& name);
    /// DiagCode::MissingReturn (E0222); warning; args: function.
    [[nodiscard]] static Diagnostic missing_return(SourceRange range, const std::string& function);
    /// DiagCode::InaccessibleMember (E0223); args: member, class, access.
    [[nodiscard]] static Diagnostic inaccessible_member(SourceRange range, const std::string& member,
                                                        const std::string& record, std::string_view access);
    /// DiagCode::AmbiguousName (E0224); args: name.
    [[nodiscard]] static Diagnostic ambiguous_name(SourceRange range, const std::string& name);
    /// DiagCode::AbstractClass (E0225); args: class, [function].
    [[nodiscard]] static Diagnostic abstract_class(SourceRange range, const std::string& record,
                                                   const std::optional<std::string>& function);
    /// DiagCode::NoMatchingFunction (E0226); args: function, arguments.
    [[nodiscard]] static Diagnostic no_matching_function(SourceRange range, const std::string& function,
                                                         const std::string& arguments);
    /// DiagCode::AmbiguousCall (E0235); args: function, arguments.
    [[nodiscard]] static Diagnostic ambiguous_call(SourceRange range, const std::string& function,
                                                   const std::string& arguments);
    /// DiagCode::StaticAssertionFailed (E0227); args: [message].
    [[nodiscard]] static Diagnostic static_assertion_failed(SourceRange range, const std::string& message);
    /// DiagCode::AssignmentInCondition (E0228); warning; args: none.
    [[nodiscard]] static Diagnostic assignment_in_condition(SourceRange range);
    /// DiagCode::ReferenceNeedsLvalue (E0229); args: type.
    [[nodiscard]] static Diagnostic reference_needs_lvalue(SourceRange range, const std::string& type);
    /// DiagCode::UnsupportedType (E0230); args: name.
    [[nodiscard]] static Diagnostic unsupported_type(SourceRange range, const std::string& name);
    /// DiagCode::DeclarationNotAllowed (E0231); args: construct.
    [[nodiscard]] static Diagnostic declaration_not_allowed(SourceRange range, std::string_view construct);
    /// DiagCode::NoDefaultConstructor (E0232); args: class.
    [[nodiscard]] static Diagnostic no_default_constructor(SourceRange range, const std::string& record);
    /// DiagCode::NothingToOverride (E0233); args: function.
    [[nodiscard]] static Diagnostic nothing_to_override(SourceRange range, const std::string& function);
    /// DiagCode::TemplateDeduction (E0234); args: function.
    [[nodiscard]] static Diagnostic template_deduction(SourceRange range, const std::string& function);
    /// DiagCode::UndefinedFunction (E0236); args: function (declared, called, never defined).
    [[nodiscard]] static Diagnostic undefined_function(SourceRange range, const std::string& function);
    /// DiagCode::DeletedFunction (E0237); args: function.
    [[nodiscard]] static Diagnostic deleted_function(SourceRange range, const std::string& function);

    // --- 3xx language features ---------------------------------------------
    /// DiagCode::FeatureLocked (E0300); args: feature.
    [[nodiscard]] static Diagnostic feature_locked(SourceRange range, Feature feature);
    /// DiagCode::FeatureRequiresStandard (E0301); args: feature, required, current.
    [[nodiscard]] static Diagnostic feature_requires_standard(SourceRange range, Feature feature, Standard current);
    /// DiagCode::FeatureNotImplemented (E0302); args: feature.
    [[nodiscard]] static Diagnostic feature_not_implemented(SourceRange range, Feature feature);
    /// DiagCode::UnsupportedSyntax (E0303); args: construct.
    [[nodiscard]] static Diagnostic unsupported_syntax(SourceRange range, const std::string& construct);

    // --- 4xx runtime --------------------------------------------------------
    /// DiagCode::BudgetExhausted (E0400); args: budget.
    [[nodiscard]] static Diagnostic budget_exhausted(SourceRange range, std::uint64_t budget);
    /// DiagCode::HostError (E0401); args: function, error (host-defined code), detail.
    [[nodiscard]] static Diagnostic host_error(SourceRange range, const std::string& function, std::uint32_t error,
                                               const std::string& detail);
    /// DiagCode::StackOverflow (E0402); args: depth.
    [[nodiscard]] static Diagnostic stack_overflow(SourceRange range, std::uint32_t depth);
    /// DiagCode::OutOfMemory (E0403); args: cells.
    [[nodiscard]] static Diagnostic out_of_memory(SourceRange range, std::uint64_t cells);
    /// DiagCode::UncaughtException (E0404); args: type, [what] (std::exception's message).
    [[nodiscard]] static Diagnostic uncaught_exception(SourceRange range, std::string_view type,
                                                       std::optional<std::string> what);
    /// DiagCode::ExceptionDuringUnwind (E0405); args: type (thrown while another exception was unwinding).
    [[nodiscard]] static Diagnostic exception_during_unwind(SourceRange range, std::string_view type);
    /// DiagCode::NoActiveException (E0406); args: none (`throw;` outside a handler).
    [[nodiscard]] static Diagnostic no_active_exception(SourceRange range);
    /// DiagCode::InternalError (E0407); args: what (a bug in cppi, not in the program).
    [[nodiscard]] static Diagnostic internal_error(SourceRange range, std::string_view what);
    /// DiagCode::ConstraintsNotSatisfied (E0238); args: function, [constraint] (C++20 concepts).
    [[nodiscard]] static Diagnostic constraints_not_satisfied(SourceRange range, std::string_view function,
                                                              std::optional<std::string> constraint);

    /// DiagCode::NoOperator (E0239); args: op, type (a class without that operator).
    [[nodiscard]] static Diagnostic no_operator(SourceRange range, std::string_view op, const std::string& type);
    /// Note after an error inside the standard library: `function` is the library
    /// function (a template instance) the player's code at `range` used.
    [[nodiscard]] static Diagnostic instantiated_from(SourceRange range, const std::string& function);

    // --- 5xx undefined behavior ----------------------------------------------
    /// Any 5xx diagnostic whose only arguments are given here.
    [[nodiscard]] static Diagnostic undefined_behavior(DiagCode code, SourceRange range,
                                                       std::vector<DiagArg> args = {});
    /// DiagCode::IntegerOverflow (E0501); args: type.
    [[nodiscard]] static Diagnostic integer_overflow(SourceRange range, std::string_view type);
    /// DiagCode::OutOfBounds (E0503); args: index, size.
    [[nodiscard]] static Diagnostic out_of_bounds(SourceRange range, std::int64_t index, std::int64_t size);
    /// DiagCode::FlowOffEnd (E0508); args: function.
    [[nodiscard]] static Diagnostic flow_off_end(SourceRange range, const std::string& function);
    /// DiagCode::InvalidShift (E0509); args: amount.
    [[nodiscard]] static Diagnostic invalid_shift(SourceRange range, std::int64_t amount);
    /// DiagCode::MemoryLeak (E0510); warning; args: count.
    [[nodiscard]] static Diagnostic memory_leak(SourceRange range, std::int64_t count);
};

}  // namespace cppi::detail
