/// @file CppIdentifier.cpp
/// @brief Implementation of CppIdentifier.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "support/CppIdentifier.hpp"

#include <algorithm>
#include <iterator>

namespace cppi::detail {

namespace {

// Every C++ keyword up to C++26, plus alternative tokens.
constexpr std::string_view kKeywords[] = {
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char8_t",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const",
    "consteval",
    "constexpr",
    "constinit",
    "const_cast",
    "continue",
    "co_await",
    "co_return",
    "co_yield",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
    "import",
    "module",
    "final",
    "override",
    "contract_assert",
};

}  // namespace

bool CppIdentifier::is_well_formed(std::string_view name) noexcept {
    if (name.empty()) {
        return false;
    }
    auto is_alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    if (!is_alpha(name.front())) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [&](char c) { return is_alpha(c) || is_digit(c); });
}

bool CppIdentifier::is_keyword(std::string_view name) noexcept {
    return std::find(std::begin(kKeywords), std::end(kKeywords), name) != std::end(kKeywords);
}

bool CppIdentifier::is_valid(std::string_view name) noexcept {
    return is_well_formed(name) && !is_keyword(name) && !name.starts_with("__");
}

}  // namespace cppi::detail
