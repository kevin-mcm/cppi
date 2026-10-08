#pragma once

/// @file MessageEntry.hpp
/// @brief One key -> text pair of a message catalog.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <string_view>

namespace cppi_run {

/// One translated text and its key.
struct MessageEntry {
    /// Lookup key.
    std::string_view key;
    /// Translated text.
    std::string_view text;
};

}  // namespace cppi_run
