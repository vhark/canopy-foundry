# F01 source-build evidence

**Case:** `F01-source-build`. **Status:** local checks and the three-platform native source matrix with approved SDK selections passed; F01 is not complete. This report does not qualify Unreal, a game package, BIM exchange, simulation accuracy or performance.

**Initial source checkpoint:** [`78ed18f2034c2c4a4769d1c244fb4694c29be302`](https://github.com/vhark/canopy-foundry/commit/78ed18f2034c2c4a4769d1c244fb4694c29be302). The approved-SDK checkpoint is recorded separately below. Native manifests bind the actual input files, compiler binary, dependency bootstrap and output library by SHA-256. No simulation model, recipe, random seed or rendering settings apply to this source-build case; gameplay and performance measurements are **not measured**.

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

## Initial hosted native source checks

The private [source matrix run](https://github.com/vhark/canopy-foundry/actions/runs/37705605555) passed on 2026-10-08 UTC at the code checkpoint above. All three jobs performed frozen environment resolution, pinned bootstrap, native Debug compilation and test execution:

| Native runner | Observed compiler / SDK / core runtime | CTest | Python build checks | Retained manifest |
|---|---|---|---|---|
| `ubuntu-24.04`, x64 | GNU 13.3.0 (`13.3.0-6ubuntu2~24.04.1`), glibc2.39, libstdc++ | 4 passed | 30 passed | [linux-x64](f01-source-manifests/linux-x64.json) |
| `windows-2025`, x64 | VS18.0, MSVC toolset14.51.36231/compiler19.51.36260, SDK10.0.26100.0, MDd | 4 passed | 26 passed, 4 skipped | [win64](f01-source-manifests/win64.json) |
| `macos-15`, arm64 | Xcode16.4 build16F6, Apple Clang17.0.0 (`clang-1700.0.13.5`), SDK15.5, libc++ | 4 passed | 30 passed | [mac-arm64](f01-source-manifests/mac-arm64.json) |

The four Windows skips are POSIX executable-permission cases, exercised on both POSIX hosts. All manifests record `unreal_qualified: false`; the Linux default compiler, Windows14.51 toolset and Mac16.4 SDK selection are explicitly **not** the approved Unreal baseline. The committed manifest copies preserve all downloaded compiler/SDK identities and SHA-256 values after hosted artifacts expire. Git normalizes the Windows report's CRLF line endings to LF; the original artifact bytes remain available in the linked run during its retention period.

The workflow requires no engine or vendor credentials and retains only explicitly selected bootstrap/core manifests and native test logs for 30 days. Its first run reported Node20 action-runtime deprecation. Checkout and artifact upload were moved to verified immutable [checkout v7.0.1](https://github.com/actions/checkout/releases/tag/v7.0.1) and [upload-artifact v7.0.2](https://github.com/actions/upload-artifact/releases/tag/v7.0.2), both using Node24. The [follow-up native matrix](https://github.com/vhark/canopy-foundry/actions/runs/37706605327), at workflow-only commit `d6e029fdd6fc982d817885eae79bf7d602b67b54`, passed on all three runners, including evidence upload. The core/build-script source is unchanged from the first checkpoint.

## Approved SDK source checks

The [approved-SDK source matrix](https://github.com/vhark/canopy-foundry/actions/runs/37714748308) passed on 2026-10-08 UTC at [`96800c19f8f1618925a28c0e51be2c374acd0f90`](https://github.com/vhark/canopy-foundry/commit/96800c19f8f1618925a28c0e51be2c374acd0f90). Each native job resolved the frozen environment, bootstrapped pinned dependencies, checked the selected compiler against the approved SDK profile, built Debug and Release, ran both native test binaries and ran the Python build checks.

| Native runner | Observed approved compiler / SDK selection | Debug / Release CTest | Python build checks | Preserved manifests |
|---|---|---|---|---|
| `ubuntu-24.04`, x64 | Epic v26 Clang20.1.8, fixed `x86_64-unknown-linux-gnu` sysroot, bundled libc++/libc++abi | 4/4 passed in each | 30 passed | [Debug](f01-source-manifests/approved-sdk/linux-x64-debug.json), [Release](f01-source-manifests/approved-sdk/linux-x64-release.json), [bootstrap](f01-source-manifests/approved-sdk/linux-x64-bootstrap.json) |
| `windows-2025`, x64 | VS18.0, MSVC toolset14.50.35717/compiler19.50.35739, SDK10.0.26100.0; MDd / MD | 4/4 passed in each | 26 passed, 4 POSIX-only skips | [Debug](f01-source-manifests/approved-sdk/win64-debug.json), [Release](f01-source-manifests/approved-sdk/win64-release.json), [bootstrap](f01-source-manifests/approved-sdk/win64-bootstrap.json) |
| `macos-15`, arm64 | Xcode26.1.1 build17B100, Apple Clang17.0.0 (`clang-1700.4.4.1`), SDK26.1, libc++ | 4/4 passed in each | 30 passed | [Debug](f01-source-manifests/approved-sdk/mac-arm64-debug.json), [Release](f01-source-manifests/approved-sdk/mac-arm64-release.json), [bootstrap](f01-source-manifests/approved-sdk/mac-arm64-bootstrap.json) |

Mac selects the installed approved Xcode rather than the image default. Windows provisions the side-by-side `Microsoft.VisualStudio.Component.VC.14.50.18.0.x86.x64` component when needed and pins both vcpkg triplets to the selected developer environment; dependencies no longer silently select the newer14.51 compiler. Linux downloads the official archive pinned in `config/toolchains.json`, verifies SHA-256 `6eef42679b744cdcb50276f2d7cff0a51f7ddd632960e06bfbc3f6b9508ef615`, and uses the v26 triplet for target libraries. Linux host-only tools retain their native host triplet; they are not linked into the core.

The [preceding run](https://github.com/vhark/canopy-foundry/actions/runs/37712962046) exposed Linux's static-runtime link-order failure and a Windows-sensitive test fixture. libc++/libc++abi now occupy CMake's trailing standard-library slot rather than preceding their consumers in linker flags. The stale-success-manifest regression now exercises a real failed preflight instead of relying on a malformed mocked SDK configuration. Its focused local run passed, followed by the successful native matrix above.

All six core manifests retain `scope: standalone-core` and `unreal_qualified: false`. The nine downloaded JSON files are preserved byte-for-byte; `.gitattributes` disables line-ending conversion for this evidence directory. Each Debug/Release manifest's bootstrap hash was checked against its downloaded bootstrap file before preservation. Compiler, standard-library and output-library hashes are observed artifacts, not invented qualification values.

## Authorized UE5.8.1 source and native consumer

Epic GitHub access is available after the owner's account link and acceptance of the pending GitHub organization invitation. The authorized `5.8.1-release` tag resolves to the immutable commit and archive SHA-256 recorded in `config/toolchains.json`; the extracted `Engine/Build/Build.version` reports 5.8.1. The source archive, extracted engine and machine-local provenance remain ignored/private. No separate license terms were accepted and no engine metadata was altered.

`bootstrap_engine.py` digest-checks the archive before safe staged extraction, refuses existing destinations, and installs only the host's official .NET SDK, native UBA executor libraries and UBT managed build inputs through GitDependencies. It publishes provenance only after the real bundled .NET commands succeed and records SHA-256 for the selected managed/native inputs. Failed provenance publication rolls the source back into owned staging instead of leaving an unusable destination.

`qualify_engine_core.py` builds official UBT and invokes a real C++20 Program target in Development and Shipping. The Program calls the public conversion API from the same Release archive whose inputs/compiler/library hashes are checked, and reports its measured compiler/runtime ABI. Linux qualification rejects a core sysroot different from UBT's multiarch target; Mac core and dependencies use the UE5.8 macOS14 deployment floor. A successful manifest has `scope: unreal-core-consumer`, separate from unchanged `standalone-core` evidence.

The current local build-orchestration suite passes **52 tests**, including rejected source hash/version/path traversal, preservation of existing installations, publication rollback/retry, invalid consumer measurements, escaped/mismatched receipts, mismatched Mac deployment targets and divergent Linux sysroots. The standalone C++ consumer printed area `5574.182400000001`, C++20, 64-bit pointers, no RTTI/exceptions and the local AppleClang/libc++ identity. This is not an approved-host UBT execution.

The real local bootstrap completed from the pinned authorized archive: GitDependencies installed 5,048 selected files, and the bundled SDK reported .NET10.0.203/runtime10.0.7 before successful provenance publication. The final local Release rebuild and all four CTest cases passed with `arm64-osx-ue58`. Invoking the real qualifier on this installation exited1 before UBT with `Xcode mismatch: required 26.1.1, found Xcode 27.0`; no engine-consumer success manifest was written.

The [first owner-authorized engine matrix](https://github.com/vhark/canopy-foundry/actions/runs/37732236532), at `07ccf182d698480bad4b16a71fea15edb9ba8617`, successfully bootstrapped the pinned engine on all three hosts and passed all native Debug/Release core cases. Python checks passed: 52 on Mac/Linux; 48 plus four POSIX-only skips on Windows. Native qualification correctly remained unsuccessful: Windows fixture identities used host backslashes against a portable allowlist; Mac/Linux built UBT successfully but did not publish it into the engine tool directory. Fixture identities now use POSIX separators, and UBT receives the separate `dotnet publish --no-build` step used by Epic's own `BuildUBT` scripts. No Program executable ran in this first attempt.

The [second engine matrix](https://github.com/vhark/canopy-foundry/actions/runs/37734408041), at `44e570ce90177ad738e5e2e8d7e319c5b1f61db3`, built and published UBT on all three native hosts. All three then stopped with `UBA is not available`. In released UE5.8, `ExecutorFactory.GetUBAExecutor` requires the native UBA runtime even when `-NoUBA` disables detouring; that switch does not restore the older executor. The bootstrap now selects the official host UbaHost/UbaDetours libraries (and Linux static stub) identified by Epic's `Library.props` and GitDependencies manifest. This supplies the actual required runtime; no UBT source or availability checks are bypassed.

## Remaining hard gate

The user retained the approved Unreal5.8.1 / Xcode26.1.1 / VS2026-MSVC14.50-SDK10.0.26100 / Linux v26 Clang20.1.8 fixed-sysroot baseline. Standalone builds with those compiler/SDK selections passed on all three native hosted runners. Authorized UE5.8.1 source access is cleared; actual native UBT consumer execution on all three approved hosts remains pending. Local Xcode27 is not a substitute. The owner-only, opt-in engine workflow consumes a short-lived private archive URL and uploads only explicitly selected provenance/qualification manifests, never engine source or binaries.

F01 remains open. F02 and the later implementation packages retain their hard completion prerequisites. The first-/third-person and overhead camera requirements are documented in architecture A13 and the F03/X04/native acceptance tasks; no camera runtime has been implemented or exercised.
