#pragma once

/// @file ConstantPool.hpp
/// @brief Deduplicating pool of the constants a program pushes.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/ProgramData.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cppi::detail {

/// Adds constants to a program's constant table, each distinct value only once.
class ConstantPool {
public:
    /// @param constants The table to fill (ProgramData::constants).
    explicit ConstantPool(std::vector<ConstantEntry>& constants) noexcept : constants_(constants) {}

    /// Index of the constant, adding it the first time it is seen.
    /// @param bits The constant's bits (integer, enumerator or double bits).
    /// @param text Rendering for listings; constants with equal bits but different
    /// types stay distinct.
    [[nodiscard]] std::uint32_t intern(std::int64_t bits, std::string text);

private:
    std::vector<ConstantEntry>& constants_;
    std::map<std::pair<std::int64_t, std::string>, std::uint32_t> index_;
};

}  // namespace cppi::detail
