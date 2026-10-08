#pragma once

/// A function or class template, and the instances made from it.

#include "sema/TypeInfo.hpp"

#include "ast/Stmt.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

class Scope;

struct TemplateInfo {
    std::string name;
    std::vector<std::string> params;
    std::vector<const ast::TypeDesc*> defaults;  ///< per parameter, null if it has none
    const ast::Stmt* inner = nullptr;            ///< the FunctionDef or RecordDef (valid during analysis)
    const ast::FunctionDecl* method = nullptr;   ///< member function templates: the declaration
    std::optional<std::uint32_t> record;         ///< member function templates: the class
    /// `[](auto x) { ... }`: the call operator of a generic lambda. Its
    /// parameters are named auto#0, auto#1... in order of appearance.
    bool generic_lambda = false;
    bool lambda_const = true;    ///< not `mutable`
    bool lambda_deduce = false;  ///< return type deduced from the body
    bool is_class = false;
    // C++20 constraints: a concept, or the constraints a template's arguments must meet.
    bool is_concept = false;
    const ast::Expr* concept_expr = nullptr;                                ///< `concept C = <expr>;`
    std::map<std::vector<TypeRef>, bool> satisfied;                         ///< concepts: results so far
    std::vector<std::pair<std::size_t, const ast::TypeSpec*>> constrained;  ///< `Number T`: (param, concept)
    std::vector<const ast::Expr*> requirements;                             ///< requires-clauses
    bool library = false;                                     ///< declared by the standard library prelude
    std::vector<Scope*> home;                                 ///< scopes visible where it was declared
    std::map<std::vector<TypeRef>, std::uint32_t> functions;  ///< instance args -> function id
    std::map<std::vector<TypeRef>, TypeRef> classes;          ///< instance args -> record type
};

}  // namespace cppi::sema
