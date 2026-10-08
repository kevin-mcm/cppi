#pragma once

/// @file Memory.hpp
/// @brief The VM's memory: one cell per scalar, each with an "initialized" bit.
///
/// Globals and stack frames share one region (address 1 upwards; address 0 is
/// never used, so it can mean null); the heap is a second region whose blocks
/// are never reused, so dangling pointers into it are always caught.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "vm/PackedPointer.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace cppi::detail {

/// The VM's memory: globals and frames on a stack, then a heap, in one
/// address space of 64-bit cells, each with an "initialized" flag. Address 0
/// is null; accesses through pointers are checked against the bounds they
/// carry and against freed blocks.
class Memory {
public:
    /// First stack address (globals live at the bottom of the stack).
    static constexpr std::uint32_t kStackBase = 1;
    /// First heap address; the stack ends here.
    static constexpr std::uint32_t kHeapBase = 1U << 20;
    /// Highest addressable heap cell (exclusive).
    static constexpr std::uint32_t kHeapLimit = PackedPointer::kLimit - 1;

    /// Why a memory access failed (None: it did not).
    enum class Fault : std::uint8_t {
        /// The access is valid.
        None,
        /// Through a null pointer.
        Null,
        /// Outside the object the pointer points into.
        OutOfBounds,
        Dangling,  ///< a stack slot of a function that already returned
        /// To a freed heap block.
        UseAfterFree,
        /// Reading a cell never written.
        Uninitialized,
        /// Freeing a block already freed.
        DoubleFree,
        /// Freeing what `new` did not return, or with the wrong `delete` form.
        InvalidDelete,
        /// The heap is full.
        OutOfMemory,
    };

    /// @param global_cells Size of the global area, zero-initialized.
    explicit Memory(std::uint32_t global_cells);

    // --- Stack -------------------------------------------------------------
    /// Address of global cell `offset`.
    [[nodiscard]] std::uint32_t globals_address(std::uint32_t offset) const noexcept { return kStackBase + offset; }
    /// First free stack address.
    [[nodiscard]] std::uint32_t top() const noexcept { return top_; }
    /// Pushes a frame of `cells` uninitialized cells; returns its base.
    /// nullopt when the stack would overflow into the heap.
    [[nodiscard]] std::optional<std::uint32_t> push_frame(std::uint32_t cells);
    /// Pops every frame from `base` up.
    void pop_frame(std::uint32_t base) noexcept { top_ = base; }

    // --- Direct access (addresses already validated) --------------------------
    /// The cell at `address`.
    [[nodiscard]] std::int64_t& cell(std::uint32_t address) noexcept;
    /// The value of the cell at `address`.
    [[nodiscard]] std::int64_t value(std::uint32_t address) const noexcept;
    /// True if the cell at `address` has been written.
    [[nodiscard]] bool initialized(std::uint32_t address) const noexcept;
    /// Marks the cell at `address` as written or not.
    void set_initialized(std::uint32_t address, bool value) noexcept;

    // --- Checked access through pointers ------------------------------------------
    /// Can `cells` cells starting at the pointer be accessed?
    [[nodiscard]] Fault check(std::int64_t pointer, std::uint32_t cells) const;
    /// Reads the cell `pointer` points to into `value`.
    [[nodiscard]] Fault load(std::int64_t pointer, std::int64_t& value) const;
    /// Writes `value` to the cell `pointer` points to.
    [[nodiscard]] Fault store(std::int64_t pointer, std::int64_t value);
    /// Zeroes `cells` cells (`zero`), or marks them uninitialized.
    [[nodiscard]] Fault fill(std::int64_t pointer, std::uint32_t cells, bool zero);
    /// Copies `cells` cells, with their initialized flags.
    [[nodiscard]] Fault copy(std::int64_t destination, std::int64_t source, std::uint32_t cells);

    // --- Heap ------------------------------------------------------------------------
    /// Allocates a heap block of `cells` cells and returns a pointer to it in
    /// `pointer`. `array` records the `new[]` form, checked by release().
    [[nodiscard]] Fault allocate(std::uint32_t cells, bool zero, bool array, std::int64_t& pointer);
    /// Frees the heap block starting at `address`.
    [[nodiscard]] Fault release(std::uint32_t address, bool array);
    /// Heap blocks not freed yet (leaks, at the end of a run).
    [[nodiscard]] std::size_t live_blocks() const noexcept;
    /// Cells of the live heap block that starts at `address`, or nullopt.
    [[nodiscard]] std::optional<std::uint32_t> block_cells(std::uint32_t address) const;

private:
    /// A heap block.
    struct Block {
        /// First address.
        std::uint32_t start = 0;
        /// Size in cells.
        std::uint32_t cells = 0;
        /// Allocated with `new[]`.
        bool array = false;
        /// Not freed yet.
        bool alive = true;
    };
    /// The block containing `address`, or nullptr.
    [[nodiscard]] const Block* block_at(std::uint32_t address) const;

    /// Stack cells.
    std::vector<std::int64_t> stack_;
    /// Initialized flags of the stack cells.
    std::vector<std::uint8_t> stack_init_;
    /// See top().
    std::uint32_t top_ = kStackBase;
    /// Heap cells.
    std::vector<std::int64_t> heap_;
    /// Initialized flags of the heap cells.
    std::vector<std::uint8_t> heap_init_;
    /// Every block ever allocated.
    std::vector<Block> blocks_;
    std::map<std::uint32_t, std::size_t> block_index_;  ///< start -> block
};

}  // namespace cppi::detail
