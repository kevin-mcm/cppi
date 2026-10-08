#pragma once

/// The VM's memory: one cell per scalar, each with an "initialized" bit.
/// Globals and stack frames share one region (address 1 upwards; address 0
/// is never used, so it can mean null); the heap is a second region whose
/// blocks are never reused, so dangling pointers into it are always caught.

#include "vm/PackedPointer.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace cppi::detail {

class Memory {
public:
    static constexpr std::uint32_t kStackBase = 1;
    static constexpr std::uint32_t kHeapBase = 1U << 20;
    static constexpr std::uint32_t kHeapLimit = PackedPointer::kLimit - 1;

    enum class Fault : std::uint8_t {
        None,
        Null,
        OutOfBounds,
        Dangling,  ///< a stack slot of a function that already returned
        UseAfterFree,
        Uninitialized,
        DoubleFree,
        InvalidDelete,
        OutOfMemory,
    };

    explicit Memory(std::uint32_t global_cells);

    // --- Stack -------------------------------------------------------------
    [[nodiscard]] std::uint32_t globals_address(std::uint32_t offset) const noexcept { return kStackBase + offset; }
    [[nodiscard]] std::uint32_t top() const noexcept { return top_; }
    /// Pushes a frame of `cells` uninitialized cells; returns its base.
    [[nodiscard]] std::optional<std::uint32_t> push_frame(std::uint32_t cells);
    void pop_frame(std::uint32_t base) noexcept { top_ = base; }

    // --- Direct access (addresses already validated) --------------------------
    [[nodiscard]] std::int64_t& cell(std::uint32_t address) noexcept;
    [[nodiscard]] std::int64_t value(std::uint32_t address) const noexcept;
    [[nodiscard]] bool initialized(std::uint32_t address) const noexcept;
    void set_initialized(std::uint32_t address, bool value) noexcept;

    // --- Checked access through pointers ------------------------------------------
    /// Can `cells` cells starting at the pointer be accessed?
    [[nodiscard]] Fault check(std::int64_t pointer, std::uint32_t cells) const;
    [[nodiscard]] Fault load(std::int64_t pointer, std::int64_t& value) const;
    [[nodiscard]] Fault store(std::int64_t pointer, std::int64_t value);
    [[nodiscard]] Fault fill(std::int64_t pointer, std::uint32_t cells, bool zero);
    [[nodiscard]] Fault copy(std::int64_t destination, std::int64_t source, std::uint32_t cells);

    // --- Heap ------------------------------------------------------------------------
    [[nodiscard]] Fault allocate(std::uint32_t cells, bool zero, bool array, std::int64_t& pointer);
    [[nodiscard]] Fault release(std::uint32_t address, bool array);
    [[nodiscard]] std::size_t live_blocks() const noexcept;
    /// Cells of the live heap block that starts at `address`, or nullopt.
    [[nodiscard]] std::optional<std::uint32_t> block_cells(std::uint32_t address) const;

private:
    struct Block {
        std::uint32_t start = 0;
        std::uint32_t cells = 0;
        bool array = false;
        bool alive = true;
    };
    [[nodiscard]] const Block* block_at(std::uint32_t address) const;

    std::vector<std::int64_t> stack_;
    std::vector<std::uint8_t> stack_init_;
    std::uint32_t top_ = kStackBase;
    std::vector<std::int64_t> heap_;
    std::vector<std::uint8_t> heap_init_;
    std::vector<Block> blocks_;
    std::map<std::uint32_t, std::size_t> block_index_;  ///< start -> block
};

}  // namespace cppi::detail
