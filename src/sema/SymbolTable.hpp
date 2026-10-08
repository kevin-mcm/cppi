#pragma once

/// @file SymbolTable.hpp
/// @brief The stack of scopes visible at the current point of the analysis: the
/// globals, the namespaces being defined, then function and block scopes.
///
/// Namespaces outlive the code that defines them (they can be reopened and
/// named from anywhere).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/Scope.hpp"
#include "sema/Symbol.hpp"

#include <cppi/SourceLocation.hpp>

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppi::sema {

/// The stack of visible scopes, plus the namespaces and class scopes that
/// outlive it.
class SymbolTable {
public:
    /// Starts with the global scope.
    SymbolTable() {
        owned_.emplace_back(Scope::Kind::Global);
        stack_.push_back(&owned_.back());
    }
    SymbolTable(const SymbolTable&) = delete;
    SymbolTable& operator=(const SymbolTable&) = delete;
    SymbolTable(SymbolTable&&) = delete;
    SymbolTable& operator=(SymbolTable&&) = delete;
    ~SymbolTable() = default;

    /// A new function or block scope.
    void push(Scope::Kind kind, SourceLocation end = {}) {
        owned_.emplace_back(kind, end);
        stack_.push_back(&owned_.back());
    }
    /// Closes the innermost scope.
    void pop() {
        if (!owned_.empty() && stack_.back() == &owned_.back()) {
            owned_.pop_back();
        }
        stack_.pop_back();
    }

    /// Opens (creating it the first time) namespace `name` inside the current scope.
    void enter_namespace(const std::string& name, SourceRange range);
    /// Closes the namespace (or scope) opened last.
    void leave_namespace() { stack_.pop_back(); }

    /// Makes an existing namespace-like scope the current one (undo with leave_namespace()).
    void enter_scope(Scope* scope) { stack_.push_back(scope); }

    /// A scope for a class's static members (`Counter::count`), owned here.
    [[nodiscard]] Scope* new_class_scope() {
        namespaces_.emplace_back(Scope::Kind::Namespace);
        return &namespaces_.back();
    }

    /// The innermost scope.
    [[nodiscard]] Scope& current() { return *stack_.back(); }
    /// The innermost global or namespace scope.
    [[nodiscard]] Scope& enclosing_namespace() {
        for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
            if ((*it)->kind() == Scope::Kind::Global || (*it)->kind() == Scope::Kind::Namespace) {
                return **it;
            }
        }
        return *stack_.front();
    }
    /// The global scope.
    [[nodiscard]] Scope& global() { return *stack_.front(); }
    /// True outside any function: declarations are globals.
    [[nodiscard]] bool at_global_scope() const noexcept {
        return stack_.back()->kind() == Scope::Kind::Global || stack_.back()->kind() == Scope::Kind::Namespace;
    }

    /// Innermost declaration of `name` (namespaces opened with `using` included).
    [[nodiscard]] Symbol* lookup(std::string_view name);
    /// Like lookup(), but only function and block scopes (class members come
    /// between local and namespace names).
    [[nodiscard]] Symbol* lookup_local(std::string_view name);
    /// `a::b::name`: `scope` names namespaces or classes (their static members).
    [[nodiscard]] Symbol* lookup_qualified(const std::vector<std::string>& scope, std::string_view name);
    /// The namespace named by `scope`, or nullptr.
    [[nodiscard]] Scope* find_namespace(const std::vector<std::string>& scope);

    /// The current chain of scopes (to come back to it later).
    [[nodiscard]] std::vector<Scope*> snapshot() const { return stack_; }
    /// Makes only `chain` visible (templates are instantiated where they were
    /// declared, not where they are used); returns what was visible before.
    [[nodiscard]] std::vector<Scope*> isolate(std::vector<Scope*> chain) {
        std::swap(chain, stack_);
        return chain;
    }
    /// Undoes isolate().
    void restore(std::vector<Scope*> previous) { stack_ = std::move(previous); }

    /// Every visible name, innermost first (for "did you mean" suggestions).
    [[nodiscard]] std::vector<std::string> visible_names(bool (*filter)(const Symbol&)) const;
    /// `ns::name` for the first namespace, nested ones included, that
    /// declares `name` (for "did you mean 'ns::name'?" when the qualifier
    /// was left out).
    [[nodiscard]] std::optional<std::string> qualified_name(std::string_view name, bool (*filter)(const Symbol&)) const;

private:
    /// `name` declared in `scope` or in a namespace it uses.
    [[nodiscard]] static Symbol* find_in(Scope& scope, std::string_view name);

    std::deque<Scope> owned_;       // global, function and block scopes (stack discipline)
    std::deque<Scope> namespaces_;  // persistent
    /// Visible scopes, outermost first.
    std::vector<Scope*> stack_;
};

}  // namespace cppi::sema
