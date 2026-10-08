#pragma once

/// @file Feature.hpp
/// The individual language features a game can lock or unlock. A construct
/// is accepted only if (1) the selected standard already includes it and
/// (2) the game has not locked it.

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
    FunctionCalls,
    Variables,
    Operators,
    Conditionals,
    Loops,
    Blocks,
    UserFunctions,
    Arrays,
    Strings,
    FloatingPoint,
    Structs,
    Classes,
    Inheritance,
    Enums,
    Pointers,
    References,
    Templates,
    Namespaces,
    Exceptions,
    Preprocessor,
    // --- C++11 -----------------------------------------------------------
    Auto,
    RangeFor,
    Lambdas,
    Nullptr,
    EnumClass,
    StaticAssert,
    Constexpr,
    TypeAliases,
    // --- C++14 -----------------------------------------------------------
    BinaryLiterals,
    DigitSeparators,
    // --- C++17 -----------------------------------------------------------
    StructuredBindings,
    IfConstexpr,
    // --- C++20 -----------------------------------------------------------
    Concepts,
    Coroutines,
    Modules,

    // --- Appended later (grouped by standard in the table, not here) ------
    DelegatingConstructors,  ///< C++11: `A() : A(0) {}`

    Count  // must stay last
};

inline constexpr std::size_t kFeatureCount = static_cast<std::size_t>(Feature::Count);

/// The first standard that contains the feature.
[[nodiscard]] Standard introduced_in(Feature feature) noexcept;

/// Stable identifier ("loops", "lambdas", ...), meant to be used as a key in
/// translation catalogs and save files.
[[nodiscard]] std::string_view feature_key(Feature feature) noexcept;

/// Inverse of feature_key(). Returns nullopt if unknown.
[[nodiscard]] std::optional<Feature> parse_feature(std::string_view key) noexcept;

}  // namespace cppi
