#pragma once

/// One level of name visibility: the globals, a function, a block.

#include "sema/Symbol.hpp"

#include <cppi/SourceLocation.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace cppi::sema {

class Scope {
public:
    enum class Kind : std::uint8_t { Global, Namespace, Function, Block };

    explicit Scope(Kind kind, SourceLocation end = {}) noexcept : kind_(kind), end_(end) {}

    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    /// Where the scope ends in the source (for debuggers).
    [[nodiscard]] SourceLocation end() const noexcept { return end_; }

    [[nodiscard]] Symbol* find(std::string_view name) {
        auto it = symbols_.find(name);
        return it == symbols_.end() ? nullptr : &it->second;
    }

    /// Adds `name`; returns nullptr if the scope already has it.
    Symbol* add(std::string name, const Symbol& symbol) {
        auto [it, inserted] = symbols_.try_emplace(std::move(name), symbol);
        return inserted ? &it->second : nullptr;
    }

    [[nodiscard]] const std::map<std::string, Symbol, std::less<>>& symbols() const noexcept { return symbols_; }

    /// Namespaces made visible here by `using namespace`.
    [[nodiscard]] const std::vector<Scope*>& usings() const noexcept { return usings_; }
    void add_using(Scope* scope) {
        if (std::find(usings_.begin(), usings_.end(), scope) == usings_.end()) {
            usings_.push_back(scope);
        }
    }

private:
    Kind kind_;
    SourceLocation end_;
    std::map<std::string, Symbol, std::less<>> symbols_;
    std::vector<Scope*> usings_;
};

}  // namespace cppi::sema
