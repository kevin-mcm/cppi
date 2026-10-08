#include "EnglishCatalog.hpp"

#include "MessageTable.hpp"

namespace cppi_run {

namespace {

constexpr MessageEntry kDiagnostics[] = {
    {"syntax-error", "this code is not valid C++"},
    {"missing-token", "expected '{token}' here"},
    {"missing-token-at-line-end", "expected '{token}' at the end of line {line}"},
    {"nesting-too-deep", "expressions are nested more than {limit} levels deep"},
    {"unknown-function", "there is no function called '{name}'"},
    {"unknown-identifier", "'{name}' has not been declared"},
    {"argument-count-mismatch", "'{function}' takes {expected} argument(s), but {actual} were given"},
    {"argument-type-mismatch", "argument {index} of '{function}' must be '{expected}', not '{actual}'"},
    {"not-callable", "'{name}' is not a function and cannot be called"},
    {"integer-out-of-range", "the number {literal} is too large"},
    {"function-not-called", "'{name}' is a function: add () to call it"},
    {"expression-has-no-effect", "this statement has no effect"},
    {"feature-locked", "you haven't unlocked {feature} yet"},
    {"feature-requires-standard", "using {feature} requires {required} (you are using {current})"},
    {"feature-not-implemented", "{feature}: not available in this version of the game yet"},
    {"unsupported-syntax", "this construct ({construct}) is not supported"},
    {"budget-exhausted", "out of operations: the budget of {budget} was used up"},
    {"host-error", "'{function}' failed: {detail}"},
    {"redefinition", "'{name}' is already declared"},
    {"not-assignable", "this cannot be modified[: '{name}' is constant]"},
    {"invalid-operands", "invalid operands for {op}: '{left}'[ and '{right}']"},
    {"cannot-convert", "cannot convert '{from}' to '{to}'"},
    {"misplaced-jump", "'{statement}' is not inside a loop or switch"},
    {"return-type-mismatch", "'{function}' must return '{expected}', not '{actual}'"},
    {"unknown-type", "unknown type '{name}'"},
    {"invalid-array-size", "invalid array size[ {size}]"},
    {"not-constant", "this must be a constant known before running"},
    {"duplicate-case", "the case {value} appears twice"},
    {"no-member", "'{type}' has no member named '{member}'"},
    {"not-subscriptable", "'{type}' cannot be indexed with []"},
    {"too-many-initializers", "too many initializers: at most {expected}, but there are {actual}"},
    {"uninitialized-const", "'{name}' must be initialized when it is declared"},
    {"missing-return", "'{function}' may end without returning a value"},
    {"inaccessible-member", "'{member}' is {access} in '{class}'"},
    {"ambiguous-name", "'{name}' is ambiguous: it is inherited more than once"},
    {"abstract-class", "'{class}' is abstract[: '{function}' has no implementation]"},
    {"no-matching-function", "no version of '{function}' accepts ({arguments})"},
    {"static-assertion-failed", "static_assert failed[: {message}]"},
    {"assignment-in-condition", "'=' assigns a value; to compare use '=='"},
    {"reference-needs-lvalue", "a '{type}' must refer to a variable"},
    {"unsupported-type", "the type '{name}' is not supported"},
    {"declaration-not-allowed", "{construct}: not allowed here"},
    {"no-default-constructor", "'{class}' has no constructor without arguments"},
    {"nothing-to-override", "'{function}' is marked override but overrides nothing"},
    {"template-deduction", "cannot deduce the template arguments of '{function}'"},
    {"ambiguous-call", "the call to '{function}' with ({arguments}) is ambiguous"},
    {"undefined-function", "'{function}' is declared but never defined"},
    {"deleted-function", "'{function}' is deleted: it cannot be used"},
    {"no-operator", "there is no operator{op} for '{type}'"},
    {"instantiated-from", "in '{function}', used here"},
    {"constraints-not-satisfied", "the arguments do not satisfy the constraints of '{function}'[: {constraint}]"},
    {"stack-overflow", "stack overflow: more than {depth} nested calls"},
    {"out-of-memory", "out of memory"},
    {"uncaught-exception", "uncaught exception of type '{type}'[: {what}]"},
    {"exception-during-unwind", "an exception of type '{type}' was thrown while another one was being handled"},
    {"no-active-exception", "'throw;' with no exception being handled"},
    {"division-by-zero", "undefined behavior: division by zero"},
    {"integer-overflow", "undefined behavior: the result does not fit in '{type}'"},
    {"uninitialized-read", "undefined behavior: reading a variable that has no value yet[ ('{name}')]"},
    {"out-of-bounds", "undefined behavior: index {index} is out of bounds (size {size})"},
    {"null-dereference", "undefined behavior: using a null pointer"},
    {"use-after-free", "undefined behavior: using memory after delete"},
    {"double-free", "undefined behavior: deleting the same memory twice"},
    {"invalid-delete", "undefined behavior: delete of memory that did not come from the matching new"},
    {"flow-off-end", "undefined behavior: '{function}' ended without returning a value"},
    {"invalid-shift", "undefined behavior: shifting by {amount} bits"},
    {"memory-leak", "memory leak: {count} block(s) created with new were never deleted"},
    {"pure-virtual-call", "undefined behavior: calling the pure virtual function '{function}'"},
    {"invalid-pointer", "undefined behavior: invalid pointer access"},
    {"bad-access", "undefined behavior: reading an empty {what}"},
};

constexpr MessageEntry kFeatures[] = {
    {"function-calls", "function calls"},
    {"variables", "variables"},
    {"operators", "operators"},
    {"conditionals", "conditionals"},
    {"loops", "loops"},
    {"blocks", "blocks"},
    {"user-functions", "your own functions"},
    {"arrays", "arrays"},
    {"strings", "strings"},
    {"floating-point", "decimal numbers"},
    {"structs", "structs"},
    {"classes", "classes"},
    {"inheritance", "inheritance"},
    {"enums", "enums"},
    {"pointers", "pointers"},
    {"references", "references"},
    {"templates", "templates"},
    {"namespaces", "namespaces"},
    {"exceptions", "exceptions"},
    {"preprocessor", "preprocessor directives"},
    {"auto", "'auto' type deduction"},
    {"range-for", "range-based for loops"},
    {"lambdas", "lambdas"},
    {"nullptr", "nullptr"},
    {"enum-class", "scoped enums (enum class)"},
    {"static-assert", "static_assert"},
    {"constexpr", "constexpr"},
    {"type-aliases", "type aliases (using)"},
    {"binary-literals", "binary literals"},
    {"digit-separators", "digit separators"},
    {"structured-bindings", "structured bindings"},
    {"if-constexpr", "if constexpr"},
    {"concepts", "concepts"},
    {"coroutines", "coroutines"},
    {"modules", "modules"},
    {"delegating-constructors", "delegating constructors"},
};

constexpr MessageEntry kUi[] = {
    {"initial-world", "Initial world"}, {"output", "Output"},
    {"final-world", "Final world"},     {"result", "Result"},
    {"operations", "Operations"},       {"hay", "Hay"},
    {"position", "Position"},           {"did-you-mean", "did you mean '{suggestion}'?"},
    {"bytecode", "Bytecode"},           {"cannot-read", "cannot read file"},
};

}  // namespace

std::optional<std::string_view> EnglishCatalog::diagnostic(std::string_view key) const {
    return MessageTable(kDiagnostics).find(key);
}

std::optional<std::string_view> EnglishCatalog::feature(std::string_view key) const {
    return MessageTable(kFeatures).find(key);
}

std::optional<std::string_view> EnglishCatalog::ui(std::string_view key) const {
    return MessageTable(kUi).find(key);
}

std::string_view EnglishCatalog::severity(cppi::Severity severity) const {
    switch (severity) {
        case cppi::Severity::Error: return "error";
        case cppi::Severity::Warning: return "warning";
        case cppi::Severity::Note: return "note";
    }
    return {};
}

std::string_view EnglishCatalog::status(cppi::RunStatus status) const {
    switch (status) {
        case cppi::RunStatus::Running: return "running";
        case cppi::RunStatus::Completed: return "completed";
        case cppi::RunStatus::BudgetExhausted: return "out of operations";
        case cppi::RunStatus::HostError: return "stopped by an error";
        case cppi::RunStatus::Paused: return "paused";
        case cppi::RunStatus::RuntimeError: return "stopped by a runtime error";
    }
    return {};
}

}  // namespace cppi_run
