#pragma once

/// @file DiagCode.hpp
/// @brief Stable diagnostic codes.
///
/// They are a public contract: never renumbered or reused (see
/// docs/diagnostics.md).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>
#include <string>
#include <string_view>

namespace cppi {

/// Diagnostic codes. Numeric values are stable and grouped by phase:
///   1xx syntax, 2xx semantic, 3xx language features, 4xx runtime,
///   5xx undefined behavior caught at runtime.
enum class DiagCode : std::uint16_t {
    // Syntax ------------------------------------------------------------
    SyntaxError = 100,     ///< args: none
    MissingToken = 101,    ///< args: token
    NestingTooDeep = 102,  ///< args: limit
    // Semantic ----------------------------------------------------------
    UnknownFunction = 200,          ///< args: name, [suggestion]
    UnknownIdentifier = 201,        ///< args: name, [suggestion]
    ArgumentCountMismatch = 202,    ///< args: function, expected, actual
    ArgumentTypeMismatch = 203,     ///< args: function, index (1-based), expected, actual
    NotCallable = 204,              ///< args: name
    IntegerOutOfRange = 205,        ///< args: literal
    FunctionNotCalled = 206,        ///< args: name (a function used without `()`)
    ExpressionHasNoEffect = 207,    ///< warning; args: none
    Redefinition = 208,             ///< args: name
    NotAssignable = 209,            ///< args: [name]
    InvalidOperands = 210,          ///< args: op, left, [right]
    CannotConvert = 211,            ///< args: from, to
    MisplacedJump = 212,            ///< args: statement ("break", "continue")
    ReturnTypeMismatch = 213,       ///< args: function, expected, actual
    UnknownType = 214,              ///< args: name, [suggestion]
    InvalidArraySize = 215,         ///< args: [size]
    NotConstant = 216,              ///< args: none
    DuplicateCase = 217,            ///< args: value
    NoMember = 218,                 ///< args: type, member, [suggestion]
    NotSubscriptable = 219,         ///< args: type
    TooManyInitializers = 220,      ///< args: expected, actual
    UninitializedConst = 221,       ///< args: name
    MissingReturn = 222,            ///< warning; args: function
    InaccessibleMember = 223,       ///< args: member, class, access
    AmbiguousName = 224,            ///< args: name
    AbstractClass = 225,            ///< args: class, [function]
    NoMatchingFunction = 226,       ///< args: function, arguments
    StaticAssertionFailed = 227,    ///< args: [message]
    AssignmentInCondition = 228,    ///< warning; args: none
    ReferenceNeedsLvalue = 229,     ///< args: type
    UnsupportedType = 230,          ///< args: name
    DeclarationNotAllowed = 231,    ///< args: construct
    NoDefaultConstructor = 232,     ///< args: class
    NothingToOverride = 233,        ///< args: function
    TemplateDeduction = 234,        ///< args: function
    AmbiguousCall = 235,            ///< args: function, arguments
    UndefinedFunction = 236,        ///< args: function (declared, called, never defined)
    DeletedFunction = 237,          ///< args: function
    ConstraintsNotSatisfied = 238,  ///< args: function, [constraint] (C++20 concepts)
    NoOperator = 239,               ///< args: op, type (a class without that operator)
    InstantiatedFrom = 240,         ///< note; args: function (the library code the error above is in)
    // Language features -------------------------------------------------
    FeatureLocked = 300,            ///< args: feature
    FeatureRequiresStandard = 301,  ///< args: feature, required, current
    FeatureNotImplemented = 302,    ///< args: feature
    UnsupportedSyntax = 303,        ///< args: construct
    // Runtime -----------------------------------------------------------
    BudgetExhausted = 400,        ///< args: budget
    HostError = 401,              ///< args: function, error (host-defined code), detail
    StackOverflow = 402,          ///< args: depth
    OutOfMemory = 403,            ///< args: cells
    UncaughtException = 404,      ///< args: type, [what] (std::exception's message)
    ExceptionDuringUnwind = 405,  ///< args: type (thrown while another exception was unwinding)
    NoActiveException = 406,      ///< args: none (`throw;` outside a handler)
    InternalError = 407,          ///< args: what (a bug in cppi, not in the program)
    // Undefined behavior ----------------------------------------------------
    DivisionByZero = 500,     ///< args: none
    IntegerOverflow = 501,    ///< args: type
    UninitializedRead = 502,  ///< args: [name]
    OutOfBounds = 503,        ///< args: index, size
    NullDereference = 504,    ///< args: none
    UseAfterFree = 505,       ///< args: none
    DoubleFree = 506,         ///< args: none
    InvalidDelete = 507,      ///< args: none
    FlowOffEnd = 508,         ///< args: function
    InvalidShift = 509,       ///< args: amount
    MemoryLeak = 510,         ///< warning; args: count
    PureVirtualCall = 511,    ///< args: function
    InvalidPointer = 512,     ///< args: none
    BadAccess = 513,          ///< args: what ("optional")
};

/// True for codes in the 5xx group.
/// @param code The diagnostic code to classify.
[[nodiscard]] constexpr bool is_undefined_behavior(DiagCode code) noexcept {
    return static_cast<unsigned>(code) >= 500 && static_cast<unsigned>(code) < 600;
}

/// Stable string key for a code, e.g. "unknown-function". Use it as the key
/// in translation catalogs.
/// @param code The diagnostic code.
/// @return Its kebab-case key.
[[nodiscard]] std::string_view diag_key(DiagCode code) noexcept;

/// "E0200"-style identifier, handy in error output and documentation.
/// @param code The diagnostic code.
/// @return Its identifier, "E" followed by four digits.
[[nodiscard]] std::string diag_id(DiagCode code);

}  // namespace cppi
