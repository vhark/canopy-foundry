# Engine and Domain Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Package a native, inspectable real-facility scene backed by an independent command/time/save authority on all three target platforms.

**Architecture:** C++20 domain library plus a thin Unreal runtime bridge; authoring is external. The headless executable, replay tools and game use the same library and model packs.

**Tech Stack:** Unreal 5.8.1, CMake, C++20, CTest/Catch2, FlatBuffers, zstd, Python 3.12 build/qualification orchestration, Git LFS.

---

This document defines future implementation work, not an existing executable. The [master plan](2026-10-07-canopy-foundry.md) owns sequencing. Follow the architecture's authority/units rules. Each task ends with a focused commit, runtime smoke and its recorded evidence; do not substitute unit tests for a packaged-game check.

## F01 — Reproducible native build and dependency lock

**Dependencies:** none; authorized Unreal access and Windows/Mac/Linux build machines are real prerequisites.

**Create:** `CMakeLists.txt`, `CMakePresets.json`, `dependencies/vcpkg.json`, `dependencies/vcpkg-configuration.json`, `config/toolchains.json`, `scripts/build_core.py`, `scripts/build_game.py`, `.github/workflows/core.yml`, `core/include/canopy/units.hpp`, `core/tests/{test_main,units_test}.cpp`.

- [ ] Provision UE **5.8.1** from the authorized release; record engine Build.version/build ID and installation artifact hashes privately. Do not commit Epic engine source/binaries. Select Windows VS2026 18.0/MSVC14.50/SDK10.0.26100, Mac Xcode26.1.1 and Linux v26 clang20.1.8 fixed sysroot; reject Xcode26.4 for this baseline. Capture patch/build identifiers for each compiler/SDK, not only product names.
- [ ] Lock CMake, Ninja, Catch2, FlatBuffers/flatc and zstd to exact versions and source hashes through one committed vcpkg baseline and toolchain record. Resolve licenses before accepting the lock; no `latest`, unpinned Git branch or network package resolution during a reproducible rebuild. The lock is produced from actually installed/verified artifacts, not fabricated checksums in this plan.
- [ ] Create root `pyproject.toml`/`uv.lock` with the creator member `apps/canopy-author/pyproject.toml`; pin Python3.12, uv, authoring/test dependencies and immutable approved GrowBIM/OpenCEA artifacts. Use `uv sync --frozen --all-packages --all-extras` for the single authoring/qualification environment. Update the upstream artifact hash only after B01 lands; do not rely on a sibling editable checkout in reproducible CI. Blender/Bonsai run in their separately pinned application environment. None of these Python dependencies ships as a gameplay prerequisite.
- [ ] Create CMake presets `native-debug`, `native-release` and sanitizer configurations. Match Unreal's target compiler, CRT/standard-library ABI, exception and RTTI policy when producing its static core library; reject incompatible triplets rather than linking a conveniently installed library.
- [ ] Implement `build_core.py --config Debug|Release` and `build_game.py --platform win64|linux-x64|mac-arm64 --configuration Development|Shipping`. They check the lock, propagate nonzero tool exit status, print exact invocations and write a build manifest. Native Mac builds run on Mac; Linux native/cross builds use the qualified toolchain.
- [ ] Configure source-only CI for core tests without engine or vendor credentials. Implement the public area-unit conversion below and test it through that API; do not test only build-file wording or a copied arithmetic expression. F02 extends this units header; B02 uses it for facility accounting.
- [ ] Build a library and test binary on each OS; record compiler and ABI metadata. Commit the lock, build rules and successful reports.

Initial concrete boundary test in `core/tests/units_test.cpp`; the function under test must be implemented in the core, not supplied as a fixture echo:

```cpp
#include <canopy/units.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
TEST_CASE("square-foot conversion uses squared length units") {
    REQUIRE(canopy::square_feet_to_square_metres(60000.0)
            == Catch::Approx(5574.1824));
}
```

B02 adds the distinct facility invariant: three 20,000 ft² storeys total 60,000 ft², independently of the number of rack tiers. It must call the shared conversion API rather than reimplement the constant in fixtures.

**Checks:** `python scripts/build_core.py --config Debug`; `ctest --test-dir .build/core --output-on-failure`. Expected: native test executable runs; toolchain mismatch exits nonzero. `.build/core` is the common native build directory used by all subplans.

## F02 — Typed state, commands, receipts and fixed clock

**Dependencies:** F01.

**Create:** `core/include/canopy/{id,command,world,clock,view}.hpp`, `core/src/{world,clock,receipts}.cpp`, `core/tests/{clock,commands,replay}_test.cpp`, `core/tests/fixtures/clock_world.hpp`. Extend F01's `units.hpp`.

- [ ] Define architecture A04's ID/time/budget types, typed quantities, `World::submit`, `World::advance`, read views and explicit result/error enums. Stable IDs never use raw pointers or Actor names. Define compact test fixture helpers in `clock_world.hpp`, not an unrelated mock engine.
- [ ] Add consumer-facing cases before implementation: a repeated control command returns the original receipt without advancing the authoritative revision twice; changed payload under the same ID conflicts; a stale revision fails; stepping 0→60 as one target versus 60 one-second targets emits the same events; a decision at 60 stops requested advancement to 120 at exactly 60. Purchase/ledger semantics are implemented and tested in S06, not a second temporary economy inside F02.
- [ ] Implement stable timestamp/type/entity tie-breaking, per-entity deterministic random streams, bounded command/receipt buffers and transactional mutation. Reject queue/full-state capacity violations with a typed error; never partially apply a command.
- [ ] Implement 1-second controls and event deadlines with the later climate/crop interval hooks. `AdvanceBudget::max_substeps` yields progress without changing the integrator's time step. No wall time is consulted by the core.
- [ ] Instrument allocations after load and run the steady-state clock case: accepted operations within reserved capacity allocate zero bytes on the stepping path. Scenario growth occurs only through an explicit capacity admission/reservation step.
- [ ] Run the actual core test executable under normal and sanitizer builds; F05 later exposes these operations through the headless CLI. Commit the observed transcript.

Fixture specification, implemented as actual state transitions rather than expected-value echoes:

```json
{"initial_control_fraction":0.25,"requested_control_fraction":0.75,
 "command_id":"00000000-0000-4000-8000-000000000001",
 "deliveries":2,"expected_control_fraction":0.75,"expected_revision":1,
 "decision_second":60,"requested_second":120,"expected_stop_second":60}
```

**Check:** `ctest --test-dir .build/core -R 'Clock|Command|Replay' --output-on-failure`. Expected: receipt/ordering/stop cases pass; original failing-before traces retained in the implementation review.

## F03 — Native first-person game and state bridge

**Dependencies:** F01/F02. Replace the original qualification room with B02/B03 output before F06.

**Create:** `game/CanopyFoundry.uproject`, `game/Source/CanopyFoundry/{CanopyFoundry.Build.cs,CanopyFoundry.Target.cs,CanopyFoundryEditor.Target.cs}`, `game/Plugins/CanopyRuntime/CanopyRuntime.uplugin`, `game/Plugins/CanopyRuntime/Source/CanopySimExternal/CanopySimExternal.Build.cs`, `game/Plugins/CanopyRuntime/Source/CanopyRuntime/{CanopyRuntime.Build.cs,Public/SimulationSubsystem.h,Private/SimulationSubsystem.cpp}`, `game/Source/CanopyFoundry/Player/{FacilityCharacter,InteractionComponent}.{h,cpp}`, `game/Config/{DefaultInput,DefaultEngine}.ini`, `game/Content/Maps/FacilityQualification.umap`.

- [ ] Build an actual packaged first-person scene using original geometry, player capsule, collision, basic controls, interact/inspect and an overhead camera. Original qualification art is not to be presented as a completed realistic equipment pack.
- [ ] Implement one domain worker and bounded command/view queues. Resolve immutable IDs to presentation proxies; apply views only on the game thread. Emit completion/error events for interaction feedback.
- [ ] Implement start/pause/time multiplier/skip controls using target domain time and pending-command reconciliation. Prevent first-person carrying/driving while in management time-lapse; return safely to hands-on mode without changing task state.
- [ ] Bind input through Enhanced Input actions; configure mouse/keyboard/controller in the same action model. Retain focus and accessibility semantics in UI; no document IDs in default inspection.
- [ ] Smoke the packaged program: walk, inspect an instance, issue a valid control change, pause while a command is pending, resume and confirm the authoritative view. Repeat in baseline non-Nanite/non-HWRT rendering.
- [ ] Shut down during queued work and confirm worker lifetime/drain behavior without use-after-free. Commit the real project and approved original assets through LFS.

**Check:** `python scripts/build_game.py --platform win64 --configuration Development` and the corresponding Mac/Linux commands. Expected: three native packages start and input works. Editor play-in-editor alone does not pass.

## F04 — Crash-safe saves and replay authority

**Dependencies:** F02.

**Create:** `contracts/save.fbs`, `contracts/command-log.fbs`, `core/include/canopy/save.hpp`, `core/src/persistence/{snapshot,journal,manifest,atomic_file}.cpp`, `core/tests/{save,save_migration}_test.cpp`, `tests/fixtures/saves/v1/` with small original committed fixtures.

- [ ] Define length-bounded versioned schemas for every authoritative state field and command receipt, pack/model hashes and scheduled events. Include random stream counters and simulated construction state. Enumerate unknown-version and missing-model errors.
- [ ] Add interrupted-write, checksum corruption, truncated journal, duplicate dispatch and old-snapshot-plus-new-journal cases. Corruption must not reset money/crops silently.
- [ ] Implement write-new/flush/atomic-manifest-replace with a retained prior valid save and OS-specific durability behavior. Never snapshot from mutable arrays without a defined epoch/view ownership contract.
- [ ] Implement checkpoint+journal replay and a read-only past view. Branching allocates a new campaign/branch identity and preserves original provenance. Define and enforce available-history retention ranges.
- [ ] Implement explicit forward schema migration into a new save file, leaving the old copy intact; data-only render-pack absence can use an approved generic presentation, mandatory physics-pack absence cannot.
- [ ] Kill the actual save-writer process at each durability stage on Windows, Mac and Linux; restart the real loader and compare inventory, cash and scheduled work. Commit deterministic corruption/crash fixtures, not machine-specific temporary files.

Case table to encode in the tests:

```text
before manifest replace -> old committed state loads
new manifest durable    -> new committed state loads
truncated new snapshot  -> prior state + explicit recovery warning
unknown schema version  -> unsupported_version; no writes
missing physics pack    -> missing_model; no substitute physics
missing optional logo   -> neutral visual, identical domain hash
```

**Check:** `ctest --test-dir .build/core -R 'Save|Migration' --output-on-failure`; native fault-injection save smoke on each target. Expected: no duplicate transaction and no unrecoverable loss of the previous committed save.

## F05 — Headless runner, diagnostics and qualification protocol

**Dependencies:** F02/F04.

**Create:** `apps/canopy-headless/{CMakeLists.txt,main.cpp}`, `core/src/diagnostics/{events,causal_links}.cpp`, `contracts/reports/report.schema.json`, `scripts/qualify.py`, `tests/qualification/{__init__,core}.py`.

- [ ] Implement `canopy-headless run --pack PATH --scenario ID --until SECOND --commands PATH --report PATH`, `replay --save PATH --report PATH` and `benchmark --workload PATH --report PATH`. Invalid/unavailable input exits nonzero; no default fake facility on load failure.
- [ ] Emit structured status, actual reached time, stop reason, state hash, resource residuals, discrete outcomes and rejected commands. Human-readable explanations reference the causal event chain and source/model assumptions rather than raw object addresses.
- [ ] Implement `python scripts/qualify.py --case CASE_ID --platform {win64,linux-x64,mac-arm64} --output DIRECTORY`. Platform defaults to native and output to `.work/qualification`. Cases register in `tests/qualification/<domain>.py`; an unknown/skipped mandatory case fails. It runs the real program and checks artifacts, not mocked stdout.
- [ ] Validate reports with required `case`, `status`, `commit`, `engine`, `model`, `seed`, `platform`, `settings`, `measurements`, `evidence`. Store unavailable metrics explicitly as not measured, never zero.
- [ ] Register `core-clock`, `core-save-recovery` and `core-replay`; run the same command stream headless and through the Unreal bridge. Compare domain results, not screenshots or message wording.
- [ ] Commit CLI usage and a small original scenario/command stream that reproduces the result.

**Checks:** `python scripts/qualify.py --case core-replay`; `python scripts/qualify.py --case unknown-case` (expected nonzero). Correct reports have artifact paths and evidence, not a hardcoded pass flag.

## F06 — Three-platform imported-room proof

**Dependencies:** F03/F05 and B02/B03.

**Create:** `tests/qualification/native_room.py`, `benchmarks/routes/room-inspection.json`, `game/Source/CanopyFoundry/Tests/ImportedRoomScenario.cpp`.

- [ ] Register `native-imported-room`: load the compiled accepted IFC room, inspect the mapped rack, verify metre-to-centimetre scale/handedness, walk through the real door, switch floorplan/first-person and select the same semantic entity.
- [ ] Save/reload the fixture state and camera/selection, verifying the selected semantic entity and accepted source digest remain unchanged. Full construction/movement transactions are qualified later in B06/G02; do not introduce a second unvalidated geometry-edit implementation to pass this early platform proof.
- [ ] Run the room on Windows/Linux and Apple Silicon Mac with the conventional LOD/raster baseline, then enable supported higher-tier features. Capture camera-route evidence at 1080p and 4K output, including readable UI and collision.
- [ ] Verify the packaged runtime works with Python/Blender/Bonsai and the authoring service absent. Verify native Windows does not import or shell out to the POSIX store.
- [ ] Record all three results; if any target fails, fix that target or explicitly change the approved release scope before proceeding. No assertion that engine support alone proves product support.
- [ ] Commit the qualification recipe and permitted evidence references. This milestone proves integration, not fun, crop accuracy or flagship performance.

**Check:** `python scripts/qualify.py --case native-imported-room --platform mac-arm64` and both other target values on their native machines. Expected: mapped geometry/interaction/save tests and actual visual captures pass.
