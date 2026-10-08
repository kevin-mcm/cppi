#pragma once

/// @file FunctionInfo.hpp
/// @brief A function the player declared: its signature, and where it lives.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/TypeInfo.hpp"

#include "ast/Stmt.hpp"

#include <cppi/SourceRange.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

/// A parameter of a function the player declared.
struct ParamInfo {
    /// Parameter name (may be empty).
    std::string name;
    TypeRef type = 0;  ///< as declared (may be a reference type)
    /// Default argument, or nullptr (valid during analysis only).
    const ast::Expr* default_value = nullptr;
    /// Where it is declared.
    SourceRange range;
};

/// A function of the program: player, library, implicit or template instance.
struct FunctionInfo {
    std::string name;     ///< `move`
    std::string display;  ///< `Robot::move`, for messages
    /// Return type.
    TypeRef return_type = 0;
    /// Parameters, in order.
    std::vector<ParamInfo> params;
    /// Has a body.
    bool defined = false;
    /// Where it is declared.
    SourceRange range;
    const ast::FunctionDecl* decl = nullptr;  ///< valid during analysis only

    // Member functions
    /// Member functions: the class they belong to.
    std::optional<std::uint32_t> record;
    /// `const` member function.
    bool is_const = false;
    /// Virtual (declared so, or overriding a virtual function).
    bool is_virtual = false;
    /// Pure virtual (`= 0`).
    bool is_pure = false;
    /// Static member function.
    bool is_static = false;
    /// A constructor.
    bool is_constructor = false;
    /// A destructor.
    bool is_destructor = false;
    /// Member access.
    ast::Access access = ast::Access::Public;

    bool library = false;  ///< part of the standard library prelude
    bool deleted = false;  ///< `= delete`: choosing it is an error
    /// `explicit` constructor or conversion operator.
    bool is_explicit = false;
    bool is_constexpr = false;  ///< may run at compile time (ConstexprInterpreter)

    /// Member functions the compiler writes itself, member by member.
    enum class Implicit : std::uint8_t { None, CopyConstructor, CopyAssignment, DefaultConstructor };
    /// Which implicit member function it is, if any.
    Implicit implicit = Implicit::None;

    // Templates: the instantiation this function came from
    /// The template it was instantiated from, if any.
    std::optional<std::uint32_t> template_id;
};

}  // namespace cppi::sema
