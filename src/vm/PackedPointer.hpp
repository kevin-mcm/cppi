#pragma once

/// A pointer as the VM stores it: the address plus the bounds of the object
/// it points into, packed in one 64-bit cell (21 bits each). The bounds let
/// the VM catch out-of-bounds accesses through pointers, not just through
/// arrays. Address 0 is never valid, so a packed 0 is the null pointer.

#include <cstdint>

namespace cppi::detail {

class PackedPointer {
public:
    static constexpr std::uint32_t kAddressBits = 21;
    static constexpr std::uint64_t kMask = (std::uint64_t{1} << kAddressBits) - 1;
    static constexpr std::uint32_t kLimit = 1U << kAddressBits;  ///< exclusive

    [[nodiscard]] static constexpr std::int64_t pack(std::uint32_t address, std::uint32_t begin,
                                                     std::uint32_t end) noexcept {
        return static_cast<std::int64_t>((static_cast<std::uint64_t>(address) & kMask) |
                                         ((static_cast<std::uint64_t>(begin) & kMask) << kAddressBits) |
                                         ((static_cast<std::uint64_t>(end) & kMask) << (2 * kAddressBits)));
    }
    [[nodiscard]] static constexpr std::uint32_t address(std::int64_t p) noexcept {
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) & kMask);
    }
    [[nodiscard]] static constexpr std::uint32_t begin(std::int64_t p) noexcept {
        return static_cast<std::uint32_t>((static_cast<std::uint64_t>(p) >> kAddressBits) & kMask);
    }
    [[nodiscard]] static constexpr std::uint32_t end(std::int64_t p) noexcept {
        return static_cast<std::uint32_t>((static_cast<std::uint64_t>(p) >> (2 * kAddressBits)) & kMask);
    }
    [[nodiscard]] static constexpr std::int64_t with_address(std::int64_t p, std::uint32_t address) noexcept {
        return pack(address, begin(p), end(p));
    }
};

}  // namespace cppi::detail
