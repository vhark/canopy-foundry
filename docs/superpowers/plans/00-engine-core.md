# Engine and Domain Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Package a native, inspectable real-facility scene backed by an independent command/time/save authority on all three target platforms.

**Architecture:** C++20 domain library plus a thin Unreal runtime bridge; authoring is external. The headless executable, replay tools and game use the same library and model packs.

**Tech Stack:** Unreal 5.8.1, CMake, C++20, CTest/Catch2, FlatBuffers, zstd, Python 3.12 build/qualification orchestration, Git LFS.

---

F01 and F02 are complete; later tasks still define future work, not an existing game executable. The [master plan](2026-10-07-canopy-foundry.md) owns sequencing. Follow the architecture's authority/units rules. Each task ends with a focused commit, runtime smoke and its recorded evidence; do not substitute unit tests for a packaged-game check.

## F01 — Reproducible native build and dependency lock

**Dependencies:** none; authorized Unreal access and Windows/Mac/Linux build machines are real prerequisites.

**Create:** `CMakeLists.txt`, `CMakePresets.json`, `dependencies/{vcpkg.json,vcpkg-configuration.json,native-lock.json}`, `config/toolchains.json`, `scripts/{bootstrap_native,build_common,build_core,build_game}.py`, `.github/workflows/core.yml`, `core/include/canopy/units.hpp`, `core/tests/units_test.cpp`. Use Catch2's supplied `Catch2WithMain` rather than a duplicate test entrypoint.

- [x] Provision UE **5.8.1** from the authorized release; record engine Build.version/build ID and installation artifact hashes privately. Do not commit Epic engine source/binaries. Select Windows VS2026 18.0/MSVC14.50/SDK10.0.26100, Mac Xcode26.1.1 and Linux v26 clang20.1.8 fixed sysroot; reject Xcode26.4 for this baseline. Capture patch/build identifiers for each compiler/SDK, not only product names.
- [x] Lock CMake, Ninja, Catch2, FlatBuffers/flatc and zstd to exact versions and source hashes through one committed vcpkg baseline and toolchain record. Resolve licenses before accepting the lock; no `latest`, unpinned Git branch or network package resolution during a reproducible rebuild. The lock is produced from actually installed/verified artifacts, not fabricated checksums in this plan.
- [x] Create root `pyproject.toml`/`uv.lock` with the creator member `apps/canopy-author/pyproject.toml`; pin Python3.12, uv, authoring/test dependencies and immutable approved GrowBIM/OpenCEA artifacts. Use `uv sync --frozen --all-packages --all-extras` for the single authoring/qualification environment. Update the upstream artifact hash only after B01 lands; do not rely on a sibling editable checkout in reproducible CI. Blender/Bonsai run in their separately pinned application environment. None of these Python dependencies ships as a gameplay prerequisite.
- [x] Create CMake presets `native-debug`, `native-release` and sanitizer configurations. Match Unreal's target compiler, CRT/standard-library ABI, exception and RTTI policy when producing its static core library; reject incompatible triplets rather than linking a conveniently installed library.
- [x] Implement `build_core.py --config Debug|Release` and `build_game.py --platform win64|linux-x64|mac-arm64 --configuration Development|Shipping`. They check the lock, propagate nonzero tool exit status, print exact invocations and write a build manifest. Native Mac builds run on Mac; Linux native/cross builds use the qualified toolchain.
- [x] Configure source-only CI for core tests without engine or vendor credentials. Implement the public area-unit conversion below and test it through that API; do not test only build-file wording or a copied arithmetic expression. F02 extends this units header; B02 uses it for facility accounting.
- [x] Build a library and test binary on each OS; record compiler and ABI metadata. Commit the lock, build rules and successful reports.

Completion evidence: the [native qualification matrix](https://github.com/vhark/canopy-foundry/actions/runs/37746886997) at `ef149389528508fb74521aa65fa3ba31dd5fcca4` passed source builds and actual UE5.8.1 UBT Development/Shipping core consumers on all three approved hosts. Each executable called the same Release core API and passed strict compiler/runtime ABI validation. The [F01 execution report](../../research/f01-build-evidence.md) preserves 18 linked provenance/consumer records and the failure/recovery history. Local Debug, Release and ASan/UBSan each passed four native cases; the build-orchestration suite passed 52 checks. `build_game.py` remains fail-closed without F03's game project; no UAT game, editor or GPU/runtime result is claimed. F02 and B01 may proceed.

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

**Checks:** `uv run --frozen python scripts/build_core.py --config Debug`; `uv run --frozen ctest --test-dir .build/core --output-on-failure`. Expected: native test executable runs; toolchain mismatch exits nonzero. `.build/core` is the common native build directory used by all subplans.

## F02 — Typed state, commands, receipts and fixed clock

**Dependencies:** F01.

**Create:** `core/include/canopy/{id,command,world,clock,view}.hpp`, `core/src/{world,clock,receipts}.cpp`, `core/tests/{clock,commands,replay}_test.cpp`, `core/tests/fixtures/clock_world.hpp`. Extend F01's `units.hpp`.

- [x] Define architecture A04's ID/time/budget types, typed quantities, `World::submit`, `World::advance`, read views and explicit result/error enums. Stable IDs never use raw pointers or Actor names. Define compact test fixture helpers in `clock_world.hpp`, not an unrelated mock engine.
- [x] Add consumer-facing cases before implementation: a repeated control command returns the original receipt without advancing the authoritative revision twice; changed payload under the same ID conflicts; a stale revision fails; stepping 0→60 as one target versus 60 one-second targets emits the same events; a decision at 60 stops requested advancement to 120 at exactly 60. Purchase/ledger semantics are implemented and tested in S06, not a second temporary economy inside F02.
- [x] Implement stable timestamp/type/entity tie-breaking, per-entity deterministic random streams, bounded command/receipt buffers and transactional mutation. Reject queue/full-state capacity violations with a typed error; never partially apply a command.
- [x] Implement 1-second controls and event deadlines with the later climate/crop interval hooks. `AdvanceBudget::max_substeps` yields progress without changing the integrator's time step. No wall time is consulted by the core.
- [x] Instrument allocations after load and run the steady-state clock case: accepted operations within reserved capacity allocate zero bytes on the stepping path. Scenario growth occurs only through an explicit capacity admission/reservation step.
- [x] Run the actual core test executable under normal and sanitizer builds; F05 later exposes these operations through the headless CLI. Commit the observed transcript.

Fixture specification, implemented as actual state transitions rather than expected-value echoes:

```json
{"initial_control_fraction":0.25,"requested_control_fraction":0.75,
 "command_id":"00000000-0000-4000-8000-000000000001",
 "deliveries":2,"expected_control_fraction":0.75,"expected_revision":1,
 "decision_second":60,"requested_second":120,"expected_stop_second":60}
```

**Check:** `uv run --frozen ctest --test-dir .build/core --output-on-failure`. The native executable contains 23 cases, including the clock, command, replay and allocation scenarios; no case-name filter may silently select zero tests.

**Implemented boundary semantics:** `World::load(controls, deadlines, seed)` validates positive authored deadlines and storage capacity before admission. No deadlines means no decision stops. Decisions sort by second, entity ID and decision ID; all same-time required events are emitted once, with resolution required in that order. Requested controls apply at the next second; 5-second climate and 60-second crop boundary events precede that second's required decisions. These events are integration hooks, not fictitious physics updates. Accepted actor/action identities use a preallocated receipt index and never evict; retry envelope time/revision may differ. Seed/entity/stream/counter mixing is ordered and stateless, with no unsolicited per-tick random draws. Views borrow const storage until mutation. Explicit moves transfer all state and reset the source to an unloaded world.

**Observed failures and corrections:** the initial Debug build rejected a shadowed `action` declaration in the resolution branch. An independent RNG probe exposed identical samples for swapped stream/counter values `(2,7)` and `(7,2)`; domain-separated ordered mixing removes that structural collision. Review found default moves copied `loaded_` after moving its storage; a new regression failed in both move sections (`DecisionRequired` instead of `InvalidModel`) before the explicit transfer/reset implementation. Its construction/assignment, receipt continuity, deadline and source-reload checks now pass 47 assertions.

**Executed verification:** native Apple Silicon macOS, AppleClang21/Xcode27/SDK27, macOS14 deployment floor: Debug 23/23, Release 23/23 and ASan/UBSan 23/23; build-orchestration pytest 52/52. These source results are not new approved-SDK game or GPU qualification. The separately compiled and linked public-API smoke used the specified command UUID, two deliveries, one authored decision at 60 and target 120; it also compared the event stream against sixty single-second advances. Its actual output was:

```text
receipt_revision=1 duplicate_deliveries=2 control=0.75 stop_second=60 resumed_second=120 climate=24 crop=2 partition_events=equal rng_domains=distinct
```

**Approved-SDK standalone source matrix:** [run 37757332589](https://github.com/vhark/canopy-foundry/actions/runs/37757332589), commit `fdd9b34`, passed Debug and Release on native Windows, Linux and Apple Silicon macOS: all 23 CTests on each host/configuration. Exact bootstrap/core manifests and Release test logs are retained under `docs/research/f02-source-manifests/`; their original bytes preserve the linked bootstrap digests. This is standalone source qualification, not a replacement for F01's UE ABI checks or F03's packaged-game, input and GPU gates.


## F03 — Native first-/third-person game and state bridge

**Dependencies:** F01/F02. Replace the original qualification room with B02/B03 output before F06.

**Create:** `game/CanopyFoundry.uproject`, `game/Source/CanopyFoundry/{CanopyFoundry.Build.cs,CanopyFoundry.Target.cs,CanopyFoundryEditor.Target.cs}`, `game/Plugins/CanopyRuntime/CanopyRuntime.uplugin`, `game/Plugins/CanopyRuntime/Source/CanopySimExternal/CanopySimExternal.Build.cs`, `game/Plugins/CanopyRuntime/Source/CanopyRuntime/{CanopyRuntime.Build.cs,Public/SimulationSubsystem.h,Private/SimulationSubsystem.cpp}`, `game/Source/CanopyFoundry/Player/{FacilityCharacter,InteractionComponent}.{h,cpp}`, `game/Config/{DefaultInput,DefaultEngine}.ini`, `game/Content/Maps/FacilityQualification.umap`.

- [ ] Build a packaged scene with one visible original/licensed character, third-person default, first-person eye view and overhead camera. Create `game/Source/CanopyFoundry/Player/FacilityCameraComponent.{h,cpp}` for architecture A13: collision-tested orbit/chase, smooth bounded transitions, local head/body visibility, per-context persisted preferences and rebindable perspective/management actions. Do not clone or respawn the actor to change cameras. Qualification art is not a completed realistic equipment pack.
- [ ] Implement one domain worker and bounded command/view queues. Resolve immutable IDs to presentation proxies; apply views only on the game thread. Emit completion/error events for interaction feedback.
- [ ] Implement start/pause/time multiplier/skip using target domain time and pending-command reconciliation. Prevent physical carrying/driving in either hands-on view during management time-lapse; return to the prior first-/third-person view safely without changing work state. Camera switching itself never changes time speed.
- [ ] Bind input through Enhanced Input actions; configure mouse/keyboard/controller in the same action model. Retain focus and accessibility semantics in UI; no document IDs in default inspection.
- [ ] Smoke the actual package using both controller and mouse/keyboard: first-/third-person/overhead transitions, narrow door and bench camera collision, avatar-to-target reach despite camera corner visibility, valid control change, pending-command pause/resume, camera preference save/relaunch and safe shutdown. Compare domain state before/after view-only changes. Repeat without Nanite/HWRT. X04 adds real vehicle seat/chase integration rather than an unused vehicle-camera stub here.
- [ ] Shut down during queued work and confirm worker lifetime/drain behavior without use-after-free. Commit the real project and approved original assets through LFS.

**Check:** `uv run --frozen python scripts/build_game.py --platform win64 --configuration Development --engine-root "$CANOPY_UE_ROOT"` and the corresponding Mac/Linux commands, with `CANOPY_UE_ROOT` naming the authorized approved engine installation on that host. Expected: three native packages start and input works. Editor play-in-editor alone does not pass.

**F03 source qualification work (native acceptance still open):** the build driver now requires a full approved Editor dependency inventory, authors the room, runs the required Editor automation suite, packages and requires a fresh structured result from the executable. Worker updates mark the final event packet explicitly: an intermediate packet no longer clears target admission or movement restrictions. The target-batch regression and modifier-free key selectors are covered by native automation source, not by an observed Editor run. Local full-Editor preflight rejects Xcode27 instead of relaxing the pinned26.1.1 requirement; no approved high-capacity runner is registered. Packaged controller/keyboard/visual acceptance remains unexecuted.

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

**Execution record (qualification in progress):** the current F02 authority has owned capture/validated restore, bounded FlatBuffers snapshots, zstd/checksums, checkpoint-bound journal provenance, retained daily/manual/branch history and forward v1 migration into a new destination. Local macOS arm64 Debug, Release and ASan/UBSan each passed 41 native cases and all 15 actual process-kill scenarios (three writer modes × five durability stages). Recovery compares the complete canonical authority digest, rejects foreign or rewritten audit identity, retains a valid fallback after repeated interruption, and resumes journal writes. Tests also cover corrupt decoded snapshots, orphan segments, exact historical generations, migration capacity and audit retention beyond 512 segments.

The three-host source workflow now invokes the real crash helper in both configurations; those new hosted results are not yet recorded. Local Xcode27 is standalone evidence only. The persisted authority currently consists of F02 controls, commands/receipts, clocks, deadlines, seed and events; inventory, cash, crops and construction must extend this same schema and recovery proof when their domain tasks introduce those fields. No crop/economic recovery result is claimed here.

The [first portability rerun](https://github.com/vhark/canopy-foundry/actions/runs/37773159698) passed macOS but exposed two native configuration defects after the portable `zstd::libzstd` target repair. Linux's pthread probe invoked missing `clang-scan-deps`; the header-based core now explicitly disables C++ module scanning. MSVC's `<chrono>` required `_HAS_EXCEPTIONS=0` alongside disabled unwinding, exactly as UE5.8.1's `VCToolChain.AddExceptionArguments` configures its platform headers. The updated policy again passed 41 local cases and all 15 real process-kill scenarios; Windows/Linux still require their next native rerun before F04 closes.

The [next rerun](https://github.com/vhark/canopy-foundry/actions/runs/37775271163) confirmed Linux pthread detection and MSVC standard-library compilation. It then exposed host-tool selection incorrectly retaining Linux's `-ue-v26` target suffix, and MSVC rejecting the fault hook's `getenv` calls. Host `flatc` selection now strips either approved target-only suffix; Windows fault injection reads its stage through the bounded native API and its sentinel path through the Unicode API. Local rebuilding again passed 41 cases and 15 kill/recovery paths; the new Windows/Linux paths remain subject to native execution.

**External stop:** [run 37777558997](https://github.com/vhark/canopy-foundry/actions/runs/37777558997), containing those fixes at `a422667`, did not start any native job. All three check annotations report: “The job was not started because recent account payments have failed or your spending limit needs to be increased.” No self-hosted repository runners or configured SSH hosts are available. Restore Actions capacity or supply authorized native hosts before attempting the Windows/Linux acceptance rerun; this infrastructure refusal is not a test pass.


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

- [ ] Register `native-imported-room`: load the accepted IFC-derived room, inspect the mapped rack, verify scale/handedness, walk the door and switch third-person/first-person/overhead while selecting the same entity. Exercise camera obstruction at racks/walls and saved preference without changing domain state.
- [ ] Save/reload the fixture state and camera/selection, verifying the selected semantic entity and accepted source digest remain unchanged. Full construction/movement transactions are qualified later in B06/G02; do not introduce a second unvalidated geometry-edit implementation to pass this early platform proof.
- [ ] Run the room on Windows/Linux and Apple Silicon Mac with the conventional LOD/raster baseline, then enable supported higher-tier features. Capture camera-route evidence at 1080p and 4K output, including readable UI and collision.
- [ ] Verify the packaged runtime works with Python/Blender/Bonsai and the authoring service absent. Verify native Windows does not import or shell out to the POSIX store.
- [ ] Record all three results; if any target fails, fix that target or explicitly change the approved release scope before proceeding. No assertion that engine support alone proves product support.
- [ ] Commit the qualification recipe and permitted evidence references. This milestone proves integration, not fun, crop accuracy or flagship performance.

**Check:** `python scripts/qualify.py --case native-imported-room --platform mac-arm64` and both other target values on their native machines. Expected: mapped geometry/interaction/save tests and actual visual captures pass.
