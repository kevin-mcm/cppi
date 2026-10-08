#include "support/NumberFormat.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

// CPPI_HAS_FLOAT_CHARCONV (set by CMake when the standard library has
// std::from_chars and std::to_chars for double) selects the locale-free
// conversions. Without it (libc++ before 20, older NDKs), parse() is exact on
// its own for the common literals and otherwise uses strtod, and shortest()
// uses snprintf; both only see the C locale (LC_NUMERIC), never the C++ one,
// and are made independent of it below.
#if CPPI_HAS_FLOAT_CHARCONV
#include <charconv>
#include <system_error>
#else
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstdlib>
#endif

namespace cppi::detail {

namespace {

#if !CPPI_HAS_FLOAT_CHARCONV
/// The decimal point of the C locale in effect ("." unless the host changed LC_NUMERIC).
std::string_view decimal_point() {
    const char* point = std::localeconv()->decimal_point;  // NOLINT(concurrency-mt-unsafe): read once, not kept
    return point != nullptr && *point != '\0' ? std::string_view(point) : std::string_view(".");
}

/// Decimal literals of at most 19 digits whose value and power of ten are
/// both exact doubles: one multiplication or division rounds correctly.
std::optional<double> parse_exact(std::string_view text) {
    std::uint64_t mantissa = 0;
    int digits = 0;
    int exponent = 0;
    bool point = false;
    bool any = false;
    std::size_t i = 0;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '.' && !point) {
            point = true;
            continue;
        }
        if (c < '0' || c > '9') {
            break;
        }
        any = true;
        if (mantissa == 0 && c == '0') {
            exponent -= point ? 1 : 0;
            continue;
        }
        if (++digits > 19) {
            return std::nullopt;
        }
        mantissa = mantissa * 10 + static_cast<std::uint64_t>(c - '0');
        exponent -= point ? 1 : 0;
    }
    if (!any) {
        return std::nullopt;
    }
    if (i < text.size()) {
        if (text[i] != 'e' && text[i] != 'E') {
            return std::nullopt;
        }
        ++i;
        bool negative = false;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
            negative = text[i] == '-';
            ++i;
        }
        if (i == text.size()) {
            return std::nullopt;
        }
        int written = 0;
        for (; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9' || written > 1000) {
                return std::nullopt;
            }
            written = written * 10 + (text[i] - '0');
        }
        exponent += negative ? -written : written;
    }
    constexpr std::uint64_t kExactLimit = std::uint64_t{1} << 53;
    static constexpr std::array<double, 23> kPowers = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                       1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                       1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    if (mantissa > kExactLimit || exponent < -22 || exponent > 22) {
        return std::nullopt;
    }
    const auto m = static_cast<double>(mantissa);
    const auto power = kPowers[static_cast<std::size_t>(exponent < 0 ? -exponent : exponent)];
    return exponent < 0 ? m / power : m * power;
}
#endif

}  // namespace

std::string NumberFormat::shortest(double value) {
    std::array<char, 64> buffer{};
#if CPPI_HAS_FLOAT_CHARCONV
    // Specified as printf's "%.6g" in the "C" locale.
    const auto [end, error] =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general, 6);
    if (error != std::errc{}) {
        return "?";
    }
    return std::string(buffer.data(), end);
#else
    const int n = std::snprintf(buffer.data(), buffer.size(), "%.6g", value);
    if (n < 0 || static_cast<std::size_t>(n) >= buffer.size()) {
        return "?";
    }
    std::string out(buffer.data(), static_cast<std::size_t>(n));
    // "%g" uses LC_NUMERIC's decimal point (and never groups digits).
    if (const std::string_view point = decimal_point(); point != ".") {
        if (const auto at = out.find(point); at != std::string::npos) {
            out.replace(at, point.size(), ".");
        }
    }
    return out;
#endif
}

std::optional<double> NumberFormat::parse(std::string_view text) {
    // A literal starts with a digit or the point (from_chars and strtod also
    // read a sign, "inf" and "nan").
    if (text.empty() || !((text.front() >= '0' && text.front() <= '9') || text.front() == '.')) {
        return std::nullopt;
    }
    const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
#if CPPI_HAS_FLOAT_CHARCONV
    const std::string_view body = hex ? text.substr(2) : text;
    double value = 0;
    const auto [end, error] = std::from_chars(body.data(), body.data() + body.size(), value,
                                              hex ? std::chars_format::hex : std::chars_format::general);
    // A literal out of range (1e999) makes the program ill-formed.
    if (error != std::errc{} || end != body.data() + body.size()) {
        return std::nullopt;
    }
    return value;
#else
    if (!hex) {
        if (auto exact = parse_exact(text)) {
            return exact;
        }
    }
    for (const char c : text) {
        // strtod also reads "inf", "nan" and leading spaces; literals have none of them.
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '.' || c == 'x' ||
              c == 'X' || c == 'p' || c == 'P' || c == '+' || c == '-')) {
            return std::nullopt;
        }
    }
    std::string localized(text);
    if (const std::string_view point = decimal_point(); point != ".") {
        if (const auto at = localized.find('.'); at != std::string::npos) {
            localized.replace(at, 1, point);
        }
    }
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(localized.c_str(), &end);
    // ERANGE also flags subnormal results, which are fine: only overflow and
    // underflow to zero are out of range, as for from_chars.
    if (end != localized.c_str() + localized.size() || (errno == ERANGE && (std::isinf(value) || value == 0))) {
        return std::nullopt;
    }
    return value;
#endif
}

}  // namespace cppi::detail
