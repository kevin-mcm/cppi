/// @file LibraryStandards.cpp
/// @brief Implementation of LibraryStandards.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-11-22

#include "library/LibraryStandards.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace cppi::library {

namespace {

struct Entry {
    std::string_view name;
    Standard since;
};

constexpr Entry kTable[] = {
    // C++11
    {"unique_ptr", Standard::Cpp11},
    {"shared_ptr", Standard::Cpp11},
    {"make_shared", Standard::Cpp11},
    {"to_string", Standard::Cpp11},
    {"array", Standard::Cpp11},
    {"tuple", Standard::Cpp11},
    {"make_tuple", Standard::Cpp11},
    {"tie", Standard::Cpp11},
    {"function", Standard::Cpp11},
    {"bad_function_call", Standard::Cpp11},
    {"initializer_list", Standard::Cpp11},
    {"iota", Standard::Cpp11},
    {"all_of", Standard::Cpp11},
    {"any_of", Standard::Cpp11},
    {"none_of", Standard::Cpp11},
    {"find_if_not", Standard::Cpp11},
    // C++14
    {"make_unique", Standard::Cpp14},
    // C++17
    {"optional", Standard::Cpp17},
    {"nullopt", Standard::Cpp17},
    {"nullopt_t", Standard::Cpp17},
    {"bad_optional_access", Standard::Cpp17},
    {"variant", Standard::Cpp17},
    {"bad_variant_access", Standard::Cpp17},
    {"visit", Standard::Cpp17},
    {"get_if", Standard::Cpp17},
    {"holds_alternative", Standard::Cpp17},
    // C++20
    {"same_as", Standard::Cpp20},
    {"integral", Standard::Cpp20},
    {"signed_integral", Standard::Cpp20},
    {"unsigned_integral", Standard::Cpp20},
    {"floating_point", Standard::Cpp20},
    {"convertible_to", Standard::Cpp20},
    {"derived_from", Standard::Cpp20},
    {"strong_ordering", Standard::Cpp20},
    {"weak_ordering", Standard::Cpp20},
    {"partial_ordering", Standard::Cpp20},
    // C++23: the names and, as `class::member`, members added to older classes.
    {"expected", Standard::Cpp23},
    {"unexpected", Standard::Cpp23},
    {"bad_expected_access", Standard::Cpp23},
    {"to_underlying", Standard::Cpp23},
    {"optional::and_then", Standard::Cpp23},
    {"optional::transform", Standard::Cpp23},
    {"optional::or_else", Standard::Cpp23},
};

std::map<std::string, Standard, std::less<>>& extra_entries() {
    static std::map<std::string, Standard, std::less<>> entries;  // only tests add to it
    return entries;
}

}  // namespace

std::optional<Standard> standard_of(std::string_view name) {
    const auto& extra = extra_entries();
    if (const auto it = extra.find(name); it != extra.end()) {
        return it->second;
    }
    const auto* found =
        std::find_if(std::begin(kTable), std::end(kTable), [&](const Entry& e) { return e.name == name; });
    if (found == std::end(kTable)) {
        return std::nullopt;
    }
    return found->since;
}

std::vector<std::string> names() {
    std::vector<std::string> out;
    for (const Entry& e : kTable) {
        if (e.name.find("::") == std::string_view::npos) {
            out.emplace_back(e.name);
        }
    }
    return out;
}

ScopedEntry::ScopedEntry(std::string name, Standard since) : name_(std::move(name)) {
    extra_entries()[name_] = since;
}

ScopedEntry::~ScopedEntry() {
    extra_entries().erase(name_);
}

}  // namespace cppi::library
