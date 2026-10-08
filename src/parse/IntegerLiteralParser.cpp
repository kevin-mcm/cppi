/// @file IntegerLiteralParser.cpp
/// @brief Implementation of IntegerLiteralParser.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "parse/IntegerLiteralParser.hpp"

#include <limits>
#include <string>

namespace cppi::parse {

std::optional<std::uint64_t> IntegerLiteralParser::parse(std::string_view text) noexcept {
    std::string digits;
    digits.reserve(text.size());
    for (const char c : text) {
        if (c != '\'') {
            digits.push_back(c);
        }
    }
    while (!digits.empty() && std::string_view("uUlLzZ").find(digits.back()) != std::string_view::npos) {
        digits.pop_back();
    }

    unsigned base = 10;
    std::size_t start = 0;
    if (digits.size() > 1 && digits[0] == '0') {
        if (digits[1] == 'x' || digits[1] == 'X') {
            base = 16;
            start = 2;
        } else if (digits[1] == 'b' || digits[1] == 'B') {
            base = 2;
            start = 2;
        } else {
            base = 8;
            start = 1;
        }
    }

    std::uint64_t value = 0;
    constexpr auto max_value = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i = start; i < digits.size(); ++i) {
        const char c = digits[i];
        unsigned d = 0;
        if (c >= '0' && c <= '9') {
            d = static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = static_cast<unsigned>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = static_cast<unsigned>(c - 'A' + 10);
        } else {
            return std::nullopt;
        }
        if (d >= base || value > (max_value - d) / base) {
            return std::nullopt;
        }
        value = value * base + d;
    }
    return value;
}

}  // namespace cppi::parse
