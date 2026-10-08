# 0010. Script mode: re-parsing failed top-level items as a block

**Status:** Accepted (extends [0006](0006-script-mode.md))

## Context

tree-sitter-cpp parses a translation unit, where only declarations belong at the top level. It tolerates many statements there (`harvest();`), but some read like broken declarations: `std::cout << x;` or `x = x + 1;` produce error nodes at the top level although they are valid inside a function.

## Decision

The parser reads the source twice when needed: as it is, and wrapped in `{ ... }` as the body of a block. Each top-level item that failed in the first reading is taken from the block reading if it is clean there. Positions are kept: only the first line moves, by the inserted `{`, and that shift is undone when converting. Items that succeed at the top level keep their top-level meaning (functions, classes, globals).

Together with this, script mode gained two rules: top-level declarations are globals, and if the program defines `int main()`, it runs after the loose statements. `#include` lines are accepted and ignored.

## Consequences

- Any statement that is valid inside a function is valid at the top level, so early levels do not need to explain the difference.
- Programs written as standard C++ (with `#include` and `int main()`) run unchanged.
- A program with real syntax errors is parsed twice; only the first reading's errors are reported unless the block reading fixes the item.
