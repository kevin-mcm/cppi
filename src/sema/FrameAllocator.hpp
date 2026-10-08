#pragma once

/// @file FrameAllocator.hpp
/// @brief Hands out cells of a function's frame: parameters first, then every
/// local and temporary.
///
/// Slots are never reused, so a debugger can always find a variable where the
/// analyzer put it.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cstdint>

namespace cppi::sema {

/// Hands out frame cells in order; frames only grow while a function is bound.
class FrameAllocator {
public:
    /// Reserves `cells` cells and returns the offset of the first.
    [[nodiscard]] std::uint32_t allocate(std::uint32_t cells) noexcept {
        const std::uint32_t offset = next_;
        next_ += cells;
        return offset;
    }
    /// Cells allocated so far: the frame size.
    [[nodiscard]] std::uint32_t size() const noexcept { return next_; }

private:
    /// Next free cell.
    std::uint32_t next_ = 0;
};

}  // namespace cppi::sema
