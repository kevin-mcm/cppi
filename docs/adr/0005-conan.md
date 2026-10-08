# 0005. Conan 2 for dependencies and distribution

**Status:** Accepted

## Context

The interpreter is a separate library that the game links at build time, on five platforms, including cross-compilation for Android and iOS.

## Decision

Use **Conan 2** both to fetch dependencies (tree-sitter, GoogleTest — Catch2 until October 2026 —, Google Benchmark) and to **publish `cppi` as a package** consumed by the game.

## Consequences

- Per-platform profiles (`conan/profiles/`) handle cross-compiling the dependencies.
- `conan create` validates the package with `test_package/`, the same way the game would use it.
- The version has a single source: `project(VERSION ...)` in `CMakeLists.txt`.
- Publishing requires our own Conan remote (Artifactory CE, GitLab, Cloudsmith...). See `docs/ci-cd.md`.
- Developers need Python and Conan installed.
