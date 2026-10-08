# 0003. Bytecode virtual machine

**Status:** Accepted

## Context

A tree-walking interpreter is simpler to write; a bytecode VM is faster and makes every step explicit.

## Decision

Compile to our own bytecode and run it on a stack machine, from phase 1.

## Consequences

- Counting operations is natural: each instruction has a cost.
- Step-by-step execution (`Execution::step`) and the visual debugger come almost for free.
- A compiled `Program` is immutable and reusable across runs and across threads.
- The bytecode is "teachable": `cppi-run --dump-bytecode` shows how the code is translated.
- Cost: one more stage (the generator). It stays simple because semantic analysis has already resolved names and types.
