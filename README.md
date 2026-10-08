# Grownetics: Canopy Foundry

Build your growing operation, master its environment, and scale from your first crop to an industrial growing business.

**Status: implementation started; F01 is in progress. No executable game is included yet.** The standalone C++ core passes native source checks on Linux, Windows and Apple Silicon macOS; approved Unreal/toolchain and runtime qualification are not complete. This is a new game repository, not a rename or fork of the existing Grownetics Sim training study. The title is a working commercial name, pending trademark/domain clearance.

## Product decisions

- Switchable third-person and first-person work/driving, overhead spatial construction, operations management and business progression in one coherent facility. Third-person is the default; view choice never changes simulation authority.
- Unreal Engine **5.8.1** presentation baseline; engine-independent **C++20** simulation; offline **Python 3.12 / GrowBIM / OpenCEA / Blender / Bonsai / IfcOpenShell** authoring and asset tooling.
- Windows 11 x64, Apple Silicon macOS 15+ and Linux x64 are explicit release qualification targets. No browser, mobile or console launch promise.
- High-resolution materials and 4K output tiers with measured performance gates; neither native 4K/60 nor experimental rendering features are baseline requirements.
- Career, Sandbox and Training share the same domain simulation. Offline single-player launches first; four-player cooperative play has a separate planned delivery gate.
- Start with a small indoor lettuce operation and a full crop-to-sale cycle. Add a weather-driven tomato greenhouse, expansion/automation, then flagship facilities and the cannabis crop/processing pack.
- Real equipment geometry, connectors, controls and operating data have separate provenance and permission requirements. Generic original equipment keeps the game playable without sponsors. Sponsors cannot buy better simulated physics.

## Start here

1. [Product specification](docs/product-specification.md) — experience, scope, modes, scenarios and acceptance.
2. [Architecture and decisions](docs/architecture.md) — authority, runtime, rendering, simulation, persistence and platforms.
3. [BIM and asset pipeline](docs/bim-and-asset-pipeline.md) — existing contracts, new work, realistic equipment and rights.
4. [Master implementation plan](docs/superpowers/plans/2026-10-07-canopy-foundry.md) — ordered work packages and release gates.
5. [Performance and qualification](docs/performance-and-qualification.md) — proposed budgets and how to prove them.
6. [Vendor program](docs/vendor-program.md) — acquisition, sponsorship, approvals and content withdrawal.
7. [Research and source register](docs/research/source-register.md) — observed support versus proposed capability.
8. [Skill evaluation](docs/research/skill-evaluation.md) — complete installed-skill triage, subagent responsibilities and pinned external Unreal guidance.

The master plan links the detailed subsystem plans. Every requirement has an implementation owner/task and an observable acceptance gate. Game and qualification commands remain future acceptance contracts until their work packages are implemented; a standalone core build is not evidence of a playable game.

## Native source build

Use uv **0.12.3** and Python **3.12.13**. The frozen environment includes the pinned CMake/Ninja tools and immutable [OpenCEA/GrowBIM wheels](dependencies/upstream/README.md); no sibling editable checkout is required.

```sh
uv sync --frozen --all-packages --all-extras
uv run --frozen python scripts/bootstrap_native.py
uv run --frozen python scripts/build_core.py --config Debug
uv run --frozen ctest --test-dir .build/core --output-on-failure
uv run --frozen python -m pytest tests/build -q
```

`--config Release` selects the release build. The CMake preset `native-asan-ubsan` uses a separate `.build/core-sanitized` directory. Build outputs, dependency caches and machine-specific manifests remain under ignored `.build/` and `.work/`.

Local macOS arm64 verification passed: four native conversion cases in each of Debug, Release and ASan/UBSan builds. The current build-orchestration suite passes 52 checks. The separately linked API smoke produced `60000 square feet = 5574.1824 square metres`. Core and dependency builds use the UE 5.8 macOS14 deployment floor through `arm64-osx-ue58`; the emitted manifest is checked against the actual archive architecture, deployment target and selected SDK. These local results do not qualify an Unreal ABI or another platform.

The approved-SDK hosted source matrix also passed Debug and Release on all three native platforms: four native cases per configuration; at that checkpoint, 30 Python checks on Linux/Mac and 26 passed plus four POSIX-only skips on Windows. The [F01 execution report](docs/research/f01-build-evidence.md) records source checkpoints, compiler/SDK and artifact hashes, failure/recovery evidence, preserved manifests and the remaining native engine-consumer gate.

The approved game baseline remains Unreal **5.8.1**, Mac Xcode **26.1.1**, Windows VS2026/MSVC14.50/SDK10.0.26100 and Linux v26 Clang20.1.8 with its fixed sysroot. Those compiler/SDK selections are exercised in source CI; they are not Unreal qualification. The pre-existing local editor is Unreal **5.8.3**; local Xcode **27.0** and SDK **27.0** are not the approved profile. Game packaging must reject an incompatible host or missing game project; do not edit engine metadata or relabel a standalone manifest to pass the gate.

Authorized Epic GitHub access is now available. `config/toolchains.json` pins the exact UE5.8.1 source commit, archive root and SHA-256; the archive and extracted source remain private and ignored. No engine metadata is edited and no standalone manifest is relabeled as engine-qualified.

On a native host with the approved SDK selected, provision the build tools from an authorized local archive and run the real UBT consumer:

```sh
uv run --frozen python scripts/bootstrap_engine.py --engine-root /absolute/new/ue581 --archive /absolute/authorized-source.tar.gz
uv run --frozen python scripts/build_core.py --config Release
uv run --frozen python scripts/qualify_engine_core.py --engine-root /absolute/new/ue581
```

The bootstrap installs only official host .NET, native UnrealBuildAccelerator (required by UE5.8 even with `-NoUBA`) and UBT managed dependencies, not an editor or game installation. The qualifier builds and publishes official UnrealBuildTool, uses it to link Development and Shipping C++20 Program executables against the actual Release core archive, and runs both programs. Each must report the expected public API result and matching compiler, CRT, pointer width, RTTI and exception policy. Receipt/executable hashes and runtime output are written separately under `.build/engine-qualification/`; this is core-consumer evidence, not UAT/game/GPU qualification.

Ordinary source CI needs no Epic credential. Engine qualification is opt-in: an owner-authored push with `[qualify-engine]` in the commit message and the short-lived `CANOPY_UE_SOURCE_ARCHIVE_URL` repository secret. The secret is an authorized GitHub codeload URL for the exact pinned commit, available only to the bootstrap step; remove it after the run. No long-lived Epic/GitHub token, engine archive, source tree or engine binary is uploaded as a CI artifact.

## Repository policy

Private development repository. Do not commit vendor source CAD, confidential facility models, player data, engine binaries or credentials. Approved Unreal content uses Git LFS; restricted source masters and build artifacts use access-controlled object storage. See [asset governance](docs/bim-and-asset-pipeline.md) and [licensing boundaries](docs/licensing.md).

Existing OpenCEA/GrowBIM contracts remain upstream; their schemas are not copied into a competing authority here. The old Grownetics Sim repository remains intact as evidence of its narrow authoring/replay qualification, not proof of this game's performance or crop accuracy.
