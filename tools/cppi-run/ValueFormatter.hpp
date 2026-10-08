#pragma once

/// @file ValueFormatter.hpp
/// @brief Renders runtime values the way the player wrote them: 3, true, East.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/HostRegistry.hpp>
#include <cppi/Value.hpp>

#include <string>

namespace cppi_run {

/// Renders runtime values as player code would write them.
class ValueFormatter {
public:
    /// `value` as text: numbers, `true`/`false`, enumerator names.
    [[nodiscard]] static std::string format(const cppi::HostRegistry& host, const cppi::Value& value);
};

}  // namespace cppi_run
