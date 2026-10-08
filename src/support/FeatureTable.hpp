#pragma once

/// The single source of truth for every language feature: its stable key and
/// the standard that introduced it. Shared by Feature.cpp and FeatureSet.cpp.

#include <cppi/Feature.hpp>
#include <cppi/Standard.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace cppi::detail {

struct FeatureRow {
    Feature feature;
    std::string_view key;
    Standard since;
};

// Order must match the Feature enum (checked below).
inline constexpr std::array<FeatureRow, kFeatureCount> kFeatureTable{{
    {Feature::FunctionCalls, "function-calls", Standard::Cpp98},
    {Feature::Variables, "variables", Standard::Cpp98},
    {Feature::Operators, "operators", Standard::Cpp98},
    {Feature::Conditionals, "conditionals", Standard::Cpp98},
    {Feature::Loops, "loops", Standard::Cpp98},
    {Feature::Blocks, "blocks", Standard::Cpp98},
    {Feature::UserFunctions, "user-functions", Standard::Cpp98},
    {Feature::Arrays, "arrays", Standard::Cpp98},
    {Feature::Strings, "strings", Standard::Cpp98},
    {Feature::FloatingPoint, "floating-point", Standard::Cpp98},
    {Feature::Structs, "structs", Standard::Cpp98},
    {Feature::Classes, "classes", Standard::Cpp98},
    {Feature::Inheritance, "inheritance", Standard::Cpp98},
    {Feature::Enums, "enums", Standard::Cpp98},
    {Feature::Pointers, "pointers", Standard::Cpp98},
    {Feature::References, "references", Standard::Cpp98},
    {Feature::Templates, "templates", Standard::Cpp98},
    {Feature::Namespaces, "namespaces", Standard::Cpp98},
    {Feature::Exceptions, "exceptions", Standard::Cpp98},
    {Feature::Preprocessor, "preprocessor", Standard::Cpp98},
    {Feature::Auto, "auto", Standard::Cpp11},
    {Feature::RangeFor, "range-for", Standard::Cpp11},
    {Feature::Lambdas, "lambdas", Standard::Cpp11},
    {Feature::Nullptr, "nullptr", Standard::Cpp11},
    {Feature::EnumClass, "enum-class", Standard::Cpp11},
    {Feature::StaticAssert, "static-assert", Standard::Cpp11},
    {Feature::Constexpr, "constexpr", Standard::Cpp11},
    {Feature::TypeAliases, "type-aliases", Standard::Cpp11},
    {Feature::BinaryLiterals, "binary-literals", Standard::Cpp14},
    {Feature::DigitSeparators, "digit-separators", Standard::Cpp14},
    {Feature::StructuredBindings, "structured-bindings", Standard::Cpp17},
    {Feature::IfConstexpr, "if-constexpr", Standard::Cpp17},
    {Feature::Concepts, "concepts", Standard::Cpp20},
    {Feature::Coroutines, "coroutines", Standard::Cpp20},
    {Feature::Modules, "modules", Standard::Cpp20},
    {Feature::DelegatingConstructors, "delegating-constructors", Standard::Cpp11},
}};

constexpr bool feature_table_is_ordered() {
    for (std::size_t i = 0; i < kFeatureTable.size(); ++i) {
        if (static_cast<std::size_t>(kFeatureTable[i].feature) != i) {
            return false;
        }
    }
    return true;
}
static_assert(feature_table_is_ordered(), "kFeatureTable must follow the order of the Feature enum");

constexpr const FeatureRow& feature_row(Feature f) noexcept {
    return kFeatureTable[static_cast<std::size_t>(f)];
}

}  // namespace cppi::detail
