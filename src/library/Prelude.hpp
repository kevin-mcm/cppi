#pragma once

/// @file Prelude.hpp
/// @brief The small standard library cppi offers (std::cout, std::string,
/// std::vector, std::sort...), written in the same C++ subset the interpreter
/// runs.
///
/// It is compiled before every program, outside the level's feature rules, and
/// its code is free and invisible to debuggers.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <string_view>

namespace cppi::library {

/// The source code of the standard library prelude.
class Prelude {
public:
    /// The prelude's C++ source, compiled before the player's code.
    [[nodiscard]] static std::string_view source() noexcept;
};

}  // namespace cppi::library
