#pragma once

/// @file Scope.hpp
/// @brief One level of name visibility: the globals, a function, a block.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/Symbol.hpp"

#include <cppi/SourceLocation.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace cppi::sema {

/// One scope: the names it declares and the namespaces it uses.
class Scope {
public:
    /// What introduces the scope.
    enum class Kind : std::uint8_t { Global, Namespace, Function, Block };

    /// @param kind The kind of scope.
    /// @param end  Where it ends in the source.
    explicit Scope(Kind kind, SourceLocation end = {}) noexcept : kind_(kind), end_(end) {}

    /// What introduces the scope.
    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    /// Where the scope ends in the source (for debuggers).
    [[nodiscard]] SourceLocation end() const noexcept { return end_; }

    /// The symbol declared here as `name`, or nullptr (no outer scopes).
    [[nodiscard]] Symbol* find(std::string_view name) {
        auto it = symbols_.find(name);
        return it == symbols_.end() ? nullptr : &it->second;
    }

    /// Adds `name`; returns nullptr if the scope already has it.
    Symbol* add(std::string name, const Symbol& symbol) {
        auto [it, inserted] = symbols_.try_emplace(std::move(name), symbol);
        return inserted ? &it->second : nullptr;
    }

    /// Every symbol declared here.
    [[nodiscard]] const std::map<std::string, Symbol, std::less<>>& symbols() const noexcept { return symbols_; }

    /// Namespaces made visible here by `using namespace`.
    [[nodiscard]] const std::vector<Scope*>& usings() const noexcept { return usings_; }
    /// `using namespace`: makes `scope` visible here (once).
    void add_using(Scope* scope) {
        if (std::find(usings_.begin(), usings_.end(), scope) == usings_.end()) {
            usings_.push_back(scope);
        }
    }

private:
    /// See kind().
    Kind kind_;
    /// See end().
    SourceLocation end_;
    /// Declared names.
    std::map<std::string, Symbol, std::less<>> symbols_;
    /// See usings().
    std::vector<Scope*> usings_;
};

}  // namespace cppi::sema
