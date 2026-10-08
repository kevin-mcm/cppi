#pragma once

/// @file Feature.hpp
/// @brief The individual language features a game can lock or unlock.
///
/// A construct is accepted only if (1) the selected standard already includes
/// it and (2) the game has not locked it.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Standard.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace cppi {

/// Individual language features. The enumerator order is part of the ABI of
/// saved games, so new features are only ever appended before `Count`.
enum class Feature : std::uint8_t {
    // --- C++98 -----------------------------------------------------------
    /// Calling functions, e.g. `harvest();`.
    FunctionCalls,
    /// Declaring and assigning variables.
    Variables,
    /// Arithmetic, comparison and logical operators.
    Operators,
    /// `if`, `else` and `switch`.
    Conditionals,
    /// `while`, `do`/`while` and `for`.
    Loops,
    /// Compound statements `{ ... }`.
    Blocks,
    /// Functions the player defines.
    UserFunctions,
    /// Built-in arrays.
    Arrays,
    /// String literals and std::string.
    Strings,
    /// `float` and `double`.
    FloatingPoint,
    /// `struct` types.
    Structs,
    /// `class` types: access control, constructors, member functions.
    Classes,
    /// Base classes and virtual functions.
    Inheritance,
    /// Unscoped enumerations.
    Enums,
    /// Pointers, `new` and `delete`.
    Pointers,
    /// References.
    References,
    /// Function and class templates.
    Templates,
    /// Namespaces and `using`.
    Namespaces,
    /// `throw`, `try` and `catch`.
    Exceptions,
    /// Preprocessor directives.
    Preprocessor,
    // --- C++11 -----------------------------------------------------------
    /// `auto` type deduction.
    Auto,
    /// Range-based `for`.
    RangeFor,
    /// Lambda expressions.
    Lambdas,
    /// The `nullptr` literal.
    Nullptr,
    /// Scoped enumerations (`enum class`).
    EnumClass,
    /// `static_assert`.
    StaticAssert,
    /// `constexpr` variables and functions.
    Constexpr,
    /// `using Name = Type;`.
    TypeAliases,
    // --- C++14 -----------------------------------------------------------
    /// Binary integer literals: `0b1010`.
    BinaryLiterals,
    /// Digit separators: `1'000'000`.
    DigitSeparators,
    // --- C++17 -----------------------------------------------------------
    /// `auto [x, y] = ...;`.
    StructuredBindings,
    /// `if constexpr`.
    IfConstexpr,
    // --- C++20 -----------------------------------------------------------
    /// Concepts and `requires` clauses.
    Concepts,
    /// Coroutines (`co_await`, `co_yield`, `co_return`).
    Coroutines,
    /// Modules (`import`, `export`).
    Modules,

    // --- Appended later (grouped by standard in the table, not here) ------
    DelegatingConstructors,  ///< C++11: `A() : A(0) {}`

    Count  // must stay last
};

/// Number of features (the enumerators before Feature::Count).
inline constexpr std::size_t kFeatureCount = static_cast<std::size_t>(Feature::Count);

/// The first standard that contains the feature.
/// @param feature The feature to look up.
[[nodiscard]] Standard introduced_in(Feature feature) noexcept;

/// Stable identifier ("loops", "lambdas", ...), meant to be used as a key in
/// translation catalogs and save files.
/// @param feature The feature to look up.
[[nodiscard]] std::string_view feature_key(Feature feature) noexcept;

/// Inverse of feature_key(). Returns nullopt if unknown.
/// @param key A key returned by feature_key().
[[nodiscard]] std::optional<Feature> parse_feature(std::string_view key) noexcept;

}  // namespace cppi
