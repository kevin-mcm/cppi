# 0002. tree-sitter-cpp as the parser

**Status:** Accepted

## Context

Parsing C++ is notoriously hard. Writing our own parser would delay the project by months. Clang (libTooling) is complete but too heavy for a phone.

## Decision

Use **tree-sitter-cpp** to obtain the syntax tree and immediately convert it into our own AST in `src/parse` (Adapter pattern). No other part of the code knows about tree-sitter.

## Consequences

- Lightweight (plain C), portable, already packaged in Conan Center.
- Error tolerant: it marks `ERROR` nodes and `MISSING` tokens, which enables messages like "missing ';' here".
- It does not validate semantics or types: that is the job of `src/sema`.
- It is a GLR parser without a symbol table: on C++ ambiguities (`T(x);` as a call or a declaration) it may choose differently than a compiler. Covered by tests and, if needed, by resolution rules in the adapter.
- Performance: ~2 MB/s on the kind of code the game uses. Enough; see `docs/architecture.md`.
- If it is ever replaced, the change stays contained in `src/parse`.
