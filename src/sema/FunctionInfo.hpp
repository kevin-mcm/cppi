#pragma once

/// A function the player declared: its signature, and where it lives.

#include "sema/TypeInfo.hpp"

#include "ast/Stmt.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

struct ParamInfo {
    std::string name;
    TypeRef type = 0;  ///< as declared (may be a reference type)
    const ast::Expr* default_value = nullptr;
    SourceRange range;
};

struct FunctionInfo {
    std::string name;     ///< `move`
    std::string display;  ///< `Robot::move`, for messages
    TypeRef return_type = 0;
    std::vector<ParamInfo> params;
    bool defined = false;
    SourceRange range;
    const ast::FunctionDecl* decl = nullptr;  ///< valid during analysis only

    // Member functions
    std::optional<std::uint32_t> record;
    bool is_const = false;
    bool is_virtual = false;
    bool is_pure = false;
    bool is_static = false;
    bool is_constructor = false;
    bool is_destructor = false;
    ast::Access access = ast::Access::Public;

    bool library = false;  ///< part of the standard library prelude
    bool deleted = false;  ///< `= delete`: choosing it is an error
    bool is_explicit = false;
    bool is_constexpr = false;  ///< may run at compile time (ConstexprInterpreter)

    /// Member functions the compiler writes itself, member by member.
    enum class Implicit : std::uint8_t { None, CopyConstructor, CopyAssignment, DefaultConstructor };
    Implicit implicit = Implicit::None;

    // Templates: the instantiation this function came from
    std::optional<std::uint32_t> template_id;
};

}  // namespace cppi::sema
