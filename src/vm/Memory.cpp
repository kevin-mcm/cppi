#include "vm/Memory.hpp"

#include <algorithm>
#include <cstddef>

namespace cppi::detail {

Memory::Memory(std::uint32_t global_cells) : top_(kStackBase + global_cells) {
    stack_.assign(top_, 0);
    stack_init_.assign(top_, 1);  // globals are zero-initialized
    stack_init_[0] = 0;
}

std::optional<std::uint32_t> Memory::push_frame(std::uint32_t cells) {
    const std::uint64_t new_top = std::uint64_t{top_} + cells;
    if (new_top > kHeapBase) {
        return std::nullopt;
    }
    const std::uint32_t base = top_;
    top_ = static_cast<std::uint32_t>(new_top);
    if (stack_.size() < top_) {
        stack_.resize(std::max<std::size_t>(top_, stack_.size() * 2), 0);
        stack_init_.resize(stack_.size(), 0);
    }
    std::fill(stack_init_.begin() + static_cast<std::ptrdiff_t>(base),
              stack_init_.begin() + static_cast<std::ptrdiff_t>(top_), std::uint8_t{0});
    return base;
}

std::int64_t& Memory::cell(std::uint32_t address) noexcept {
    return address >= kHeapBase ? heap_[address - kHeapBase] : stack_[address];
}

std::int64_t Memory::value(std::uint32_t address) const noexcept {
    return address >= kHeapBase ? heap_[address - kHeapBase] : stack_[address];
}

bool Memory::initialized(std::uint32_t address) const noexcept {
    return (address >= kHeapBase ? heap_init_[address - kHeapBase] : stack_init_[address]) != 0;
}

void Memory::set_initialized(std::uint32_t address, bool value) noexcept {
    (address >= kHeapBase ? heap_init_[address - kHeapBase] : stack_init_[address]) = value ? 1 : 0;
}

const Memory::Block* Memory::block_at(std::uint32_t address) const {
    auto it = block_index_.upper_bound(address);
    if (it == block_index_.begin()) {
        return nullptr;
    }
    --it;
    const Block& b = blocks_[it->second];
    return address < b.start + std::max<std::uint32_t>(1, b.cells) ? &b : nullptr;
}

Memory::Fault Memory::check(std::int64_t pointer, std::uint32_t cells) const {
    if (pointer == 0) {
        return Fault::Null;
    }
    const std::uint32_t address = PackedPointer::address(pointer);
    const std::uint32_t begin = PackedPointer::begin(pointer);
    const std::uint32_t end = PackedPointer::end(pointer);
    if (address < begin || std::uint64_t{address} + cells > end || cells == 0) {
        return Fault::OutOfBounds;
    }
    if (address >= kHeapBase) {
        const Block* b = block_at(address);
        if (b == nullptr) {
            return Fault::Dangling;
        }
        if (!b->alive) {
            return Fault::UseAfterFree;
        }
        return std::uint64_t{address} + cells <= std::uint64_t{b->start} + b->cells ? Fault::None : Fault::OutOfBounds;
    }
    if (address < kStackBase || std::uint64_t{address} + cells > top_) {
        return Fault::Dangling;
    }
    return Fault::None;
}

Memory::Fault Memory::load(std::int64_t pointer, std::int64_t& value) const {
    if (const Fault f = check(pointer, 1); f != Fault::None) {
        return f;
    }
    const std::uint32_t address = PackedPointer::address(pointer);
    if (!initialized(address)) {
        return Fault::Uninitialized;
    }
    value = address >= kHeapBase ? heap_[address - kHeapBase] : stack_[address];
    return Fault::None;
}

Memory::Fault Memory::store(std::int64_t pointer, std::int64_t value) {
    if (const Fault f = check(pointer, 1); f != Fault::None) {
        return f;
    }
    const std::uint32_t address = PackedPointer::address(pointer);
    cell(address) = value;
    set_initialized(address, true);
    return Fault::None;
}

Memory::Fault Memory::fill(std::int64_t pointer, std::uint32_t cells, bool zero) {
    if (cells == 0) {
        return Fault::None;
    }
    if (const Fault f = check(pointer, cells); f != Fault::None) {
        return f;
    }
    const std::uint32_t address = PackedPointer::address(pointer);
    for (std::uint32_t i = 0; i < cells; ++i) {
        if (zero) {
            cell(address + i) = 0;
        }
        set_initialized(address + i, zero);
    }
    return Fault::None;
}

Memory::Fault Memory::copy(std::int64_t destination, std::int64_t source, std::uint32_t cells) {
    if (cells == 0) {
        return Fault::None;
    }
    if (const Fault f = check(source, cells); f != Fault::None) {
        return f;
    }
    if (const Fault f = check(destination, cells); f != Fault::None) {
        return f;
    }
    const std::uint32_t dst = PackedPointer::address(destination);
    const std::uint32_t src = PackedPointer::address(source);
    if (dst == src) {
        return Fault::None;
    }
    // Overlap-safe: copy through a buffer.
    std::vector<std::pair<std::int64_t, bool>> buffer(cells);
    for (std::uint32_t i = 0; i < cells; ++i) {
        buffer[i] = {cell(src + i), initialized(src + i)};
    }
    for (std::uint32_t i = 0; i < cells; ++i) {
        cell(dst + i) = buffer[i].first;
        set_initialized(dst + i, buffer[i].second);
    }
    return Fault::None;
}

Memory::Fault Memory::allocate(std::uint32_t cells, bool zero, bool array, std::int64_t& pointer) {
    const std::uint32_t size = std::max<std::uint32_t>(1, cells);
    const std::uint64_t start = kHeapBase + heap_.size();
    if (start + size > kHeapLimit) {
        return Fault::OutOfMemory;
    }
    heap_.resize(heap_.size() + size, 0);
    heap_init_.resize(heap_.size(), zero ? 1 : 0);
    blocks_.push_back(Block{static_cast<std::uint32_t>(start), cells, array, true});
    block_index_.emplace(static_cast<std::uint32_t>(start), blocks_.size() - 1);
    pointer = PackedPointer::pack(static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(start),
                                  static_cast<std::uint32_t>(start + cells));
    return Fault::None;
}

Memory::Fault Memory::release(std::uint32_t address, bool array) {
    if (address < kHeapBase) {
        return Fault::InvalidDelete;
    }
    auto it = block_index_.find(address);
    if (it == block_index_.end()) {
        return Fault::InvalidDelete;
    }
    Block& b = blocks_[it->second];
    if (!b.alive) {
        return Fault::DoubleFree;
    }
    if (b.array != array) {
        return Fault::InvalidDelete;
    }
    b.alive = false;
    return Fault::None;
}

std::optional<std::uint32_t> Memory::block_cells(std::uint32_t address) const {
    auto it = block_index_.find(address);
    if (it == block_index_.end() || !blocks_[it->second].alive) {
        return std::nullopt;
    }
    return blocks_[it->second].cells;
}

std::size_t Memory::live_blocks() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(blocks_.begin(), blocks_.end(), [](const Block& b) { return b.alive; }));
}

}  // namespace cppi::detail
