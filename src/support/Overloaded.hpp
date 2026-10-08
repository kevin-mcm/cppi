#pragma once

/// Builds a visitor for std::visit out of lambdas, one per alternative.

namespace cppi::detail {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

// Explicit guide: aggregate deduction (C++20) is missing from older Clang/AppleClang.
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace cppi::detail
