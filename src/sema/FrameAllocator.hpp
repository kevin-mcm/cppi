#pragma once

/// Hands out cells of a function's frame: parameters first, then every
/// local and temporary. Slots are never reused, so a debugger can always
/// find a variable where the analyzer put it.

#include <cstdint>

namespace cppi::sema {

class FrameAllocator {
public:
    [[nodiscard]] std::uint32_t allocate(std::uint32_t cells) noexcept {
        const std::uint32_t offset = next_;
        next_ += cells;
        return offset;
    }
    [[nodiscard]] std::uint32_t size() const noexcept { return next_; }

private:
    std::uint32_t next_ = 0;
};

}  // namespace cppi::sema
