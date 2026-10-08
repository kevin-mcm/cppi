#pragma once

/// @file ProgramCache.hpp
/// Remembers compiled programs, so pressing "Run" again without editing the
/// code does not compile it again. Keyed by the source, the level's rules
/// and the host registry. Thread-safe; least recently used entries go first.

#include <cppi/CompileResult.hpp>
#include <cppi/Interpreter.hpp>

#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace cppi {

class ProgramCache {
public:
    explicit ProgramCache(std::size_t capacity = 32) : capacity_(capacity == 0 ? 1 : capacity) {}

    /// The result of `interpreter.compile(source)`, compiling only on a miss.
    [[nodiscard]] CompileResult compile(const Interpreter& interpreter, std::string_view source);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t hits() const;
    [[nodiscard]] std::size_t misses() const;
    void clear();

private:
    using Entry = std::pair<std::string, CompileResult>;

    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::list<Entry> entries_;  // most recently used first
    std::unordered_map<std::string, std::list<Entry>::iterator> index_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
};

}  // namespace cppi
