#pragma once

/// Entry point of the parse module. tree-sitter types never leave src/parse:
/// this header only forward-declares TSParser.

#include "parse/ParseResult.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

struct TSParser;
struct TSTree;

namespace cppi::parse {

class Parser {
public:
    Parser();
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
    struct TreeDeleter {
        void operator()(TSTree* tree) const noexcept;
    };
    using Tree = std::unique_ptr<TSTree, TreeDeleter>;

    /// tree-sitter's reading of `source` (null if it is too long or tree-sitter fails).
    [[nodiscard]] Tree parse_tree(std::string_view source);
    /// One parse from `tree` (parse_tree(source)): syntax errors, or (when `convert`) the AST.
    [[nodiscard]] ParseResult parse_once(std::string_view source, const TSTree* tree, bool convert);
    /// parse_once, then, while it fails, the forgotten ';', ')' or '}' that
    /// makes it parse further (see find_missing_token). `depth` counts repairs.
    [[nodiscard]] ParseResult parse_repairing(std::string_view source, std::size_t depth);

    struct Repair {
        std::uint32_t offset = 0;  ///< byte where the token goes
        std::string_view token;
    };
    /// tree-sitter often reports a forgotten ';' as an ERROR further on, or
    /// puts its MISSING node after the next token. Tries inserting ';', ')'
    /// or '}' at the token boundaries before the end of the first error,
    /// nearest first (line ends before the middle of lines), and keeps the
    /// first insertion that moves the first syntax error past that error.
    [[nodiscard]] std::optional<Repair> find_missing_token(std::string_view source, const TSTree* tree,
                                                           const Diagnostic& first_error);

    struct Deleter {
        void operator()(TSParser* parser) const noexcept;
    };
    std::unique_ptr<TSParser, Deleter> parser_;
    std::size_t repair_budget_ = 0;
};

}  // namespace cppi::parse
