# Grownetics: Canopy Foundry

Build your growing operation, master its environment, and scale from your first crop to an industrial growing business.

**Status: F01, F02 and upstream B01 are complete; F03, F04 and B02 remain under qualification. No packaged game is included.** F01 passed native source checks and real UE5.8.1 Development/Shipping boundary-consumer execution on Linux, Windows and Apple Silicon macOS. F02 provides deterministic bounded command/clock authority. B01's accepted IFC4 facility/MEP profile is pinned as GrowBIM 0.2.0. Persistence and the accepted-source compiler now have local executable evidence; native game rendering/input and cooked coordinate alignment remain unproved. This is a new repository, not a rename or fork of Grownetics Sim. The title remains subject to trademark/domain clearance.

## License

Original project code, documentation and project-owned content are available under [PolyForm Noncommercial 1.0.0](LICENSE), unless a file has its own license. Noncommercial use, modification and redistribution are permitted under those terms; commercial rights require a separate agreement with the relevant copyright holders. This is **source-available**, not OSI open source, because commercial use is restricted. Unreal Engine, third-party dependencies, assets and trademarks retain their own terms; the project license does not relicense them.

Active implementation and its qualification evidence are on [`feat/canopy-foundry-implementation`](https://github.com/vhark/canopy-foundry/tree/feat/canopy-foundry-implementation). Public source availability is not a claim that a packaged game or commercial release is ready.

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
uv run --frozen cmake --build .build/core --target canopy_save_crash
.build/core/canopy_save_crash qualify .work/qualification/save-crash
uv run --frozen python -m pytest tests/build -q
```

`--config Release` selects the release build. The CMake preset `native-asan-ubsan` uses a separate `.build/core-sanitized` directory. Build outputs, dependency caches and machine-specific manifests remain under ignored `.build/` and `.work/`.

Local macOS arm64 verification passed **41 native cases** in each of Debug, Release and ASan/UBSan builds. Each configuration also passed **15 actual process-kill scenarios**: checkpoint, journal and recovered-journal writes at five durability stages, comparing the complete restored authority digest and verifying resumed writing. F02's independent command/replay smoke remains recorded below. Reserved submit/advance paths allocate nothing; owned save capture is separate from those paths. These local Xcode27 results do not qualify another platform or an Unreal ABI. F04's approved-host crash matrix remains a separate gate.

The [qualified native matrix](https://github.com/vhark/canopy-foundry/actions/runs/37746886997) passed F01 Debug/Release checks and built and ran six UBT consumers on approved hosts. The [F01 report](docs/research/f01-build-evidence.md) retains all 18 byte-preserved provenance records. [F02's record](docs/superpowers/plans/00-engine-core.md#f02--typed-state-commands-receipts-and-fixed-clock) includes its later all-three-host Debug/Release source matrix and independent command/replay smoke. [B01's record](docs/superpowers/plans/01-equipment-bim.md#b01-add-upstream-facilitymep-profile-and-accepted-artifact-contract) documents 136 upstream tests, 88 service tests, actual authenticated HTTP acceptance/rejection/export and a frozen downstream wheel importing the same accepted revision and exact artifact bytes.

`canopy::World` admits controls, positive-second `DecisionDeadline` entries, a seed and explicit storage limits before stepping. Accepted IDs never expire; exhausted receipt/event capacity returns a typed error without partial mutation. Retries match the original actor and typed action, ignoring delivery timestamp/revision. Requested controls apply at the next one-second boundary; climate/crop events occur every 5/60 seconds. Same-time decisions stop advancement until each is resolved in stable entity/decision-ID order. `view()`, `events()` and `receipts()` expose borrowed const spans. `capture()` produces an owned state at the domain owner's mutation boundary; `save_checkpoint`, `append_journal`, `load_save`, `load_history` and `create_branch` persist and replay that authority. These contracts do not yet provide crop physics, economic transactions or a gameplay loop.

The approved game baseline remains Unreal **5.8.1**, Mac Xcode **26.1.1**, Windows VS2026/MSVC14.50/SDK10.0.26100 and Linux v26 Clang20.1.8 with its fixed sysroot. Those selections now have native UBT core-consumer proof, not a UAT game or GPU result. The pre-existing local editor is Unreal **5.8.3**; local Xcode **27.0** and SDK **27.0** are not the approved profile. Game packaging must reject an incompatible host or missing game project; do not edit engine metadata or relabel a standalone manifest to pass the gate.

Authorized Epic GitHub access is now available. `config/toolchains.json` pins the exact UE5.8.1 source commit, archive root and SHA-256; the archive and extracted source remain private and ignored. No engine metadata is edited and no standalone manifest is relabeled as engine-qualified.

On a native host with the approved SDK selected, provision the build tools from an authorized local archive and run the real UBT consumer:

```sh
uv run --frozen python scripts/bootstrap_engine.py --engine-root /absolute/new/ue581 --archive /absolute/authorized-source.tar.gz
uv run --frozen python scripts/build_core.py --config Release
uv run --frozen python scripts/qualify_engine_core.py --engine-root /absolute/new/ue581
```

The bootstrap installs only official host .NET, native UnrealBuildAccelerator (required by UE5.8 even with `-NoUBA`), UBT managed dependencies and native link/resource tools, not an editor or game installation. The qualifier builds and publishes official UnrealBuildTool, uses it to link Development and Shipping C++20 Program executables against the actual Release core archive, and runs both programs. Each must report the expected public API result and matching compiler, CRT, pointer width, RTTI and exception policy. UE5.8.1 forces exceptions on for Mac; the Mac core matches that policy while Windows/Linux keep exceptions disabled. Receipt/executable hashes and runtime output are written separately under `.build/engine-qualification/`; this is core-consumer evidence, not UAT/game/GPU qualification.

Ordinary source CI needs no Epic credential. Engine qualification is opt-in: an owner-authored push with `[qualify-engine]` in the commit message and the short-lived `CANOPY_UE_SOURCE_ARCHIVE_URL` repository secret. The secret is an authorized GitHub codeload URL for the exact pinned commit, available only to the bootstrap step; remove it after the run. No long-lived Epic/GitHub token, engine archive, source tree or engine binary is uploaded as a CI artifact.

### Full Editor, room and cooked-coordinate gate

Use a fresh checkout and a new engine directory; minimal-tool bootstrap evidence is not accepted as an Editor installation. The full build requires an approved native SDK and at least **100 GiB free** on its build volume:

```sh
uv run --frozen python scripts/bootstrap_engine.py --full-editor --engine-root /absolute/new/ue581-editor --archive /absolute/authorized-source.tar.gz
uv run --frozen python scripts/build_core.py --config Release
uv run --frozen python scripts/build_game.py --platform mac-arm64 --configuration Development --engine-root /absolute/new/ue581-editor
```

The full bootstrap uses Epic's original GitDependencies selection without a dependency filter, verifies every selected payload against its recorded SHA-1 and records a SHA-256 inventory. Packaging rechecks that inventory, builds the Editor/ShaderCompileWorker, authors the qualification room, imports the generated RH Y-up metre GLB through Interchange, checks Editor automation, cooks/packages and invokes separate F03 and B02 native qualification modes. The cooked B02 mode requires a rendering RHI, not NullRHI, to retain the render buffers it measures. Each executable invocation must exit successfully and publish its own fresh JSON result; Shipping acceptance does not depend on compiled-out log messages. The pipeline accepts Epic's UTF-8-BOM automation report without weakening repository manifest parsing.

The manual `native-game.yml` workflow requires an explicitly selected trusted native runner label, platform and Development/Shipping configuration. Standard GitHub-hosted runners' [14 GB job storage](https://docs.github.com/en/actions/reference/runners/github-hosted-runners) is insufficient for this source build; no high-capacity runners are registered for this repository. Locally, the actual full-Editor command rejects Xcode27; the [approved Xcode26.1.1 download](https://developer.apple.com/services-account/download?path=/Developer_Tools/Xcode_26.1.1/Xcode_26.1.1_Apple_silicon.xip) currently requires Apple sign-in.

Observed source evidence: **86 Python checks passed**, including payload-tamper rejection, authentic BOM/camel-case automation parsing and packaged-report rejection. The original generated GLB's binary positions/winding/normals, four collision corners and two asymmetric port frames passed an independent decoder smoke; its SHA-256 is `8d23b10703d75297d12c5c6bde159eecb95b47525268c5cbc7a69b38614682b2`. These are **not native import/cook, interactive input or visual results**. F03 and B02 remain open.

The latest F04 core rebuild passes 41 local tests and all 15 real process-kill recovery scenarios. Windows/Linux portability fixes are pushed, but [run 37777558997](https://github.com/vhark/canopy-foundry/actions/runs/37777558997) could not start any job: GitHub reports failed account payments or a spending limit. Restore Actions capacity or supply authorized native hosts; F04's three-platform crash-safety gate remains open.

## Accepted-source semantic compiler

The offline authoring package verifies immutable accepted GrowBIM revisions, then emits a game-only FlatBuffers definition in right-handed Z-up metres. It preserves source hashes and UUID/IFC crosswalks, rooms/storeys/zones, physical equipment envelopes and clearances, complete equipment/connector frames and referenced provenance. It never imports an installed asset ID as a campaign instance ID. An absent upstream validity interval remains explicitly unknown, not an invented approval.

```sh
uv run --frozen --package canopy-author python -m pytest apps/canopy-author/tests -q
uv run --frozen --package canopy-author python apps/canopy-author/tests/fixtures/bim/generate.py --root .work/original-source
uv run --frozen --package canopy-author python -m canopy_author \
  --root .work/original-source --project-id PROJECT_FROM_GENERATOR \
  --revision ACCEPTED_REVISION_FROM_GENERATOR --output .work/facility.cfp
```

The source/coordinate suite passed **30 cases**. Actual CLI output from both an original accepted fixture and a reconciled accepted service archive decoded with native `flatc` 25.12.19. The original fixture retained 216 m² gross floor, eight rooms, six connector frames and separate canopy area; repeated compilation produced identical bytes. B02 remains open until the asymmetric GLB fixture's imported/cooked mesh, collider and port alignment is observed in the approved native engine.

## Repository policy

Public source repository. Do not commit vendor source CAD, confidential facility models, player data, Unreal Engine source/binaries or credentials. Approved original Unreal content uses Git LFS; restricted source masters and engine/build artifacts remain in access-controlled storage. GitHub Actions logs and artifacts in this repository must be safe for public readers, even for owner-triggered workflows. See [asset governance](docs/bim-and-asset-pipeline.md) and [licensing boundaries](docs/licensing.md).

Existing OpenCEA/GrowBIM contracts remain upstream; their schemas are not copied into a competing authority here. The old Grownetics Sim repository remains intact as evidence of its narrow authoring/replay qualification, not proof of this game's performance or crop accuracy.
