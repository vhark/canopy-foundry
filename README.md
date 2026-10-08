# Grownetics: Canopy Foundry

Build your growing operation, master its environment, and scale from your first crop to an industrial growing business.

**Status: the UE5.8.3/Xcode27.0 Development game builds and runs locally on Apple Silicon.** F01, F02, F04 and upstream B01 are complete; B02's native cooked-coordinate gate now passes on Mac. F03's full input/platform acceptance remains open. F01's original UE5.8.1 three-platform boundary-consumer results remain historical evidence, not qualification of the new engine patch on Windows/Linux. No packaged game is distributed with this public source repository. This is a new repository, not a rename or fork of Grownetics Sim. The title remains subject to trademark/domain clearance.

## License

Original project code, documentation and project-owned content are available under [PolyForm Noncommercial 1.0.0](LICENSE), unless a file has its own license. Noncommercial use, modification and redistribution are permitted under those terms; commercial rights require a separate agreement with the relevant copyright holders. This is **source-available**, not OSI open source, because commercial use is restricted. Unreal Engine, third-party dependencies, assets and trademarks retain their own terms; the project license does not relicense them.

Active implementation and its qualification evidence are on [`feat/canopy-foundry-implementation`](https://github.com/vhark/canopy-foundry/tree/feat/canopy-foundry-implementation). Public source availability is not a claim that a packaged game or commercial release is ready.

## Product decisions

- Switchable third-person and first-person work/driving, overhead spatial construction, operations management and business progression in one coherent facility. Third-person is the default; view choice never changes simulation authority.
- Unreal Engine **5.8.3** presentation baseline; engine-independent **C++20** simulation; offline **Python 3.12 / GrowBIM / OpenCEA / Blender / Bonsai / IfcOpenShell** authoring and asset tooling.
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

The master plan links the detailed subsystem plans. Every requirement has an implementation owner/task and an observable acceptance gate. Commands in unchecked work packages remain future acceptance contracts; a standalone core build is not evidence of a playable game. The local Mac package evidence below is narrower than the three-platform release gates.

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

The current game baseline is Unreal **5.8.3**, Mac **Xcode 27.0 / SDK 27.0**, Windows VS2026/MSVC14.50/SDK10.0.26100 and Linux v26 Clang20.1.8 with its fixed sysroot. Xcode27.0 is the latest stable release checked against [Apple's release listing](https://developer.apple.com/xcode/system-requirements/); 27.1 RC and 27.2 beta are not this reproducible profile. F01's original UE5.8.1/Xcode26.1.1 manifests remain historical evidence, not qualification of the new engine patch. Do not edit engine metadata or relabel a standalone manifest to pass a game gate.

`config/toolchains.json` pins the authorized UE5.8.3 source commit, archive root and SHA-256, plus the exact Mac Epic Launcher release and Editor/UBT/Build.version hashes. Archives, extracted source and installed-engine inventory evidence remain private and ignored. Source-bootstrap provenance and installed-Editor provenance are distinct; neither is accepted as the other.

On a native host with the approved SDK selected, provision the build tools from an authorized local archive and run the real UBT consumer:

```sh
uv run --frozen python scripts/bootstrap_engine.py --engine-root /absolute/new/ue583 --archive /absolute/authorized-source.tar.gz
uv run --frozen python scripts/build_core.py --config Release
uv run --frozen python scripts/qualify_engine_core.py --engine-root /absolute/new/ue583
```

The minimal source bootstrap installs official host .NET, native UnrealBuildAccelerator (required by UE5.8 even with `-NoUBA`), UBT managed dependencies and native link/resource tools, not an editor or game installation. The qualifier builds and publishes official UnrealBuildTool, uses it to link Development and Shipping C++20 Program executables against the actual Release core archive, and runs both programs. Each must report the expected public API result and matching compiler, CRT, pointer width, RTTI and exception policy. UE5.8 forces exceptions on for Mac; the Mac core matches that policy while Windows/Linux keep exceptions disabled. Receipt/executable hashes and runtime output are written separately under `.build/engine-qualification/`; this is core-consumer evidence, not UAT/game/GPU qualification.

Public source CI needs no Epic credential and runs only the standalone core checks. Engine qualification must run locally or in a private build repository: a public request containing `[qualify-engine]` fails before provisioning, and the full native-game workflow refuses public execution. Owner-only execution does not make GitHub logs private. In a private build repository, qualification is opt-in via an owner-authored `[qualify-engine]` push and the short-lived `CANOPY_UE_SOURCE_ARCHIVE_URL` secret, available only to bootstrap and removed after the run. Never upload an engine archive, source tree or binary as a public artifact.

### Full Editor, room and cooked-coordinate gate

For a local Apple Silicon build, select Xcode27.0 and install its Metal toolchain component. The approved Epic Launcher Editor is **UE5.8.3, CL58210709**. Register that existing installation without modifying or rebuilding Epic's engine binaries:

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
uv run --frozen python scripts/bootstrap_engine.py --installed-editor --engine-root "/Users/Shared/Epic Games/UE_5.8"
uv run --frozen python scripts/build_core.py --config Release
uv run --frozen python scripts/build_game.py --platform mac-arm64 --configuration Development --engine-root "/Users/Shared/Epic Games/UE_5.8"
```

Registration checks the exact launcher registry/receipt, release and pinned binaries, then records an inventory including shipped precompiled rules and UHT headers under `Intermediate`. The narrow exclusions in `bootstrap_engine.py` cover runtime caches, generated project-selection/indexing metadata, the project's UAT staging INI, copied plist templates and shared-PCH wrapper/response files—not compiled PCHs or rules assemblies. Build-only Xcode generation disables IntelliSense output. Packaging checks the admitted inventory again before and after execution. The project uses UE5.8 build settings/include order and an explicit Interchange recipe, not ambient Editor import preferences.

Game provenance excludes the cooker-written `EditorOpenOrder.log`/`CookerOpenOrder.log` and Xcode version counter, while retaining authored packaging inputs such as `GameOpenOrder.log`. Generated outputs must not invalidate an otherwise unchanged source build.

UE5.8.3 has [Epic issue UE-396802](https://forums.unrealengine.com/t/ue5-8-unable-to-build-game-on-macos-27/2746676): its build executor can leave descriptor0 closed, so Xcode27 cannot launch a scheme pre-action during UBT-driven app finalization. Disabling UBA detours does not fix it. The Mac pipeline instead generates the normal modern Xcode workspace and lets `xcodebuild` drive native game compilation and app finalization; UAT then performs cooking, staging, packaging and archiving. Source-built Mac Editors use their Xcode scheme too. No engine patch, SDK allowlist change, failed-command suppression or omitted finalization is involved. The complete Xcode game scheme compiled, linked and locally signed successfully on the selected toolchain; full source-Editor rebuilding is not yet locally qualified.

Alternatively, provision a full source Editor into a new engine directory. Minimal-tool bootstrap evidence is not accepted as a full Editor installation. Source builds require the approved native SDK and at least **100 GiB free**:

```sh
uv run --frozen python scripts/bootstrap_engine.py --full-editor --engine-root /absolute/new/ue583-editor --archive /absolute/authorized-source.tar.gz
uv run --frozen python scripts/build_core.py --config Release
uv run --frozen python scripts/build_game.py --platform mac-arm64 --configuration Development --engine-root /absolute/new/ue583-editor
```

Full source bootstrap uses Epic's original GitDependencies selection without a dependency filter, verifies selected payloads against recorded SHA-1 and records a SHA-256 inventory. Packaging builds the project Editor (and engine Editor/ShaderCompileWorker only for source installations), authors the dynamically lit qualification room, imports the generated RH Y-up metre GLB through Interchange, runs Editor automation, cooks/packages and invokes separate F03 and B02 native qualification modes. The cooked B02 mode requires a rendering RHI, not NullRHI, to retain the render buffers it measures. Each executable invocation must exit successfully and publish its own fresh JSON result; Shipping acceptance does not depend on compiled-out log messages. Mac reports are written inside the app's own sandbox container and moved into the archive by the parent build process; sandbox protection stays enabled. The pipeline accepts Epic's UTF-8-BOM automation report without weakening repository manifest parsing.

In a private build repository, `native-game.yml` requires an explicitly selected trusted native runner label, platform and Development/Shipping configuration. Public dispatch is refused rather than exposing raw engine logs. Standard GitHub-hosted runners' [14 GB job storage](https://docs.github.com/en/actions/reference/runners/github-hosted-runners) is insufficient for this full source build; no high-capacity runners are registered for this repository. Public standalone CI selects the `xcode-27` arm64 image and exact Xcode27.0 path; native Windows/Linux game qualification on UE5.8.3 remains a separate gate.

Observed local evidence on UE5.8.3 CL58210709, Xcode27.0 (27A266a), SDK27.0 and AppleClang21:

- **101 Python checks** passed, including installed-payload tamper rejection, generated-input exclusions, sandbox report paths, authentic BOM/camel-case automation parsing and packaged-report rejection. The degenerate IFC placement case emits one expected IfcOpenShell warning.
- **41 Release core cases and all 15 native process-kill/recovery scenarios** passed locally on the selected Xcode27 toolchain.
- The project Editor modules compiled and **six native Editor automation cases** passed. This uses Epic's installed Editor; it is not a full source-Editor rebuild.
- Native game compilation, app finalization, signing and UAT cooking/staging/archiving succeeded. The cooked F03 run reached second60, revision3 and control0.75; the cooked Metal B02 run measured 18 render vertices, 12 collision vertices/four unique corners, a blocking Chaos ray and both asymmetric port frames.
- The actual 1280×800 Metal game window rendered the qualification room and first-/third-person views. This is development geometry, not finished equipment art. Controller coverage, complete keyboard/mouse interaction and rebindings, camera save/relaunch, Shipping and Windows/Linux UE5.8.3 packages remain unqualified; F03 stays open.

The build prints the successful archive's `canopy-game-manifest.json` path. Its directory contains `CanopyFoundry.app`, `automation/index.json`, `f03-native-room.json` and `b02-cooked-fiducial.json`. Launch that specific app with `open /absolute/path/from-the-build/CanopyFoundry.app`; do not glob all prior `uat-*` attempts. Generated room/assets, local packages, reports and raw engine logs are excluded from Git and must not be uploaded by public CI.

Public [source CI, attempt 2](https://github.com/vhark/canopy-foundry/actions/runs/37804761665/attempts/2) passed on Windows, Apple Silicon macOS and Linux: 41 core tests and 15 real process-kill/recovery scenarios per Debug/Release configuration on each host. This closes F04's three-platform persistence gate for the implemented F02 authority and clears the earlier hosted-runner billing blocker. It does not qualify new gameplay state, engine builds or rendering. See the [F04 execution record](docs/superpowers/plans/00-engine-core.md#f04--crash-safe-saves-and-replay-authority).

## Accepted-source semantic compiler

The offline authoring package verifies immutable accepted GrowBIM revisions, then emits a game-only FlatBuffers definition in right-handed Z-up metres. It preserves source hashes and UUID/IFC crosswalks, rooms/storeys/zones, physical equipment envelopes and clearances, complete equipment/connector frames and referenced provenance. It never imports an installed asset ID as a campaign instance ID. An absent upstream validity interval remains explicitly unknown, not an invented approval.

```sh
uv run --frozen --package canopy-author python -m pytest apps/canopy-author/tests -q
uv run --frozen --package canopy-author python apps/canopy-author/tests/fixtures/bim/generate.py --root .work/original-source
uv run --frozen --package canopy-author python -m canopy_author \
  --root .work/original-source --project-id PROJECT_FROM_GENERATOR \
  --revision ACCEPTED_REVISION_FROM_GENERATOR --output .work/facility.cfp
```

The source/coordinate suite passed **30 cases**. Actual CLI output from both an original accepted fixture and a reconciled accepted service archive decoded with native `flatc` 25.12.19. The original fixture retained 216 m² gross floor, eight rooms, six connector frames and separate canopy area; repeated compilation produced identical bytes. B02's remaining asymmetric GLB gate now passes after real Interchange import, cook and Metal runtime mesh/collider/port measurement on the approved Mac engine. Detailed equipment, reproducible multi-platform cooks and the imported-room gameplay tour remain B03/F06 gates.

## Repository policy

Public source repository. Do not commit vendor source CAD, confidential facility models, player data, Unreal Engine source/binaries or credentials. Approved original Unreal content uses Git LFS; restricted source masters and engine/build artifacts remain in access-controlled storage. GitHub Actions logs and artifacts in this repository must be safe for public readers, even for owner-triggered workflows. See [asset governance](docs/bim-and-asset-pipeline.md) and [licensing boundaries](docs/licensing.md).

Existing OpenCEA/GrowBIM contracts remain upstream; their schemas are not copied into a competing authority here. The old Grownetics Sim repository remains intact as evidence of its narrow authoring/replay qualification, not proof of this game's performance or crop accuracy.
