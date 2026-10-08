#pragma once

/// Deduplicating pool of the constants a program pushes.

#include "codegen/ProgramData.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cppi::detail {

class ConstantPool {
public:
    explicit ConstantPool(std::vector<ConstantEntry>& constants) noexcept : constants_(constants) {}

    /// Index of the constant, adding it the first time it is seen.
    [[nodiscard]] std::uint32_t intern(std::int64_t bits, std::string text);

private:
    std::vector<ConstantEntry>& constants_;
    std::map<std::pair<std::int64_t, std::string>, std::uint32_t> index_;
};

}  // namespace cppi::detail
