#pragma once

/// @file Language.hpp
/// @brief Languages cppi-run can print messages in.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>
#include <optional>
#include <string_view>

namespace cppi_run {

/// Message languages.
enum class Language : std::uint8_t { English, Spanish };

/// Parses "en", "es"...; nullopt if unknown.
[[nodiscard]] std::optional<Language> parse_language(std::string_view code) noexcept;

/// Language from the environment (LANG / LC_ALL), English by default.
[[nodiscard]] Language detect_language() noexcept;

}  // namespace cppi_run
