#include "codegen/ConstantPool.hpp"

namespace cppi::detail {

std::uint32_t ConstantPool::intern(std::int64_t bits, std::string text) {
    auto key = std::pair{bits, text};
    if (auto it = index_.find(key); it != index_.end()) {
        return it->second;
    }
    const auto index = static_cast<std::uint32_t>(constants_.size());
    constants_.push_back(ConstantEntry{bits, std::move(text)});
    index_.emplace(std::move(key), index);
    return index;
}

}  // namespace cppi::detail
