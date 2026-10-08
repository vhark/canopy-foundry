# F01 source-build evidence

**Case:** `F01-source-build`. **Status:** local checks and the three-platform native source matrix passed; F01 is not complete. This report does not qualify Unreal, a game package, BIM exchange, simulation accuracy or performance.

**Code checkpoint:** [`78ed18f2034c2c4a4769d1c244fb4694c29be302`](https://github.com/vhark/canopy-foundry/commit/78ed18f2034c2c4a4769d1c244fb4694c29be302). The native manifests also bind the actual input files, compiler binary, dependency bootstrap and output library by SHA-256. No simulation model, recipe, random seed or rendering settings apply to this source-build case; gameplay and performance measurements are **not measured**.

## Local native execution

Observed host: macOS arm64, Xcode 27.0 build `27A266a`, Apple Clang `21.0.0 (clang-2100.3.34.2)`, macOS SDK 27.0. The build scripts select the canonical SDK path explicitly and verify the generated compiler arguments and actual archive's arm64 architecture. These are development-host observations, not the approved Xcode 26.1.1 qualification.

| Execution | Observed result |
|---|---|
| `uv sync --frozen --all-packages --all-extras` | Pinned workspace and immutable OpenCEA/GrowBIM wheels installed |
| `uv run --frozen python scripts/bootstrap_native.py` | Verified pinned checkout, port/source hashes and native vcpkg executable; successful reuse after integrity probe removal |
| `uv run --frozen python scripts/build_core.py --config Debug` followed by `uv run --frozen ctest --test-dir .build/core --output-on-failure` | Native library/test executable built; 4/4 conversion cases passed |
| The same build/test commands with `--config Release` | Native library/test executable built; 4/4 cases passed |
| `uv run --frozen cmake --preset native-asan-ubsan`, `uv run --frozen cmake --build --preset native-asan-ubsan`, `uv run --frozen ctest --preset native-asan-ubsan` | ASan/UBSan build; 4/4 cases passed |
| `uv run --frozen python -m pytest tests/build -q` | 30 passed |
| Separately compiled C++ consumer linked against `libcanopy_core.a` | `60000 square feet = 5574.1824 square metres`, exit 0; throwaway consumer removed afterward |
| `uv run --frozen growbim --help` | Real pinned CLI started and listed its supported commands; no new IFC profile or Windows creator qualification claimed |

The area cases use the public core API, including finite conversion, signed/zero inputs, non-finite propagation and deterministic linearity/order properties. They are not copied arithmetic in fixture code.

## Failure and recovery evidence

- An owned untracked file added to the pinned vcpkg checkout was rejected by bootstrap, the core build preflight and direct CMake configuration. Removing the probe restored verified bootstrap reuse and successful native builds.
- The bootstrap regression checks the injected fetch failure rather than an incidental filesystem error. Failed bootstrap attempts do not erase the last valid provenance record.
- A pre-correction CMake cache selecting Intel `x86_64`/`x64-osx` produced Intel build outputs on this arm64 host and failed during Catch2 test discovery with system error `-86` (Intel execution unavailable). No successful mislabeled manifest was observed on this host. After correction, the build entrypoint overrode that stale selection with arm64/`arm64-osx`; native tests and the separately linked consumer then ran successfully. `lipo -archs` reported `arm64` before manifest publication.
- Five macOS target-rejection cases failed before implementation, then passed in the 30-case suite. They cover wrong/missing compiler architecture, wrong/missing effective SDK sysroot and an incompatible dependency triplet. The generated compile commands and actual selected SDK are checked, not just the host's default SDK version string.
- Game packaging on the current checkout exits nonzero because `game/CanopyFoundry.uproject` does not exist. The engine patch and approved SDK rejection cases are exercised by the orchestration tests. No UAT cook or game executable is claimed.

Independent native/bootstrap and build-orchestration spec reviews, followed by quality reviews and targeted correction rechecks, returned no outstanding findings. Review is not a substitute for execution or qualification.

## Hosted native source checks

The private [source matrix run](https://github.com/vhark/canopy-foundry/actions/runs/37705605555) passed on 2026-10-08 UTC at the code checkpoint above. All three jobs performed frozen environment resolution, pinned bootstrap, native Debug compilation and test execution:

| Native runner | Observed compiler / SDK / core runtime | CTest | Python build checks | Retained manifest |
|---|---|---|---|---|
| `ubuntu-24.04`, x64 | GNU 13.3.0 (`13.3.0-6ubuntu2~24.04.1`), glibc2.39, libstdc++ | 4 passed | 30 passed | [linux-x64](f01-source-manifests/linux-x64.json) |
| `windows-2025`, x64 | VS18.0, MSVC toolset14.51.36231/compiler19.51.36260, SDK10.0.26100.0, MDd | 4 passed | 26 passed, 4 skipped | [win64](f01-source-manifests/win64.json) |
| `macos-15`, arm64 | Xcode16.4 build16F6, Apple Clang17.0.0 (`clang-1700.0.13.5`), SDK15.5, libc++ | 4 passed | 30 passed | [mac-arm64](f01-source-manifests/mac-arm64.json) |

The four Windows skips are POSIX executable-permission cases, exercised on both POSIX hosts. All manifests record `unreal_qualified: false`; the Linux default compiler, Windows14.51 toolset and Mac16.4 SDK selection are explicitly **not** the approved Unreal baseline. The committed manifests are unchanged copies of the downloaded run artifacts, preserving compiler/SDK identities and SHA-256 evidence after the hosted artifacts expire.

The workflow requires no engine or vendor credentials and retains only explicitly selected bootstrap/core manifests and native test logs for 30 days. Its first run reported Node20 action-runtime deprecation. Checkout and artifact upload were moved to verified immutable [checkout v7.0.1](https://github.com/actions/checkout/releases/tag/v7.0.1) and [upload-artifact v7.0.2](https://github.com/actions/upload-artifact/releases/tag/v7.0.2), both using Node24. The [follow-up native matrix](https://github.com/vhark/canopy-foundry/actions/runs/37706605327), at workflow-only commit `d6e029fdd6fc982d817885eae79bf7d602b67b54`, passed on all three runners, including evidence upload. The core/build-script source is unchanged from the first checkpoint.

## Remaining hard gate

The user retained the approved Unreal 5.8.1 / Xcode 26.1.1 / VS2026-MSVC14.50-SDK10.0.26100 / Linux v26 Clang20.1.8 fixed-sysroot baseline. The inspected installation is UE5.8.0 with Xcode27; authorized UE5.8.1 artifacts and approved native engine runners are unavailable. Hosted source CI cannot certify those engine/compiler ABIs or GPU/runtime acceptance.

F01 remains open. F02 and the later implementation packages retain their hard completion prerequisites. The first-/third-person and overhead camera requirements are documented in architecture A13 and the F03/X04/native acceptance tasks; no camera runtime has been implemented or exercised.
