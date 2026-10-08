#include "sema/SymbolTable.hpp"

namespace cppi::sema {

Symbol* SymbolTable::find_in(Scope& scope, std::string_view name) {
    if (Symbol* s = scope.find(name)) {
        return s;
    }
    for (Scope* used : scope.usings()) {
        if (Symbol* s = used->find(name)) {
            return s;
        }
    }
    return nullptr;
}

void SymbolTable::enter_namespace(const std::string& name, SourceRange range) {
    Scope& outer = current();
    Symbol* existing = outer.find(name);
    if (existing == nullptr || existing->kind != SymbolKind::Namespace) {
        namespaces_.emplace_back(Scope::Kind::Namespace);
        Symbol ns;
        ns.kind = SymbolKind::Namespace;
        ns.range = range;
        ns.scope = &namespaces_.back();
        if (existing == nullptr) {
            outer.add(name, ns);
        } else {
            *existing = ns;  // a poisoned name becomes the namespace
        }
        existing = outer.find(name);
    }
    stack_.push_back(existing->scope);
}

Symbol* SymbolTable::lookup(std::string_view name) {
    for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
        if (Symbol* s = find_in(**it, name)) {
            return s;
        }
    }
    return nullptr;
}

Symbol* SymbolTable::lookup_local(std::string_view name) {
    for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
        const Scope::Kind kind = (*it)->kind();
        if (kind == Scope::Kind::Global || kind == Scope::Kind::Namespace) {
            break;
        }
        if (Symbol* s = find_in(**it, name)) {
            return s;
        }
    }
    return nullptr;
}

Scope* SymbolTable::find_namespace(const std::vector<std::string>& scope) {
    if (scope.empty()) {
        return nullptr;
    }
    // Namespaces, and classes with static members (their Type symbol has a scope).
    auto names_scope = [](const Symbol* s) {
        return s != nullptr && (s->kind == SymbolKind::Namespace || s->kind == SymbolKind::Type) && s->scope != nullptr;
    };
    Symbol* first = lookup(scope.front());
    if (!names_scope(first)) {
        return nullptr;
    }
    Scope* current_scope = first->scope;
    for (std::size_t i = 1; i < scope.size(); ++i) {
        Symbol* next = find_in(*current_scope, scope[i]);
        if (!names_scope(next)) {
            return nullptr;
        }
        current_scope = next->scope;
    }
    return current_scope;
}

Symbol* SymbolTable::lookup_qualified(const std::vector<std::string>& scope, std::string_view name) {
    Scope* ns = find_namespace(scope);
    return ns == nullptr ? nullptr : find_in(*ns, name);
}

std::vector<std::string> SymbolTable::visible_names(bool (*filter)(const Symbol&)) const {
    std::vector<std::string> names;
    for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
        for (const auto& [name, symbol] : (*it)->symbols()) {
            if (filter(symbol)) {
                names.push_back(name);
            }
        }
        for (const Scope* used : (*it)->usings()) {
            for (const auto& [name, symbol] : used->symbols()) {
                if (filter(symbol)) {
                    names.push_back(name);
                }
            }
        }
    }
    return names;
}

std::optional<std::string> SymbolTable::qualified_name(std::string_view name, bool (*filter)(const Symbol&)) const {
    // Breadth first, so `a::name` is preferred over `a::b::name`.
    std::vector<std::pair<std::string, const Scope*>> queue{{"", stack_.front()}};
    for (std::size_t i = 0; i < queue.size(); ++i) {
        const auto [prefix, scope] = queue[i];
        if (!prefix.empty()) {
            auto it = scope->symbols().find(name);
            if (it != scope->symbols().end() && filter(it->second)) {
                return prefix + std::string(name);
            }
        }
        for (const auto& [inner, symbol] : scope->symbols()) {
            if (symbol.kind == SymbolKind::Namespace && symbol.scope != nullptr) {
                queue.emplace_back(prefix + inner + "::", symbol.scope);
            }
        }
    }
    return std::nullopt;
}

}  // namespace cppi::sema
