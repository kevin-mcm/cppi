/// @file DiagnosticFactory.cpp
/// @brief Implementation of DiagnosticFactory.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "support/DiagnosticFactory.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace cppi::detail {

namespace {

Diagnostic make(DiagCode code, SourceRange range, std::vector<DiagArg> args = {}, Severity severity = Severity::Error) {
    return Diagnostic{code, severity, range, std::move(args)};
}

std::vector<DiagArg> name_with_suggestion(const std::string& name, const std::optional<std::string>& suggestion) {
    std::vector<DiagArg> args{{"name", name}};
    if (suggestion) {
        args.push_back({"suggestion", *suggestion});
    }
    return args;
}

std::string key_of(Feature feature) {
    return std::string(feature_key(feature));
}

}  // namespace

Diagnostic DiagnosticFactory::syntax_error(SourceRange range) {
    return make(DiagCode::SyntaxError, range);
}

Diagnostic DiagnosticFactory::missing_token(SourceRange range, std::string_view token, std::uint32_t line_end) {
    if (line_end != 0) {
        return make(DiagCode::MissingToken, range, {{"token", std::string(token)}, {"line", std::int64_t{line_end}}});
    }
    return make(DiagCode::MissingToken, range, {{"token", std::string(token)}});
}

Diagnostic DiagnosticFactory::nesting_too_deep(SourceRange range, std::uint32_t limit) {
    return make(DiagCode::NestingTooDeep, range, {{"limit", std::int64_t{limit}}});
}

Diagnostic DiagnosticFactory::unknown_function(SourceRange range, const std::string& name,
                                               const std::optional<std::string>& suggestion) {
    return make(DiagCode::UnknownFunction, range, name_with_suggestion(name, suggestion));
}

Diagnostic DiagnosticFactory::unknown_identifier(SourceRange range, const std::string& name,
                                                 const std::optional<std::string>& suggestion) {
    return make(DiagCode::UnknownIdentifier, range, name_with_suggestion(name, suggestion));
}

Diagnostic DiagnosticFactory::argument_count_mismatch(SourceRange range, const std::string& function,
                                                      std::size_t expected, std::size_t actual) {
    return make(DiagCode::ArgumentCountMismatch, range,
                {{"function", function},
                 {"expected", static_cast<std::int64_t>(expected)},
                 {"actual", static_cast<std::int64_t>(actual)}});
}

Diagnostic DiagnosticFactory::argument_type_mismatch(SourceRange range, const std::string& function, std::size_t index,
                                                     const std::string& expected, const std::string& actual) {
    return make(DiagCode::ArgumentTypeMismatch, range,
                {{"function", function},
                 {"index", static_cast<std::int64_t>(index)},
                 {"expected", expected},
                 {"actual", actual}});
}

Diagnostic DiagnosticFactory::not_callable(SourceRange range, const std::string& name) {
    return make(DiagCode::NotCallable, range, {{"name", name}});
}

Diagnostic DiagnosticFactory::integer_out_of_range(SourceRange range, const std::string& literal) {
    return make(DiagCode::IntegerOutOfRange, range, {{"literal", literal}});
}

Diagnostic DiagnosticFactory::function_not_called(SourceRange range, const std::string& name, Severity severity) {
    return make(DiagCode::FunctionNotCalled, range, {{"name", name}}, severity);
}

Diagnostic DiagnosticFactory::expression_has_no_effect(SourceRange range) {
    return make(DiagCode::ExpressionHasNoEffect, range, {}, Severity::Warning);
}

Diagnostic DiagnosticFactory::redefinition(SourceRange range, const std::string& name) {
    return make(DiagCode::Redefinition, range, {{"name", name}});
}

Diagnostic DiagnosticFactory::not_assignable(SourceRange range, const std::optional<std::string>& name) {
    std::vector<DiagArg> args;
    if (name) {
        args.push_back({"name", *name});
    }
    return make(DiagCode::NotAssignable, range, std::move(args));
}

Diagnostic DiagnosticFactory::invalid_operands(SourceRange range, std::string_view op, const std::string& left,
                                               const std::optional<std::string>& right) {
    std::vector<DiagArg> args{{"op", std::string(op)}, {"left", left}};
    if (right) {
        args.push_back({"right", *right});
    }
    return make(DiagCode::InvalidOperands, range, std::move(args));
}

Diagnostic DiagnosticFactory::cannot_convert(SourceRange range, const std::string& from, const std::string& to) {
    return make(DiagCode::CannotConvert, range, {{"from", from}, {"to", to}});
}

Diagnostic DiagnosticFactory::misplaced_jump(SourceRange range, std::string_view statement) {
    return make(DiagCode::MisplacedJump, range, {{"statement", std::string(statement)}});
}

Diagnostic DiagnosticFactory::return_type_mismatch(SourceRange range, const std::string& function,
                                                   const std::string& expected, const std::string& actual) {
    return make(DiagCode::ReturnTypeMismatch, range,
                {{"function", function}, {"expected", expected}, {"actual", actual}});
}

Diagnostic DiagnosticFactory::unknown_type(SourceRange range, const std::string& name,
                                           const std::optional<std::string>& suggestion) {
    return make(DiagCode::UnknownType, range, name_with_suggestion(name, suggestion));
}

Diagnostic DiagnosticFactory::invalid_array_size(SourceRange range, std::optional<std::int64_t> size) {
    std::vector<DiagArg> args;
    if (size) {
        args.push_back({"size", *size});
    }
    return make(DiagCode::InvalidArraySize, range, std::move(args));
}

Diagnostic DiagnosticFactory::not_constant(SourceRange range) {
    return make(DiagCode::NotConstant, range);
}

Diagnostic DiagnosticFactory::duplicate_case(SourceRange range, std::int64_t value) {
    return make(DiagCode::DuplicateCase, range, {{"value", value}});
}

Diagnostic DiagnosticFactory::no_member(SourceRange range, const std::string& type, const std::string& member,
                                        const std::optional<std::string>& suggestion) {
    std::vector<DiagArg> args{{"type", type}, {"member", member}};
    if (suggestion) {
        args.push_back({"suggestion", *suggestion});
    }
    return make(DiagCode::NoMember, range, std::move(args));
}

Diagnostic DiagnosticFactory::not_subscriptable(SourceRange range, const std::string& type) {
    return make(DiagCode::NotSubscriptable, range, {{"type", type}});
}

Diagnostic DiagnosticFactory::too_many_initializers(SourceRange range, std::size_t expected, std::size_t actual) {
    return make(DiagCode::TooManyInitializers, range,
                {{"expected", static_cast<std::int64_t>(expected)}, {"actual", static_cast<std::int64_t>(actual)}});
}

Diagnostic DiagnosticFactory::uninitialized_const(SourceRange range, const std::string& name) {
    return make(DiagCode::UninitializedConst, range, {{"name", name}});
}

Diagnostic DiagnosticFactory::missing_return(SourceRange range, const std::string& function) {
    return make(DiagCode::MissingReturn, range, {{"function", function}}, Severity::Warning);
}

Diagnostic DiagnosticFactory::inaccessible_member(SourceRange range, const std::string& member,
                                                  const std::string& record, std::string_view access) {
    return make(DiagCode::InaccessibleMember, range,
                {{"member", member}, {"class", record}, {"access", std::string(access)}});
}

Diagnostic DiagnosticFactory::ambiguous_name(SourceRange range, const std::string& name) {
    return make(DiagCode::AmbiguousName, range, {{"name", name}});
}

Diagnostic DiagnosticFactory::abstract_class(SourceRange range, const std::string& record,
                                             const std::optional<std::string>& function) {
    std::vector<DiagArg> args{{"class", record}};
    if (function) {
        args.push_back({"function", *function});
    }
    return make(DiagCode::AbstractClass, range, std::move(args));
}

Diagnostic DiagnosticFactory::no_matching_function(SourceRange range, const std::string& function,
                                                   const std::string& arguments) {
    return make(DiagCode::NoMatchingFunction, range, {{"function", function}, {"arguments", arguments}});
}

Diagnostic DiagnosticFactory::ambiguous_call(SourceRange range, const std::string& function,
                                             const std::string& arguments) {
    return make(DiagCode::AmbiguousCall, range, {{"function", function}, {"arguments", arguments}});
}

Diagnostic DiagnosticFactory::static_assertion_failed(SourceRange range, const std::string& message) {
    std::vector<DiagArg> args;
    if (!message.empty()) {
        args.push_back({"message", message});
    }
    return make(DiagCode::StaticAssertionFailed, range, std::move(args));
}

Diagnostic DiagnosticFactory::assignment_in_condition(SourceRange range) {
    return make(DiagCode::AssignmentInCondition, range, {}, Severity::Warning);
}

Diagnostic DiagnosticFactory::reference_needs_lvalue(SourceRange range, const std::string& type) {
    return make(DiagCode::ReferenceNeedsLvalue, range, {{"type", type}});
}

Diagnostic DiagnosticFactory::unsupported_type(SourceRange range, const std::string& name) {
    return make(DiagCode::UnsupportedType, range, {{"name", name}});
}

Diagnostic DiagnosticFactory::declaration_not_allowed(SourceRange range, std::string_view construct) {
    return make(DiagCode::DeclarationNotAllowed, range, {{"construct", std::string(construct)}});
}

Diagnostic DiagnosticFactory::no_default_constructor(SourceRange range, const std::string& record) {
    return make(DiagCode::NoDefaultConstructor, range, {{"class", record}});
}

Diagnostic DiagnosticFactory::nothing_to_override(SourceRange range, const std::string& function) {
    return make(DiagCode::NothingToOverride, range, {{"function", function}});
}

Diagnostic DiagnosticFactory::template_deduction(SourceRange range, const std::string& function) {
    return make(DiagCode::TemplateDeduction, range, {{"function", function}});
}

Diagnostic DiagnosticFactory::undefined_function(SourceRange range, const std::string& function) {
    return make(DiagCode::UndefinedFunction, range, {{"function", function}});
}

Diagnostic DiagnosticFactory::deleted_function(SourceRange range, const std::string& function) {
    return make(DiagCode::DeletedFunction, range, {{"function", function}});
}

Diagnostic DiagnosticFactory::feature_locked(SourceRange range, Feature feature) {
    return make(DiagCode::FeatureLocked, range, {{"feature", key_of(feature)}});
}

Diagnostic DiagnosticFactory::feature_requires_standard(SourceRange range, Feature feature, Standard current) {
    return make(DiagCode::FeatureRequiresStandard, range,
                {{"feature", key_of(feature)},
                 {"required", std::string(to_string(introduced_in(feature)))},
                 {"current", std::string(to_string(current))}});
}

Diagnostic DiagnosticFactory::feature_not_implemented(SourceRange range, Feature feature) {
    return make(DiagCode::FeatureNotImplemented, range, {{"feature", key_of(feature)}});
}

Diagnostic DiagnosticFactory::unsupported_syntax(SourceRange range, const std::string& construct) {
    return make(DiagCode::UnsupportedSyntax, range, {{"construct", construct}});
}

Diagnostic DiagnosticFactory::budget_exhausted(SourceRange range, std::uint64_t budget) {
    const auto clamped =
        static_cast<std::int64_t>(std::min<std::uint64_t>(budget, std::numeric_limits<std::int64_t>::max()));
    return make(DiagCode::BudgetExhausted, range, {{"budget", clamped}});
}

Diagnostic DiagnosticFactory::host_error(SourceRange range, const std::string& function, std::uint32_t error,
                                         const std::string& detail) {
    return make(DiagCode::HostError, range,
                {{"function", function}, {"error", static_cast<std::int64_t>(error)}, {"detail", detail}});
}

Diagnostic DiagnosticFactory::stack_overflow(SourceRange range, std::uint32_t depth) {
    return make(DiagCode::StackOverflow, range, {{"depth", std::int64_t{depth}}});
}

Diagnostic DiagnosticFactory::out_of_memory(SourceRange range, std::uint64_t cells) {
    return make(DiagCode::OutOfMemory, range, {{"cells", static_cast<std::int64_t>(cells)}});
}

Diagnostic DiagnosticFactory::uncaught_exception(SourceRange range, std::string_view type,
                                                 std::optional<std::string> what) {
    std::vector<DiagArg> args{{"type", std::string(type)}};
    if (what) {
        args.push_back({"what", std::move(*what)});
    }
    return make(DiagCode::UncaughtException, range, std::move(args));
}

Diagnostic DiagnosticFactory::exception_during_unwind(SourceRange range, std::string_view type) {
    return make(DiagCode::ExceptionDuringUnwind, range, {{"type", std::string(type)}});
}

Diagnostic DiagnosticFactory::no_operator(SourceRange range, std::string_view op, const std::string& type) {
    return make(DiagCode::NoOperator, range, {{"op", std::string(op)}, {"type", type}});
}

Diagnostic DiagnosticFactory::instantiated_from(SourceRange range, const std::string& function) {
    return make(DiagCode::InstantiatedFrom, range, {{"function", function}}, Severity::Note);
}

Diagnostic DiagnosticFactory::constraints_not_satisfied(SourceRange range, std::string_view function,
                                                        std::optional<std::string> constraint) {
    std::vector<DiagArg> args{{"function", std::string(function)}};
    if (constraint) {
        args.push_back({"constraint", std::move(*constraint)});
    }
    return make(DiagCode::ConstraintsNotSatisfied, range, std::move(args));
}

Diagnostic DiagnosticFactory::no_active_exception(SourceRange range) {
    return make(DiagCode::NoActiveException, range);
}

Diagnostic DiagnosticFactory::undefined_behavior(DiagCode code, SourceRange range, std::vector<DiagArg> args) {
    return make(code, range, std::move(args));
}

Diagnostic DiagnosticFactory::integer_overflow(SourceRange range, std::string_view type) {
    return make(DiagCode::IntegerOverflow, range, {{"type", std::string(type)}});
}

Diagnostic DiagnosticFactory::out_of_bounds(SourceRange range, std::int64_t index, std::int64_t size) {
    return make(DiagCode::OutOfBounds, range, {{"index", index}, {"size", size}});
}

Diagnostic DiagnosticFactory::flow_off_end(SourceRange range, const std::string& function) {
    return make(DiagCode::FlowOffEnd, range, {{"function", function}});
}

Diagnostic DiagnosticFactory::invalid_shift(SourceRange range, std::int64_t amount) {
    return make(DiagCode::InvalidShift, range, {{"amount", amount}});
}

Diagnostic DiagnosticFactory::memory_leak(SourceRange range, std::int64_t count) {
    return make(DiagCode::MemoryLeak, range, {{"count", count}}, Severity::Warning);
}

}  // namespace cppi::detail
