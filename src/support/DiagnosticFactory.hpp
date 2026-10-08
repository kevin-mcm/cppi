#pragma once

/// Factory for every diagnostic the interpreter can emit. Argument names and
/// their order are part of the public contract (docs/diagnostics.md), so they
/// are spelled out in exactly one place.

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

class DiagnosticFactory {
public:
    // --- 1xx syntax -------------------------------------------------------
    [[nodiscard]] static Diagnostic syntax_error(SourceRange range);
    /// `line_end`: the line whose end the token is missing from (0 if it goes mid-line).
    [[nodiscard]] static Diagnostic missing_token(SourceRange range, std::string_view token,
                                                  std::uint32_t line_end = 0);
    [[nodiscard]] static Diagnostic nesting_too_deep(SourceRange range, std::uint32_t limit);

    // --- 2xx semantic -----------------------------------------------------
    [[nodiscard]] static Diagnostic unknown_function(SourceRange range, const std::string& name,
                                                     const std::optional<std::string>& suggestion);
    [[nodiscard]] static Diagnostic unknown_identifier(SourceRange range, const std::string& name,
                                                       const std::optional<std::string>& suggestion);
    [[nodiscard]] static Diagnostic argument_count_mismatch(SourceRange range, const std::string& function,
                                                            std::size_t expected, std::size_t actual);
    [[nodiscard]] static Diagnostic argument_type_mismatch(SourceRange range, const std::string& function,
                                                           std::size_t index, const std::string& expected,
                                                           const std::string& actual);
    [[nodiscard]] static Diagnostic not_callable(SourceRange range, const std::string& name);
    [[nodiscard]] static Diagnostic integer_out_of_range(SourceRange range, const std::string& literal);
    [[nodiscard]] static Diagnostic function_not_called(SourceRange range, const std::string& name, Severity severity);
    [[nodiscard]] static Diagnostic expression_has_no_effect(SourceRange range);
    [[nodiscard]] static Diagnostic redefinition(SourceRange range, const std::string& name);
    [[nodiscard]] static Diagnostic not_assignable(SourceRange range, const std::optional<std::string>& name);
    [[nodiscard]] static Diagnostic invalid_operands(SourceRange range, std::string_view op, const std::string& left,
                                                     const std::optional<std::string>& right);
    [[nodiscard]] static Diagnostic cannot_convert(SourceRange range, const std::string& from, const std::string& to);
    [[nodiscard]] static Diagnostic misplaced_jump(SourceRange range, std::string_view statement);
    [[nodiscard]] static Diagnostic return_type_mismatch(SourceRange range, const std::string& function,
                                                         const std::string& expected, const std::string& actual);
    [[nodiscard]] static Diagnostic unknown_type(SourceRange range, const std::string& name,
                                                 const std::optional<std::string>& suggestion);
    [[nodiscard]] static Diagnostic invalid_array_size(SourceRange range, std::optional<std::int64_t> size);
    [[nodiscard]] static Diagnostic not_constant(SourceRange range);
    [[nodiscard]] static Diagnostic duplicate_case(SourceRange range, std::int64_t value);
    [[nodiscard]] static Diagnostic no_member(SourceRange range, const std::string& type, const std::string& member,
                                              const std::optional<std::string>& suggestion);
    [[nodiscard]] static Diagnostic not_subscriptable(SourceRange range, const std::string& type);
    [[nodiscard]] static Diagnostic too_many_initializers(SourceRange range, std::size_t expected, std::size_t actual);
    [[nodiscard]] static Diagnostic uninitialized_const(SourceRange range, const std::string& name);
    [[nodiscard]] static Diagnostic missing_return(SourceRange range, const std::string& function);
    [[nodiscard]] static Diagnostic inaccessible_member(SourceRange range, const std::string& member,
                                                        const std::string& record, std::string_view access);
    [[nodiscard]] static Diagnostic ambiguous_name(SourceRange range, const std::string& name);
    [[nodiscard]] static Diagnostic abstract_class(SourceRange range, const std::string& record,
                                                   const std::optional<std::string>& function);
    [[nodiscard]] static Diagnostic no_matching_function(SourceRange range, const std::string& function,
                                                         const std::string& arguments);
    [[nodiscard]] static Diagnostic ambiguous_call(SourceRange range, const std::string& function,
                                                   const std::string& arguments);
    [[nodiscard]] static Diagnostic static_assertion_failed(SourceRange range, const std::string& message);
    [[nodiscard]] static Diagnostic assignment_in_condition(SourceRange range);
    [[nodiscard]] static Diagnostic reference_needs_lvalue(SourceRange range, const std::string& type);
    [[nodiscard]] static Diagnostic unsupported_type(SourceRange range, const std::string& name);
    [[nodiscard]] static Diagnostic declaration_not_allowed(SourceRange range, std::string_view construct);
    [[nodiscard]] static Diagnostic no_default_constructor(SourceRange range, const std::string& record);
    [[nodiscard]] static Diagnostic nothing_to_override(SourceRange range, const std::string& function);
    [[nodiscard]] static Diagnostic template_deduction(SourceRange range, const std::string& function);
    [[nodiscard]] static Diagnostic undefined_function(SourceRange range, const std::string& function);
    [[nodiscard]] static Diagnostic deleted_function(SourceRange range, const std::string& function);

    // --- 3xx language features ---------------------------------------------
    [[nodiscard]] static Diagnostic feature_locked(SourceRange range, Feature feature);
    [[nodiscard]] static Diagnostic feature_requires_standard(SourceRange range, Feature feature, Standard current);
    [[nodiscard]] static Diagnostic feature_not_implemented(SourceRange range, Feature feature);
    [[nodiscard]] static Diagnostic unsupported_syntax(SourceRange range, const std::string& construct);

    // --- 4xx runtime --------------------------------------------------------
    [[nodiscard]] static Diagnostic budget_exhausted(SourceRange range, std::uint64_t budget);
    [[nodiscard]] static Diagnostic host_error(SourceRange range, const std::string& function, std::uint32_t error,
                                               const std::string& detail);
    [[nodiscard]] static Diagnostic stack_overflow(SourceRange range, std::uint32_t depth);
    [[nodiscard]] static Diagnostic out_of_memory(SourceRange range, std::uint64_t cells);
    [[nodiscard]] static Diagnostic uncaught_exception(SourceRange range, std::string_view type,
                                                       std::optional<std::string> what);
    [[nodiscard]] static Diagnostic exception_during_unwind(SourceRange range, std::string_view type);
    [[nodiscard]] static Diagnostic no_active_exception(SourceRange range);
    [[nodiscard]] static Diagnostic constraints_not_satisfied(SourceRange range, std::string_view function,
                                                              std::optional<std::string> constraint);

    [[nodiscard]] static Diagnostic no_operator(SourceRange range, std::string_view op, const std::string& type);
    /// Note after an error inside the standard library: `function` is the library
    /// function (a template instance) the player's code at `range` used.
    [[nodiscard]] static Diagnostic instantiated_from(SourceRange range, const std::string& function);

    // --- 5xx undefined behavior ----------------------------------------------
    /// Any 5xx diagnostic whose only arguments are given here.
    [[nodiscard]] static Diagnostic undefined_behavior(DiagCode code, SourceRange range,
                                                       std::vector<DiagArg> args = {});
    [[nodiscard]] static Diagnostic integer_overflow(SourceRange range, std::string_view type);
    [[nodiscard]] static Diagnostic out_of_bounds(SourceRange range, std::int64_t index, std::int64_t size);
    [[nodiscard]] static Diagnostic flow_off_end(SourceRange range, const std::string& function);
    [[nodiscard]] static Diagnostic invalid_shift(SourceRange range, std::int64_t amount);
    [[nodiscard]] static Diagnostic memory_leak(SourceRange range, std::int64_t count);
};

}  // namespace cppi::detail
