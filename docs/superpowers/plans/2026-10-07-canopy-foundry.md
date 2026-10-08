# Grownetics: Canopy Foundry — Complete Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a high-fidelity, cross-platform growing-facility game with switchable third-person/first-person work and driving plus overhead management: build, operate, grow, diagnose, sell and expand. Reuse GrowBIM/OpenCEA and Blender/Bonsai/IFC authoring without runtime tool dependencies.

**Architecture:** Unreal Engine 5.8.1 for world presentation, input and interaction; engine-independent C++20 CanopySim for authoritative fixed-clock simulation; an offline Python3.12 creator/compiler pipeline emits versioned semantic and cooked render packs from accepted BIM and approved equipment sources. Single-player first, host-authoritative 2–4-player co-op later.

**Tech stack:** UE/UBT; C++20/CMake/Ninja; FlatBuffers and zstd; Catch2/CTest; Python3.12/uv/pytest; pinned GrowBIM/OpenCEA/IfcOpenShell; separately installed Blender/Bonsai; Git/Git LFS; private S3-compatible object storage; GitHub Actions with isolated native Windows/Linux/Mac build workers. Dependencies are selected here; exact installed artifacts and hashes are verified and locked in F01 rather than invented in a plan.

**Status:** F01, F02, F04 and B01 are complete; [F01's native build report](../../research/f01-build-evidence.md), [F02's authority/replay record](00-engine-core.md#f02--typed-state-commands-receipts-and-fixed-clock), [F04's native crash-recovery record](00-engine-core.md#f04--crash-safe-saves-and-replay-authority) and [B01's immutable upstream release](01-equipment-bim.md#b01-add-upstream-facilitymep-profile-and-accepted-artifact-contract) retain their evidence. The [public source matrix](https://github.com/vhark/canopy-foundry/actions/runs/37804761665/attempts/2) passed 41 core cases and all 15 process-kill scenarios in both Debug and Release on Windows, Mac and Linux, clearing the earlier hosted-runner billing refusal and F05's F04 prerequisite. B02's source compiler and the build tooling pass 86 local Python checks; real accepted-source CLI/native FlatBuffers decoding passed, but cooked mesh/collider/port alignment remains unproved. F03 and B02 require an approved full Editor/SDK and high-capacity native hosts; local Xcode27 is not the pinned26.1.1. No packaged game, qualified runtime benchmark, signed vendor deal or commercial title clearance exists. Unchecked work packages remain pending; this repository is separate from Grownetics Sim.

## 1. Decisions already made

| Critical aspect | Decision |
|---|---|
| Name | **Grownetics: Canopy Foundry**, working commercial title; repository `canopy-foundry`; name clearance before announcement |
| Experience | Third-person default with switchable first-person on foot/in vehicles, overhead spatial design and operations/business progression; one actor/state authority across views |
| Initial proof | **First Shipment**: 216 m² original facility, two production rooms sharing plant capacity, lettuce, real construction/commissioning, a crop cycle, diagnosis, sale and next investment |
| Platforms | Windows11 x64; macOS15+ Apple Silicon with M2 Pro reference tier; Linux x64 Ubuntu24.04 reference; 22.04 additional qualification |
| Rendering | Conventional LOD/raster baseline on all platforms; optional supported Lumen/Nanite enhancements; no required hardware RT or vendor-only upscaler |
| High resolution | High PC target: 3840×2160 output with TSR at 2560×1440 internal; UI at output resolution; native4K measured separately |
| Performance | Baseline PC1080p60; Mac1080p30; explicit p95/p99/memory gates and scale workload, **not achieved promises** |
| Simulation scale | Zones, equipment networks and crop cohorts; instanced representative plants; no Actor/Tick per plant; camera distance never lowers domain fidelity |
| Authority | Accepted upstream source → compiled game definition → fictional campaign state → explicit export candidate; only upstream review can accept a new BIM revision |
| Real equipment | Detailed original generic baseline plus separately approved SKU geometry, mechanics and operating maps; licensed vendor content does not block core progress |
| Sponsorship | Disclosed brand/showcase placement; no funding-dependent physics, hidden scoring advantage or exclusive progression gate |
| Modes | Career/Sandbox/Training at base release; replay and branching are tools on the same core |
| Release model | Paid offline single-player base game; no game account, compulsory telemetry, subscription or in-app currency; optional later content packs and co-op |
| Distribution | Steam primary storefront plus a DRM-free direct build channel, same core saves and approved packs; store purchase/download requirements are distinct from no in-game account. Store/publisher approvals are release prerequisites. |
| Initial commercial target | US$39.99 base-game list-price target; regional pricing and taxes set through store configuration after the small-loop value/playtest gate. This is pricing policy, not a revenue projection. |
| Deferred releases | Cannabis-specific crop/processing pack; internet/LAN co-op; vendor SKUs as agreements arrive. No console/mobile/browser/VR/MMO promise. |
| Validity | Initial physical/crop coefficients explicitly illustrative. Calibration and validation-for-stated-use are separate evidence gates, never conferred by visual quality or sponsorship. |

Normative requirements: [product specification](../../product-specification.md), [architecture](../../architecture.md), [asset pipeline](../../bim-and-asset-pipeline.md), [vendor program](../../vendor-program.md), [performance protocol](../../performance-and-qualification.md), [licensing](../../licensing.md). Evidence and observed existing limits: [source register](../../research/source-register.md).

## 2. Implementation pack and ownership

The seven subplans contain **60 work packages**, concrete future file paths, ordered implementation checklists, failure cases, smoke scenarios and expected outputs. Read the relevant normative section and task before changing code; do not create an alternate implementation to avoid an upstream dependency.

| Subplan | Packages | Delivers |
|---|---|---|
| [00 — Engine and domain core](00-engine-core.md) | F01–F06 | Reproducible native toolchains, IDs/commands/clock, game bridge, saves/replay, headless runner, three-platform imported room |
| [01 — Equipment/BIM/vendor](01-equipment-bim.md) | B01–B06, V01–V03 | Upstream general facility/MEP profile, source-bound compiler, original detailed equipment, typed ports, Bonsai exchange, construction, licensing/pilot/withdrawal |
| [02 — Coupled simulation](02-coupled-simulation.md) | S01–S08 | Utility operating points, climate balances, crop history, recipes/meters, workers/logistics, ledger/contracts, failures and integrated crop-to-sale proof |
| [03 — First Shipment gameplay](03-first-shipment.md) | G01–G08 | Walk/inspect/build/operate/plant/work/diagnose/ship/reinvest, actual equipment/crop visuals and formative playtest |
| [04 — Greenhouse and progression](04-progression-greenhouse.md) | X01–X07 | Weather/solar greenhouse, repeated tomato harvest, templates/expansion, industrial work, economy, cannabis pack and flagship campaigns |
| [05 — Performance and release](05-performance-release.md) | P01–P06, R01–R06 | Sealed workloads, streaming/render budgets, measured acceleration, long saves/campaigns, all-platform tour, secure builds/packs, accessibility/signing/release/rollback |
| [06 — Modes/training/mods/co-op](06-training-coop.md) | T01–T06, C01–C04 | Modes, diagnosis/branch debrief, replay, usable creator SDK, claim admission, host authority and actual native multiplayer qualification |

One integration owner owns each shared boundary: F02 core command/time/IDs; F03 runtime bridge; F04 save schema; F05 qualification dispatcher; B02 facility-pack schema/crosswalk; B04 equipment/port contract; B06 construction; S04 domain orchestration; T05 mod manifest; R01 native build/release artifact orchestration. Consumers extend these contracts rather than creating competing schemas, clocks, ledgers, port types, pack loaders or runners.

User camera addition: F03 owns the shared camera component and on-foot input/collision/preferences; G01/G05 qualify character, tool and work continuity; X04 supplies real vehicle operator/chase views; F06/P06/R03 qualify perspective transitions, obstruction handling, saved preferences and controller parity natively. Architecture A13 is normative; camera-only changes never alter the domain clock, work or inventory.

### Common commands and evidence

Run from this repository unless a task explicitly says upstream:

```text
uv run --frozen python scripts/build_core.py --config Debug
uv run --frozen ctest --test-dir .build/core --output-on-failure
uv run --frozen python scripts/build_game.py --platform win64 --configuration Development --engine-root "$CANOPY_UE_ROOT"
python scripts/qualify.py --case CASE_ID --platform win64 --output .work/qualification/CASE_ID
```

F01 supplies build entrypoints; F05 supplies the future qualification entrypoint. Build platform/configuration and the authorized engine installation path are explicit. For `qualify.py`, `--platform` defaults to the native host and `--output` to `.work/qualification`; shortened qualification commands use those defaults. Platform IDs are `win64`, `linux-x64`, `mac-arm64`. The rights checker separately uses the rights-manifest platform vocabulary; its scope mapping must be explicit, not stringly inferred.

Reports require case/status/commit/engine/model/seed/platform/settings/measurements/evidence; missing measurements are **not measured**, not zero. Unknown/skipped mandatory cases exit nonzero. A command listed in an uncompleted task is an acceptance contract, not evidence it ran successfully. Current source-build instructions and observed limits are in the [README](../../../README.md#native-source-build). Benchmarks must name source/model/recipe hashes, hardware, internal/output resolution and raw evidence. Native packaged interaction plus visual review is required for player-facing work; a headless pass is insufficient.

## 3. Dependency ledger

These are hard completion prerequisites, not barriers to reading/designing the next task. For source-only contracts, a downstream integration check is performed after its consumer lands; never create a dependency cycle by requiring that future consumer to exist before its input contract. External prerequisites are tracked separately in §8.

| Task | Hard task prerequisites | Completion evidence |
|---|---|---|
| F01 | — | Three native toolchains/build manifests and real core boundary test |
| F02 | F01 | Idempotent commands, stable clock/event order, bounded allocations |
| F03 | F01, F02 | Packaged scene/input, worker bridge and safe shutdown |
| F04 | F02 | Native crash injection, recovery/replay and protected migration |
| F05 | F02, F04 | Real headless program and fail-closed case runner |
| B01 | F01 | New upstream IFC facility/MEP profile and legacy acceptance tests |
| B02 | B01, F02 | Source digest/identity, SI/basis/area conversion and semantic pack |
| V01 | F01, B02 | Approved-source registry and rejected unauthorized asset intake |
| B03 | B02, V01 | Detailed original masters and repeatable native render cooks |
| B04 | B02, F02 | Typed ports/curves/domains and graph-admission contract |
| B05 | B01, B02 | Narrow plus new-profile Bonsai candidate diff/review/acceptance |
| F06 | F03, F04, F05, B02, B03 | Imported room actually walked/inspected/saved on all three OSes |
| S01 | F05, B04 | Network operating points/shared capacity and delivered quantities |
| S02 | F05, B04 | Analytic and conserved energy/moisture/gas balances |
| S03 | F05, B02, B04 | Cohort/history/harvest conservation and full illustrative cycle |
| S04 | S01, S02, S03 | Causal coupling, control boundaries and metered resources |
| S06 | F05, B02 | Stock/ledger/contract transactions and exactly-once dispatch |
| S05 | F05, B02, S03, S06 | Work/route/reservations, storage and lift/aisle contention |
| S07 | S04 | Seeded failures, observation freshness and validity policy |
| B06 | B01, B02, B04, F03, F04, S01, S05, S06 | Atomic construction, safe movement/commissioning and export candidate |
| S08 | S04, S05, S06, S07, B06 | Integrated First Shipment and calibration-report ingestion |
| G01 | F06, S08, B06 | Navigable real facility and understandable inspection |
| G02 | G01, B06 | Interactive build/connect/commission and refusal/cancel behavior |
| G03 | G01, B03, S04, B06 | Operable equipment, moving aisle and filter recovery |
| G04 | G01, S03, B03 | Visible crop/history/harvest without duplicate inventory |
| G05 | G01, S05, S06, G03, G04 | Personal work/helper/automation with actual stock and wages |
| G06 | G01, S06, G04, G05 | Native crop-to-shipment and reconciled operating statement |
| G07 | G01, S07, G03, G04 | World/trend diagnosis and two corrective action branches |
| G08 | G02, G03, G04, G05, G06, G07 | Continuous first/second cycle and five-person formative gate |
| P01 | F05, B02, B03, G01 | Sealed actual-small/generated-scale workloads and tour |
| P02 | P01, F03, B03, S05 | Streamed/instanced presentation preserves domain state |
| P03 | P02, B03, F03 | Baseline/high lighting, PSO/texture and geometric fidelity |
| P04 | P03, S08, F04, F05 | Measured native frame/memory/hitch and acceleration reports |
| T05 | B02, B03, V01, F03, F04 | Safe usable local/curated creator-pack SDK and trust policy |
| R01 | F01, B03, F05 | Trusted native build/cook lanes, private artifacts and manifests |
| R02 | B02, B03, V01, T05, F04, R01 | Pack abuse/rights tests, provenance and SBOM |
| X01 | G08, S08 | Weather/solar/vent/screen greenhouse balances |
| X02 | X01, S03, S05, G04, G06 | Two actual tomato pick/pack/dispatch cycles |
| X03 | G08, B06, S05, S06 | Templates, staging, outages and paid expansion |
| X04 | X03, S05, S06 | Staff/fleet/automation with real routing and downtime |
| X05 | X03, X04, S06 | Multi-cycle economy/recovery/forecast without arbitrage |
| X07 | X01, X02, X03, X04, X05, P04 | Authored million-ft² greenhouse and 60,000-ft² indoor campaigns |
| X06 | X03, X05, G08 | Separate cannabis production/processing pack plus legal gates |
| T01 | G08, F04 | Career/Sandbox/Training policies on one simulation |
| T02 | T01, S08, G07 | Executable diagnosis scenarios with distinct recovery choices |
| T03 | T02, F04, F05 | Actual alternative-action reruns and causal debrief |
| T04 | F04, G08 | Native timeline/bookmark/branch interactions |
| T06 | T02, T03, S07 | Claim admission; illustrative remains usable without stronger evidence |
| P05 | G08, S08, F04, P01 | 30/90-day and 100 save/load cycles with fault recovery |
| R03 | G08, F03 | Controller/accessibility/localization implemented and exercised |
| P06 | P04, P05, G08, R03, R01 | Three-platform pre-store packaged replay/input/visual tour |
| V02 | V01, B03, B04 | One genuinely licensed SKU, masters/data and held-out review |
| V03 | V01, F04, F05, R01, R02, P06 | Generic-only/withdrawal policy; real SKU release additionally needs V02 |
| R04 | R01, R02, P06, V03 | Signed/notarized distribution, store/title/age/rights approvals |
| R06 | F04, P05, R01, R02, R04 | Rehearsed update/migration/rollback and support process |
| R05 | G08, X01, X02, X03, X04, X05, X07, T01, T02, T03, T04, T05, T06, P06, R02, R03, R04, R06, B05 | Base-game release decision with all shipping content qualified |
| C01 | R05, F02, F04, P06 | One authoritative host; conflicting/retried commands safe |
| C02 | C01 | Actual LAN and authorized EOS internet join/relay across OSes |
| C03 | C01, C02 | Four players, shared-time consent, construction and reconnect |
| C04 | C03 | Native host rotation, latency/loss soak, privacy/offline regression |

The base flagship indoor campaign uses lettuce/multi-tier production; X06 adds the cannabis crop and dedicated processing configurations after their own gate. X06 and V02 are not prerequisites for generic base-game release. Stronger professional/training efficacy claims require external evidence under T06; base Training is explicitly illustrative. This preserves the complete roadmap without silently making sponsor/legal/data participation mandatory for playing the base game.

## 4. Delivery gates and order

### Gate A — Native/BIM spine, before production gameplay content

- [ ] F01–F05, B01–B04, V01, F06 and B05 complete at their dependency order.
- [ ] A real accepted IFC-derived room loads and can be inspected at correct scale/basis on Windows, Mac and Linux. It saves without the authoring toolchain installed.
- [ ] New general facility/MEP operations have their own upstream profile and real reconciliation qualification; the historical single-rack demo is not relabeled as that proof.
- [ ] Generic fan/bench/light meshes are detailed and collision/ports match. Original-source approvals and three-platform cook evidence exist.

### Gate B — Coupled systems, not decorative simulation

- [ ] S01–S08 and B06 complete, including energy/mass/stock/ledger conservation and no per-step allocations in reserved steady state.
- [ ] Headless commissioning, a full crop cycle, failure/recovery, actual sale and continuation work through public commands. A room expansion competes for the same shared plant.
- [ ] Invalid state/connection/model or exhausted compute budget yields a specific refusal/stop, not free capacity, dropped time or made-up output.

### Gate C — First game and early scale risk

- [ ] G01–G08 complete with a five-person formative playtest; four of five finish and explain a meaningful equipment–climate–crop–cost consequence without coaching.
- [ ] P01–P04 run before expensive flagship art. Native render/frame/memory and 1440× targets are measured on the declared hardware using active representative workloads.
- [ ] First Shipment has detailed equipment/crops, personal work, helper/automation, recovery options, one sale, another cycle and an investment decision. An imported walkable room or headless demo alone fails.
- [ ] Missing fun or performance triggers an implementation/design correction and recheck, not a larger facility to disguise the problem.

### Gate D — Full base-game content and tools

- [ ] X01–X05/X07; T01–T06; T05 safe creator SDK; R01/R02 secure native/artifact pipeline complete.
- [ ] Eight-compartment contrast greenhouse runs repeated tomato harvest; full-build flagship greenhouse has **1,000,000 ft² ground-level building footprint**, indoor has **60,000 ft² total gross floor over three 20,000-ft² storeys**. Canopy and rack tiers remain separate quantities.
- [ ] Both actual authored flagships are played/qualified in addition to the generated stress fixture. Production/dispatch/failure/logistics are connected, not empty buildings.
- [ ] Base Career, Sandbox, illustrative Training, replay/branching and local creator tools work without sponsors or network services.

### Gate E — Single-player release

- [ ] P05/P06, R03–R06 and V03 generic-only policy complete on every target; rerun on actual shipping X/T content.
- [ ] No missing native platform, unsafe data pack, save-loss/economy duplication, undisclosed validity claim, prohibited asset, inaccessible core action or unresolved blocker.
- [ ] Store/title/rating/signing/engine-license obligations satisfied; clean install, offline gameplay, update and rollback demonstrated.
- [ ] Publish machine/build/settings-specific measured performance and known limits. Do not promise every Mac or native4K60 based on one reference machine.

### Gate F — Conditional content and co-op releases

- [ ] V02/V03 per actual SKU grant, X06 per content/legal gate, and C01–C04 per online entitlement/privacy/platform evidence. These are separate releasable packages with complete functionality, not placeholders in a base-game menu.
- [ ] Co-op requires real LAN and authorized EOS sessions, host rotation across Windows/Linux/Mac, four participants, reconnect and a faulted network soak; offline play still succeeds when EOS is unavailable.
- [ ] Each release repeats affected content/rights/save/native qualification and supports rollback. Vendor expiry never silently changes existing physics.

## 5. Parallel execution lanes and integration rules

After F01/F02's shared contracts are established:

1. **Engine/platform lane:** F03/F04/F05, later P/R native build/performance work.
2. **BIM/technical-art lane:** B01/B02/V01, original equipment/cook, Bonsai round trip and later approved vendor pilot.
3. **Domain lane:** S tasks and B06; first stabilize B04 ports and F02/F04 APIs with the integration owner.
4. **Gameplay/content lane:** G tasks against real domain APIs, then X/T content. Prototype interaction/art independently but do not count a mocked subsystem as acceptance.

Do not fan out siblings over the same schema/source file. Agree typed inputs/outputs first, allocate disjoint files, and integrate one cohesive task at a time. S01 network equations and S02 balances may be developed independently once their contracts are fixed; S04 couples them. S06 stock/ledger precedes S05 tasks consuming stock. R03 implements controls **before** P06 verifies them; T05 implements mod admission **before** R02 audits it; V03 audits rights **before** R04 approves distribution. These distinctions avoid circular plans.

For each work package: add useful failing boundary/behavior tests where uncertainty warrants them, implement real behavior, run relevant tests and smoke, inspect the native player surface where applicable, record model/pack/provenance and platform evidence, update operational documentation, and commit the cohesive change. Review requirements and security/rights/performance consequences before advancing its completion gate. Test stubs, source-text snapshots and mocked qualification output are not completion evidence.

## 6. Numerical targets and workload policy

| Gate | Initial proposed target |
|---|---|
| Windows/Linux baseline | Ryzen5 7600 + RTX4060 8 GiB + 32 GiB RAM; 1080p60; p95<=16.7 ms, p99<=25 ms |
| High Windows | Ryzen7 7800X3D + RTX4080; 4K output/1440p TSR; same60fps frame gates; native4K logged separately |
| Native Mac | M2 Pro16 GiB; 1080p medium30; p95<=33.3 ms, p99<=50 ms; no required Nanite/HWRT |
| Memory | Core<=512 MiB; baseline game<=10 GiB and VRAM<=7 GiB; Mac game-attributable unified<=10 GiB; high VRAM<=12 GiB |
| Generated stress | 256 zones; 16,000 equipment; 20,000 cohorts; 1,000 logical workers; 1,000,000 representative plant instances; <=64 detailed nearby workers |
| Time acceleration | Windows/Linux seven virtual days in <=420 real seconds at requested1440× with real events/production and renderer running; report actual Mac speed |
| Shader startup | After first usable view: zero >100-ms PSO/shader hitches in cold and warm routes; other hitches reported/triaged |
| Geometric conversion | Fixed interfaces/end-stops<=10 mm, visible architecture<=25 mm from admitted source; source-to-real-SKU uncertainty tracked separately |
| Persistence | 30/90 virtual days,100 repeated save/load cycles, deterministic crash phases, preserved original save on migration |

The full [qualification protocol](../../performance-and-qualification.md) controls sampling, hardware/settings, percentile computation, route/workload sealing and raw evidence. A failed budget is a failed budget until corrected or explicitly revised; no sim simplification by camera distance, hidden frame generation or reduced integration fidelity.

## 7. Requirement-to-acceptance coverage

| Requirement | Implementation owners | Observable acceptance |
|---|---|---|
| A separate named game/repository | This planning deliverable, R04 title clearance | Private repo and linked specifications now; cleared commercial name later |
| High-resolution, performant cross-platform game | F01/F03/F06, B03, P01–P06, R01/R03/R04 | Real native packages, declared internal/output resolutions and measured reference tiers |
| GrowBIM/OpenCEA/IFC/Blender/Bonsai reuse | B01/B02/B05/B06, F06 | Accepted-source hashes/crosswalk, unit/basis tests, actual profile round trip, candidate-only exports |
| Real racks/rolling benches/ducted fans/components | B03/B04/B06, G02/G03, S01/S04/S05 | Detailed movable geometry, typed ports, safe working aisle, real airflow/capacity/meter consequences |
| Vendor files and sponsorship | V01–V03, R02/R04 | Signed SKU-scoped grants, approved conversion/data, build denial without rights, lawful save/withdrawal behavior |
| Full game, not only tooling or a dashboard | G01–G08, X01–X05/X07 | Hands-on crop-to-sale-to-next-cycle, weather-driven greenhouse and populated flagship campaigns |
| Coupled climate/crops/equipment/economy | S01–S08 | Conserved analytic examples, state history, actual transactions and alternative recovery outcomes |
| Area/storey/tier correctness | B01/B02, X07 | Explicit polygons/area sums; indoor60,000 total over3 storeys; greenhouse1M ground-level footprint |
| Time/replay/long campaigns | F02/F04/F05, T04, P04/P05 | Same-build outcomes across advancement partitions and saved replay; no duplicate sale or lost state |
| Training without false certification | T01–T03/T06, S07 | Executed diagnosis/branch cases; stronger labels denied without stated-use evidence |
| Extensibility without unsafe mods | T05, R02 | Usable original example pack, bounded admission and real adversarial rejection |
| Complete later co-op | C01–C04 | Cross-platform4-player full loop, authenticated encrypted transport, reconnect/fault soak |

## 8. Resources, risk register and stop conditions

Assign accountable roles before execution: one technical/integration lead; core simulation engineer; Unreal gameplay engineer; technical artist/BIM engineer; environment/equipment artist; gameplay/economy designer; native QA/performance owner. Agronomy/MEP reviewers and licensing counsel review scoped datasets/claims/grants. One person may hold multiple roles, but the review responsibility must not disappear. Hardware/SDK access and real content work are required; a fleet of coding agents does not replace rights grants, qualified review, native GPUs or actual playtest participants.

| Risk / unavailable prerequisite | Owner / task | Decision and stop rule |
|---|---|---|
| Existing BIM toolchain is only narrowly qualified | BIM lead B01/B05 | Add/qualify new upstream profile; never weaken the accepted placement-only route or claim arbitrary round trip |
| Windows upstream local-store `fcntl` dependency | BIM/platform lead B01/B05 | Creator on supported host/service; all game platforms consume packs. Native Windows creator needs a separately qualified backend |
| Unreal engine/SDK entitlement, actual build machines | Platform lead F01/R01 | Obtain approved artifacts and native access; do not invent installed version hashes or count cross-compilation as a native run |
| Fine geometry, foliage, glass, moving lights exceed budget | Technical art P01–P04 | Profile before flagship art; instances/LOD/visibility/PSO improve presentation without dropping domain work |
| Fast simulation exceeds target | Core lead S04/P04 | Profile bounded network/zone/cohort work; display achieved speed, preserve events, fix design or explicitly revise target |
| Real SKU masters/operating maps/license unavailable | Vendor lead V01/V02 | Ship detailed original generic alternative; vendor pilot remains blocked, not filled with scraped CAD or a mock license |
| Crop/MEP data not independently qualified | Domain review S08/T06 | Ship honestly illustrative models; stronger claims remain blocked until licensed data/review supports them |
| First cycle feels like chores/dashboard/quiz | Design lead G08 | Observe fresh players, repair loop/interaction/investment choices before expanding art scope |
| Rights expiry or pack/version change breaks old saves | Release lead F04/V03/R06 | Pin where lawful; otherwise explicit new-save migration and notice, never pretend unchanged physics |
| Name, store, age/rating, signing or engine obligations unresolved | Producer/counsel R04 | Original source may be public under its noncommercial license; keep restricted content private and block affected packaged/commercial releases until cleared |
| Online credentials/terms/privacy approval missing | Network lead C02 | Base single-player unaffected; no claimed internet co-op pass without real service qualification |

Schedule by demonstrated gates, not a fabricated fixed ship date. Gate C produces the first defensible production forecast because it measures the complete game loop, actual content throughput and real scale costs. Track outstanding prerequisite, responsible role and evidence needed; do all independent work without making up unavailable data.

## 9. Starting execution

- [ ] Confirm the documented baseline and licensed hardware/tooling access; create an implementation worktree/branch from this plan commit.
- [ ] Execute F01, then F02 and the first independent engine/BIM lanes with the shared contracts above.
- [ ] Maintain this dependency ledger and task checkboxes as work passes real acceptance; record explicit design changes rather than silently editing the meaning of a completed gate.

The initial deliverable after implementation starts is **Gate A's genuine three-platform BIM-derived room and authoring exchange**, followed by the coupled headless proof and the actual First Shipment game. Do not replace the complete plan with that early milestone or advertise it as the finished game.
