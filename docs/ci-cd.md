# CI/CD, releases, and deployment

## GitHub Actions workflows

| Workflow | When | What it does |
|---|---|---|
| `ci.yml` | push to `master`, every PR | Formatting; build and tests on every platform (table below); ASan + UBSan; clang-tidy; coverage (minimum 85%); 60 s of fuzzing. |
| `benchmarks.yml` | push to `master`, every PR | Runs the benchmarks and compares them with the history. Fails the PR and comments on it if anything regresses by more than 50%. On `master`, stores the result in the `gh-pages` branch. |
| `nightly.yml` | every night | 30 minutes of fuzzing with a persistent corpus. |
| `release.yml` | on pushing a `vX.Y.Z` tag | Checks the version, publishes the Conan package, and creates a GitHub Release with `cppi-run` binaries. |

### Platform matrix

All warnings are errors (`CPPI_WARNINGS_AS_ERRORS=ON`).

macOS and Windows runners are not in CI yet, so build and test on those platforms locally before a release.

| Platform | Runner | How it is tested |
|---|---|---|
| Linux x64 (GCC and Clang, Debug and Release) | `ubuntu-24.04` | Native tests |
| Linux arm64 | `ubuntu-24.04-arm` | Native tests |
| Linux x86 32-bit | `ubuntu-24.04` + multilib | Native tests (catches pointer-width bugs that would affect armv7 and WebAssembly) |
| Android x86_64 | `ubuntu-24.04` + API 30 emulator | Unit tests inside the emulator |
| Android arm64-v8a, armeabi-v7a | `ubuntu-24.04` + NDK | Cross-compilation |
| WebAssembly (Emscripten) | `ubuntu-24.04` | Tests and golden cases under Node.js (without the thread tests) |

`actionlint` validates the workflows.

Every job uses the composite action `.github/actions/setup-conan`, which installs Conan, detects the profile, and restores the package cache.

### What blocks a merge

In GitHub (*Settings → Branches → Branch protection* for `master`), make these checks required:

- `Format`
- Every `build` combination
- `Sanitizers (ASan + UBSan)`
- `clang-tidy`
- `Coverage`
- `Fuzz (60 s)`
- `benchmark`

## Versioning

- [SemVer](https://semver.org/). While the version is `0.x`, the API may change between minor versions, and releases are marked as *pre-release*.
- **Single source for the version:** `project(VERSION ...)` in `CMakeLists.txt`. `conanfile.py` reads it from there and `cppi/version.hpp` is generated from it.
- Commits follow [Conventional Commits](https://www.conventionalcommits.org/) (`feat:`, `fix:`, `perf:`, `docs:`...) so the release notes read on their own.

## How to publish a release

1. Update `VERSION` in `CMakeLists.txt` and add the entry to `CHANGELOG.md`.
2. Merge into `master` with CI green.
3. Create and push the tag:
   ```bash
   git tag v0.2.0
   git push origin v0.2.0
   ```
4. `release.yml` does the rest. If the tag does not match the project version, it fails without publishing anything.

## Deploying the Conan package

GitHub Packages does not support Conan, so the package is published to our own Conan remote. Options:

- **JFrog Artifactory** (self-hosted Community Edition, or the cloud with a free plan).
- **GitLab Package Registry** (supports Conan 2).
- **Cloudsmith** or another compatible service.

Configure these repository *secrets*:

| Secret | Example |
|---|---|
| `CONAN_REMOTE_URL` | `https://my-company.jfrog.io/artifactory/api/conan/conan-local` |
| `CONAN_LOGIN_USERNAME` | deploy user or token |
| `CONAN_PASSWORD` | password or token |

Without `CONAN_REMOTE_URL`, the release is still created and the upload step is skipped with a warning.

The game consumes the package by adding that remote:

```bash
conan remote add cppi-remote <URL>
```

## Performance history

`benchmarks.yml` uses [github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark). Enable GitHub Pages from the `gh-pages` branch to see the charts at `https://<user>.github.io/<repo>/dev/bench/`. The first run on `master` creates the `gh-pages` branch; until it exists, pull requests skip the comparison.

## Reproducing CI locally

```bash
# arm64 without ARM hardware: cross-compilation + qemu (g++-aarch64-linux-gnu, qemu-user)
conan install . -pr:h conan/profiles/linux-aarch64-cross -pr:b default -s build_type=Release --build=missing \
  -c "tools.cmake.cmake_layout:build_folder_vars=['settings.arch']"
cmake --preset conan-armv8-release && cmake --build --preset conan-armv8-release && ctest --preset conan-armv8-release

# 32-bit (g++-multilib)
conan install . -pr:h conan/profiles/linux-x86 -pr:b default -s build_type=Debug --build=missing \
  -c "tools.cmake.cmake_layout:build_folder_vars=['settings.arch']"
cmake --preset conan-x86-debug && cmake --build --preset conan-x86-debug && ctest --preset conan-x86-debug

# Validate the workflows
actionlint
```

```bash
# Sanitizers (requires Clang with compiler-rt)
conan install . -pr:h conan/profiles/linux-clang -pr:b default -s build_type=Debug --build=missing
cmake --preset conan-debug -DCPPI_WARNINGS_AS_ERRORS=ON "-DCPPI_SANITIZERS=address;undefined"
cmake --build --preset conan-debug && ctest --preset conan-debug

# Formatting and static analysis
find include src tools tests bench fuzz \( -name '*.cpp' -o -name '*.hpp' \) | xargs clang-format -i
run-clang-tidy -p build/Debug -quiet $(find src tools -name '*.cpp')

# Update the golden cases after intentionally changing a message
CPPI_UPDATE_GOLDEN=1 cmake --preset conan-debug && ctest --preset conan-debug -L golden
cmake --preset conan-debug   # back to verification mode
```
