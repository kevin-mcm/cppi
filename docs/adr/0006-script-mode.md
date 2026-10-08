# 0006. Script mode: top-level statements

**Status:** Accepted

## Context

In real C++, executable code lives inside functions (`int main()`). But in the first levels the player does not even have their own functions unlocked.

## Decision

Player code is a sequence of top-level statements, executed in order. tree-sitter-cpp already accepts this format. When user-defined functions are unlocked, definitions and statements can coexist; loose statements remain the entry point.

## Consequences

- The first levels are as simple as `harvest(); move(East);`.
- It is a deliberate deviation from standard C++. The game can present it as: "your program is the body of `main`".
- Later, an option could require `int main()` in advanced levels to close the gap.
- Update: programs may also define `int main()`, which runs after the loose statements; see [0010](0010-script-mode-parsing-fallback.md).
