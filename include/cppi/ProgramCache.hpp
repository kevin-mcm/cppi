#pragma once

/// @file ProgramCache.hpp
/// @brief Remembers compiled programs, so pressing "Run" again without editing
/// the code does not compile it again.
///
/// Keyed by the source, the level's rules and the host registry. Thread-safe;
/// least recently used entries go first.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

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

/// A thread-safe, least-recently-used cache of compile results, keyed by
/// source text.
class ProgramCache {
public:
    /// @param capacity Maximum number of programs kept (at least 1).
    explicit ProgramCache(std::size_t capacity = 32) : capacity_(capacity == 0 ? 1 : capacity) {}

    /// The result of `interpreter.compile(source)`, compiling only on a miss.
    /// The cache does not know the interpreter's options: use one cache per
    /// interpreter configuration.
    [[nodiscard]] CompileResult compile(const Interpreter& interpreter, std::string_view source);

    /// Number of cached programs.
    [[nodiscard]] std::size_t size() const;
    /// Number of compile() calls answered from the cache.
    [[nodiscard]] std::size_t hits() const;
    /// Number of compile() calls that had to compile.
    [[nodiscard]] std::size_t misses() const;
    /// Forgets every cached program (the statistics are kept).
    void clear();

private:
    /// Source text and its compile result.
    using Entry = std::pair<std::string, CompileResult>;

    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::list<Entry> entries_;  // most recently used first
    std::unordered_map<std::string, std::list<Entry>::iterator> index_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
};

}  // namespace cppi
