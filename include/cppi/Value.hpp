#pragma once

/// @file Value.hpp
/// @brief Runtime values.
///
/// A Value is a 16-byte trivially-copyable tagged integer: cheap to push on the
/// VM stack and to pass to host functions.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/TypeId.hpp>

#include <bit>

#include <cstdint>
#include <type_traits>

namespace cppi {

/// A value passed between player code and the host: a TypeId plus a 64-bit
/// payload. Trivially copyable and cheap to pass by value.
class Value {
public:
    /// A void value (what functions returning nothing produce).
    constexpr Value() noexcept = default;

    /// A void value; same as Value{}.
    [[nodiscard]] static constexpr Value void_value() noexcept { return {}; }
    /// A `bool`.
    [[nodiscard]] static constexpr Value from_bool(bool b) noexcept { return Value{types::Bool, b ? 1 : 0}; }
    /// An `int`.
    [[nodiscard]] static constexpr Value from_int(std::int64_t i) noexcept { return Value{types::Int, i}; }
    /// A `long`.
    [[nodiscard]] static constexpr Value from_long(std::int64_t i) noexcept { return Value{types::Long, i}; }
    /// A `char`.
    [[nodiscard]] static constexpr Value from_char(char c) noexcept { return Value{types::Char, c}; }
    /// A `double`.
    [[nodiscard]] static constexpr Value from_double(double d) noexcept {
        return Value{types::Double, std::bit_cast<std::int64_t>(d)};
    }
    /// An enumerator of the enum `type`, by its value.
    [[nodiscard]] static constexpr Value from_enum(TypeId type, std::int64_t enumerator) noexcept {
        return Value{type, enumerator};
    }
    /// A value of `type` whose payload is `raw` (see raw()).
    [[nodiscard]] static constexpr Value from_raw(TypeId type, std::int64_t raw) noexcept { return Value{type, raw}; }

    /// The type of the value.
    [[nodiscard]] constexpr TypeId type() const noexcept { return type_; }
    /// True for the void value.
    [[nodiscard]] constexpr bool is_void() const noexcept { return type_ == types::Void; }

    /// The value as a `bool` (any non-zero payload is true).
    [[nodiscard]] constexpr bool as_bool() const noexcept { return payload_ != 0; }
    /// The value as an `int`.
    [[nodiscard]] constexpr std::int64_t as_int() const noexcept { return payload_; }
    /// The value as a `long`.
    [[nodiscard]] constexpr std::int64_t as_long() const noexcept { return payload_; }
    /// The value as a `char`.
    [[nodiscard]] constexpr char as_char() const noexcept { return static_cast<char>(payload_); }
    /// The value as a `double` (only meaningful when type() is Double).
    [[nodiscard]] constexpr double as_double() const noexcept { return std::bit_cast<double>(payload_); }
    /// The enumerator's value (its position in the enum declaration).
    [[nodiscard]] constexpr std::int64_t as_enum() const noexcept { return payload_; }

    /// Raw payload: the integer, enumerator, or the bits of a double.
    [[nodiscard]] constexpr std::int64_t raw() const noexcept { return payload_; }

    friend constexpr bool operator==(const Value&, const Value&) = default;

private:
    /// Use the named factories instead.
    constexpr Value(TypeId type, std::int64_t payload) noexcept : type_(type), payload_(payload) {}

    TypeId type_ = types::Void;
    std::int64_t payload_ = 0;
};

static_assert(std::is_trivially_copyable_v<Value>);

}  // namespace cppi
