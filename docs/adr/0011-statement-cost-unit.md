# 0011. Charging per statement as well as per instruction

**Status:** Accepted

## Context

The operation budget is how a game such as cppi-farm turns code into a resource: a robot's battery, or the stars a solution earns. Until now the only unit was the bytecode instruction (`CostModel::instruction`), with the standard library free (`library_instruction = 0`) and host functions adding their own cost.

Instructions are precise and cheap to count, but they are an implementation detail. `x = a[i] + 1;` is several instructions, and how many changes whenever code generation improves (the work that cut cppi-farm's shortest path solution from 12,973 to 9,222 operations changed every level's numbers). A player cannot predict them, and a level designer cannot explain them: cppi-farm's README said "each statement adds about 2 instructions", which was not true. A BFS over a 7×5 map costing about 10,000 operations is opaque.

## Decision

`CostModel` gets a `unit`:

- `Unit::Instruction` (the default, so nothing changes for existing hosts): every instruction of the player's code costs `instruction`.
- `Unit::Statement`: every statement of the player's code costs `statement` (1 by default) each time it runs, and so does every evaluation of the condition of an `if`, `while`, `for`, `do`-`while` or `switch`. Compound statements (blocks, the `if` or loop itself, `try`) cost nothing beyond their conditions. In a `for`, the init statement is a statement and the update is part of the loop header, like the condition; `for (;;)` charges each jump back to the top, so an empty endless loop still runs out of operations.

In both units, standard library instructions cost `library_instruction`, and host functions add their own cost.

The binder marks the first bound statement produced from each simple statement the player wrote (expression, declaration, `return`, `break`, `continue`, `throw`). Code generation records, in `ProgramData::statement_starts` (parallel to the code), the first instruction of each marked statement and of each condition evaluation; in the statement unit the VM charges only those instructions. Jump threading never skips one of them.

## Consequences

- "One line, one operation" is close to what the player sees: a loop of 10 iterations with 2 statements costs 1 + 11 + 20 = 32 (`int i = 0`, eleven tests, twenty statements), and the line profile (`RunResult::line_operations`) reads as how many times each line ran. cppi-farm's shortest path costs 1,088 operations, the travelling salesman 18,255, memoization 824.
- Numbers no longer depend on code generation: optimizations make programs faster without changing their cost, and a level's budget stays valid across cppi versions.
- A statement is charged before it runs, so the budget can only run out where a statement or a condition starts, or before a host call (which still adds its own cost).
- The cost no longer reflects how much work a statement does: `v = big_function_result();` and `x = 1;` cost the same, and a long expression is as cheap as a short one. Expensive work is still visible through the statements of the functions it calls, and host functions keep their own cost.
- The default stays `Unit::Instruction`, so existing levels keep their numbers; a host opts in per run (`RunOptions::cost.unit`) and recalibrates its budgets when it does.
