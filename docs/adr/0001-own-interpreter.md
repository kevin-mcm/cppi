# 0001. Own interpreter instead of a compiler

**Status:** Accepted

## Context

The game must work **offline** on Android, iOS, Windows, and Linux. The alternatives were:

- Compiling with Clang to WebAssembly on the device: tens of MB, slow compilation on modest phones.
- Compiling on a server: requires a connection and incurs costs.
- Cling / clang-repl: depend on LLVM and JIT, which iOS does not allow.

## Decision

Write our own interpreter for a subset of C++ that grows in phases, following the game's progression.

## Consequences

- Works offline on every platform, with no JIT or special permissions.
- Full control: counting operations, locking features, step-by-step execution, simulating undefined behavior as a game mechanic.
- Player code never touches real memory: it cannot break the game.
- Cost: implementing C++ semantics and a minimal standard library is large, ongoing work. Mitigated by growing only what each stage of the game needs.
- Risk: subtle differences from real C++. Mitigated with tests that compare against standard behavior.
