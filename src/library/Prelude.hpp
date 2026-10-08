#pragma once

/// The small standard library cppi offers (std::cout, std::string,
/// std::vector, std::sort...), written in the same C++ subset the
/// interpreter runs. It is compiled before every program, outside the
/// level's feature rules, and its code is free and invisible to debuggers.

#include <string_view>

namespace cppi::library {

class Prelude {
public:
    [[nodiscard]] static std::string_view source() noexcept;
};

}  // namespace cppi::library
