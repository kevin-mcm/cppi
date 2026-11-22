#pragma once

/// @file LibraryStandards.hpp
/// @brief Which standard introduced each name of the library prelude, so a
/// program compiled as an older standard cannot use it (E0304).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-11-22

#include <cppi/Standard.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppi::library {

/// The standard that introduced `std::name`, or nothing if it is as old as C++98
/// (or unknown). `name` is `vector` for a name and `optional::value_or` for a
/// member of a library class.
[[nodiscard]] std::optional<Standard> standard_of(std::string_view name);

/// Every name of the table that is not a member (`class::member`), to mark the
/// prelude's symbols.
[[nodiscard]] std::vector<std::string> names();

/// For tests: adds an entry (a member of an existing class, say) while it lives.
class ScopedEntry {
public:
    ScopedEntry(std::string name, Standard since);
    ScopedEntry(const ScopedEntry&) = delete;
    ScopedEntry& operator=(const ScopedEntry&) = delete;
    ScopedEntry(ScopedEntry&&) = delete;
    ScopedEntry& operator=(ScopedEntry&&) = delete;
    ~ScopedEntry();

private:
    std::string name_;
};

}  // namespace cppi::library
