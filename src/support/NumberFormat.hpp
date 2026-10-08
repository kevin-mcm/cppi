#pragma once

/// @file NumberFormat.hpp
/// @brief Reads and writes doubles without iostreams or the C++ locale: the
/// same text in any process, even one that mixes two copies of the C++ standard
/// library (a static libstdc++ in a Godot extension plus the system's, loaded
/// by a graphics driver), whose locale facets must never meet.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <optional>
#include <string>
#include <string_view>

namespace cppi::detail {

/// Locale-independent conversions between doubles and text.
class NumberFormat {
public:
    /// What `std::cout << value` prints by default: printf's "%g" with 6
    /// significant digits ("0.3", "1e+21", "1.23457e+08", "-0", "inf", "nan").
    [[nodiscard]] static std::string shortest(double value);

    /// The value of a floating-point literal without digit separators or
    /// suffix: "5.0", ".5", "5.", "2.5e-3", "0x1.8p3". Correctly rounded.
    /// Empty if the text is not such a number.
    [[nodiscard]] static std::optional<double> parse(std::string_view text);
};

}  // namespace cppi::detail
