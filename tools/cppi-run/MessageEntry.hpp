#pragma once

// One key -> text pair of a message catalog.

#include <string_view>

namespace cppi_run {

struct MessageEntry {
    std::string_view key;
    std::string_view text;
};

}  // namespace cppi_run
