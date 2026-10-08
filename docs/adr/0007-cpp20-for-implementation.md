# 0007. C++20 to implement the interpreter

**Status:** Accepted

## Context

The interpreter *interprets* from C++98 onward, but its own code can use any standard supported by the compilers of every target platform.

## Decision

Implement in **C++20** (`std::span`, `operator<=>`, `starts_with`, heterogeneous lookup in maps), avoiding `<format>`, `std::expected`, and modules for now, since their support is still uneven across the NDK's libc++, AppleClang, and MSVC.

## Consequences

- Minimum compilers: GCC 11, Clang 14, AppleClang 15, MSVC 19.30, NDK r26.
- Revisit this decision when every target supports C++23 reliably.
