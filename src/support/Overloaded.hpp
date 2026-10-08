#pragma once

/// @file Overloaded.hpp
/// @brief Builds a visitor for std::visit out of lambdas, one per alternative.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

namespace cppi::detail {

template <class... Ts>
/// A callable inheriting the call operator of every `Ts`:
/// `std::visit(Overloaded{[](int) {...}, [](double) {...}}, v)`.
struct Overloaded : Ts... {
    using Ts::operator()...;
};

// Explicit guide: aggregate deduction (C++20) is missing from older Clang/AppleClang.
template <class... Ts>
/// Deduces `Overloaded<Ts...>` from the lambdas passed.
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace cppi::detail
