#pragma once

/// @file MessageTable.hpp
/// @brief A constant key -> text table, the storage behind each message
/// catalog.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "MessageEntry.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace cppi_run {

/// A read-only view over an array of MessageEntry.
class MessageTable {
public:
    /// @param entries The entries (static storage).
    constexpr explicit MessageTable(std::span<const MessageEntry> entries) noexcept : entries_(entries) {}

    /// The text for `key`, or nullopt.
    [[nodiscard]] constexpr std::optional<std::string_view> find(std::string_view key) const noexcept {
        for (const auto& entry : entries_) {
            if (entry.key == key) {
                return entry.text;
            }
        }
        return std::nullopt;
    }

private:
    /// The entries.
    std::span<const MessageEntry> entries_;
};

}  // namespace cppi_run
