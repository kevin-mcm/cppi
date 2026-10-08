#pragma once

// Languages cppi-run can print messages in.

#include <cstdint>
#include <optional>
#include <string_view>

namespace cppi_run {

enum class Language : std::uint8_t { English, Spanish };

[[nodiscard]] std::optional<Language> parse_language(std::string_view code) noexcept;

/// Language from the environment (LANG / LC_ALL), English by default.
[[nodiscard]] Language detect_language() noexcept;

}  // namespace cppi_run
