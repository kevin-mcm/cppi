#pragma once

/// @file CheckedArithmetic.hpp
/// @brief Signed integer arithmetic that reports overflow instead of invoking
/// undefined behavior in the interpreter itself.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>
#include <limits>

namespace cppi::detail {

/// Overflow-checked 64-bit signed arithmetic.
class CheckedArithmetic {
public:
    /// Each returns false on overflow (and leaves `out` unspecified).
    static bool add(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        return !__builtin_add_overflow(a, b, &out);
#else
        if ((b > 0 && a > kMax - b) || (b < 0 && a < kMin - b)) {
            return false;
        }
        out = a + b;
        return true;
#endif
    }

    /// `out = a - b`.
    static bool sub(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        return !__builtin_sub_overflow(a, b, &out);
#else
        if ((b < 0 && a > kMax + b) || (b > 0 && a < kMin + b)) {
            return false;
        }
        out = a - b;
        return true;
#endif
    }

    /// `out = a * b`.
    static bool mul(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        return !__builtin_mul_overflow(a, b, &out);
#else
        if (a == 0 || b == 0) {
            out = 0;
            return true;
        }
        if ((a == -1 && b == kMin) || (b == -1 && a == kMin)) {
            return false;
        }
        if (a > 0 ? (b > 0 ? a > kMax / b : b < kMin / a) : (b > 0 ? a < kMin / b : a < kMax / b)) {
            return false;
        }
        out = a * b;
        return true;
#endif
    }

    /// True if `value` is representable as a 32-bit int.
    static bool fits_int32(std::int64_t value) noexcept {
        return value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max();
    }

private:
    /// Largest int64.
    static constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    /// Smallest int64.
    static constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
};

}  // namespace cppi::detail
