# 0009. Undefined behavior as a game mechanic

**Status:** Accepted

## Context

Real C++ programs that read uninitialized memory, index out of bounds or use a pointer after `delete` may crash, may keep running with garbage, or may appear to work. For a learner this is the hardest part of C++, and for the game it is a safety problem: player code must never corrupt the host.

## Decision

The VM never executes undefined behavior. It detects it and stops the program with a `RuntimeError` and a 5xx diagnostic at the exact line (see `docs/diagnostics.md`):

- Memory is made of cells with an *initialized* bit, so uninitialized reads are caught (E0502).
- Pointers carry the bounds of the object they point into (packed into one cell, 21 bits each), and heap blocks remember whether they are alive and whether they are arrays. This catches out-of-bounds access through pointers, dangling pointers to finished stack frames, use after free, double free and mismatched `delete` (E0503–E0507, E0512).
- Signed overflow, division by zero and invalid shifts are checked (E0500, E0501, E0509).
- Reaching the end of a non-`void` function, calling a pure virtual function and reading an empty `std::optional` are reported too (E0508, E0511, E0513).
- Leaks are a *warning* when the program ends (E0510).

## Consequences

- Undefined behavior becomes something the game can teach and even design levels around ("the automaton read a variable nobody watered").
- The VM is slower than an unchecked one (every load and store is checked), which is acceptable: player programs are small and the budget limits them anyway.
- Implementation-defined details are fixed by the interpreter, not by the host machine, so a program behaves the same on every platform.
- The address space is limited by the 21-bit pointer fields: about one million cells for globals and the stack, and as many for the heap. Exceeding it is reported as E0403.
