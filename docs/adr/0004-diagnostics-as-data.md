# 0004. Diagnostics as codes with arguments

**Status:** Accepted

## Context

The game is bilingual (Spanish and English) and could add more languages. If the interpreter generated text messages, localization would be mixed into the compiler.

## Decision

Every problem is reported as `Diagnostic { code, severity, range, args }`, with stable numeric codes (`E0200`), stable keys (`unknown-function`), and named arguments (`name`, `suggestion`). The interpreter **produces no text for the player**.

## Consequences

- Translation lives in a single place: the game (Godot CSV files or `.po`).
- Codes are a public contract: they are never reused or renumbered. See `docs/diagnostics.md`.
- `to_debug_string()` exists only for logs and tests.
- Tests check codes and arguments, not sentences: wording can improve without breaking anything.
