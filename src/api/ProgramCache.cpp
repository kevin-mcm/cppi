/// @file ProgramCache.cpp
/// @brief Implementation of ProgramCache.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/ProgramCache.hpp>

#include <cstdint>

namespace cppi {

namespace {

/// Everything besides the source that changes what compile() returns.
std::string fingerprint(const Interpreter& interpreter) {
    const Options& o = interpreter.options();
    std::string key = std::to_string(reinterpret_cast<std::uintptr_t>(&interpreter.host())) + "|" +
                      std::to_string(static_cast<int>(o.standard)) + "|";
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        key.push_back(o.locked.contains(static_cast<Feature>(i)) ? '1' : '0');
    }
    return key + "|";
}

}  // namespace

CompileResult ProgramCache::compile(const Interpreter& interpreter, std::string_view source) {
    std::string key = fingerprint(interpreter);
    key.append(source);
    {
        const std::lock_guard lock(mutex_);
        if (auto it = index_.find(key); it != index_.end()) {
            entries_.splice(entries_.begin(), entries_, it->second);
            ++hits_;
            return it->second->second;
        }
        ++misses_;
    }
    CompileResult result = interpreter.compile(source);  // outside the lock: compiling can take a while
    const std::lock_guard lock(mutex_);
    if (index_.find(key) == index_.end()) {
        entries_.emplace_front(key, result);
        index_.emplace(std::move(key), entries_.begin());
        while (entries_.size() > capacity_) {
            index_.erase(entries_.back().first);
            entries_.pop_back();
        }
    }
    return result;
}

std::size_t ProgramCache::size() const {
    const std::lock_guard lock(mutex_);
    return entries_.size();
}

std::size_t ProgramCache::hits() const {
    const std::lock_guard lock(mutex_);
    return hits_;
}

std::size_t ProgramCache::misses() const {
    const std::lock_guard lock(mutex_);
    return misses_;
}

void ProgramCache::clear() {
    const std::lock_guard lock(mutex_);
    entries_.clear();
    index_.clear();
}

}  // namespace cppi
