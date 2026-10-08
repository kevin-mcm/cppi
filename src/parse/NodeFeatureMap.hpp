#pragma once

/// @file NodeFeatureMap.hpp
/// @brief Maps tree-sitter-cpp node kinds to language features.
///
/// This table is what lets the interpreter say "loops are locked" or "lambdas
/// need C++11" instead of a generic syntax error.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Feature.hpp>

#include <tree_sitter/api.h>

#include <optional>
#include <string_view>

namespace cppi::parse {

/// The language feature each tree-sitter-cpp node kind stands for.
class NodeFeatureMap {
public:
    /// The feature a node *introduces*, or nullopt for neutral nodes
    /// (identifiers, punctuation, argument lists...).
    [[nodiscard]] static std::optional<Feature> feature_of(TSNode node, std::string_view source) noexcept;

    /// Feature required by a numeric literal's spelling: floating point, binary
    /// literals (C++14) or digit separators (C++14). nullopt for plain integers.
    [[nodiscard]] static std::optional<Feature> number_literal_feature(std::string_view text) noexcept;
};

}  // namespace cppi::parse
