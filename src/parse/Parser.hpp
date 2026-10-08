#pragma once

/// @file Parser.hpp
/// @brief Entry point of the parse module.
///
/// tree-sitter types never leave src/parse: this header only forward-declares
/// TSParser.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "parse/ParseResult.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

/// tree-sitter's parser (opaque).
struct TSParser;
/// tree-sitter's syntax tree (opaque).
struct TSTree;

namespace cppi::parse {

/// Turns player source text into an AST with tree-sitter-cpp, reporting
/// syntax errors precisely (including forgotten tokens). Move-only; owns a
/// tree-sitter parser.
class Parser {
public:
    /// Creates a tree-sitter parser for C++.
    Parser();
    /// Releases the tree-sitter parser.
    ~Parser();
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    Parser(Parser&&) noexcept = default;
    Parser& operator=(Parser&&) noexcept = default;

    /// Parses player code in "script mode": a sequence of top-level
    /// statements (see docs/adr/0006-script-mode.md).
    [[nodiscard]] ParseResult parse(std::string_view source);

    /// Cap on reported syntax errors, to avoid flooding beginners.
    static constexpr std::size_t kMaxSyntaxErrors = 10;
    /// Forgotten tokens reported in one program (later errors are reported as found).
    static constexpr std::size_t kMaxRepairs = 3;
    /// Insertions find_missing_token tries for each token and error (each is a re-parse).
    static constexpr std::size_t kMaxRepairTries = 24;
    /// Bytes all those re-parses may read for one program: long garbage gets few tries.
    static constexpr std::size_t kRepairBudget = std::size_t{256} << 10;

private:
    /// Frees a tree-sitter tree.
    struct TreeDeleter {
        /// Calls ts_tree_delete().
        void operator()(TSTree* tree) const noexcept;
    };
    /// Owning pointer to a tree-sitter tree.
    using Tree = std::unique_ptr<TSTree, TreeDeleter>;

    /// tree-sitter's reading of `source` (null if it is too long or tree-sitter fails).
    [[nodiscard]] Tree parse_tree(std::string_view source);
    /// One parse from `tree` (parse_tree(source)): syntax errors, or (when `convert`) the AST.
    [[nodiscard]] ParseResult parse_once(std::string_view source, const TSTree* tree, bool convert);
    /// parse_once, then, while it fails, the forgotten ';', ')' or '}' that
    /// makes it parse further (see find_missing_token). `depth` counts repairs.
    [[nodiscard]] ParseResult parse_repairing(std::string_view source, std::size_t depth);

    /// A token to insert so the program parses further.
    struct Repair {
        std::uint32_t offset = 0;  ///< byte where the token goes
        /// The token: ";", ")" or "}".
        std::string_view token;
    };
    /// tree-sitter often reports a forgotten ';' as an ERROR further on, or
    /// puts its MISSING node after the next token. Tries inserting ';', ')'
    /// or '}' at the token boundaries before the end of the first error,
    /// nearest first (line ends before the middle of lines), and keeps the
    /// first insertion that moves the first syntax error past that error.
    [[nodiscard]] std::optional<Repair> find_missing_token(std::string_view source, const TSTree* tree,
                                                           const Diagnostic& first_error);

    /// Frees a tree-sitter parser.
    struct Deleter {
        /// Calls ts_parser_delete().
        void operator()(TSParser* parser) const noexcept;
    };
    /// The tree-sitter parser.
    std::unique_ptr<TSParser, Deleter> parser_;
    /// Bytes re-parses may still read for the current program (see kRepairBudget).
    std::size_t repair_budget_ = 0;
};

}  // namespace cppi::parse
