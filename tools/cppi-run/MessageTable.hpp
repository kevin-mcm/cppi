#pragma once

// A constant key -> text table, the storage behind each message catalog.

#include "MessageEntry.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace cppi_run {

class MessageTable {
public:
    constexpr explicit MessageTable(std::span<const MessageEntry> entries) noexcept : entries_(entries) {}

    [[nodiscard]] constexpr std::optional<std::string_view> find(std::string_view key) const noexcept {
        for (const auto& entry : entries_) {
            if (entry.key == key) {
                return entry.text;
            }
        }
        return std::nullopt;
    }

private:
    std::span<const MessageEntry> entries_;
};

}  // namespace cppi_run
