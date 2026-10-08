# Architecture and decision record

Baseline 2026-10-07. Decisions below are binding for the implementation plans; changing them requires an explicit decision record and revised qualification gates. Sources and observed limitations are in the [source register](research/source-register.md).

## A01 — Unreal presentation, independent domain core

Choose **Unreal Engine 5.8.3**, **C++20**, CMake for headless tools/tests and Unreal Build Tool for the game. The Mac toolchain is pinned to stable **Xcode 27.0** with macOS SDK 27.0; exact engine distribution and compiler evidence are checked before packaging. Unreal supplies rendering, spatial interaction, audio, input, UI, navigation presentation, packaging and later network transport. It does not own crop, climate, inventory, scheduling or business truth.

Alternatives considered: a custom Three.js/browser client lowers web deployment friction but puts high-end first-person rendering, authoring and native interaction infrastructure on the project; Godot offers an open engine but requires more project-owned high-end asset/render pipeline work; Unity is viable, but adds managed/native integration choices without an existing project advantage. Unreal best matches the high-fidelity industrial-world requirement. Its licensing, large build footprint and uneven rendering-feature support are accepted costs, with explicit fallback profiles and platform builds.

Do not depend on experimental Mac ray tracing, Mac beta Nanite, mandatory MegaLights, per-plant Actors or vendor-only upscalers. New 5.8 vegetation features may be evaluated as tools, not required for the first crop's core representation. A later engine patch is a deliberate upgrade with saved-pack, import and platform regression checks—not an automatic dependency update.

```text
GrowBIM/OpenCEA accepted program + IFC + identity + catalog
             │                    Vendor masters + signed rights
             │                         │
             └──── Python validation / Blender+Bonsai authoring ────┐
                                                                    │
                      Offline pack compiler + Unreal asset cook ◀───┘
                                    │
               Semantic pack + render pack + provenance/rights manifest
                                    │
                      C++ CanopySim authoritative state
                       │                          │
                       │                     Headless CLI
                       │ commands + immutable views/events
                       ▼
                      Unreal CanopyRuntime bridge
                       │ first-person/build/UI; later co-op host
                       └ shared core save/replay authority
```

## A02 — Four distinct records, no competing BIM authority

1. **Accepted source design:** immutable GrowBIM program/design revision and original IFC/identity/catalog bytes. OpenCEA remains the authority for what was actually accepted and the evidence attached to it.
2. **Compiled facility definition:** disposable game-specific semantic and render projection bound to source hashes, compiler version and equipment/model pack hashes. Rebuilding it cannot silently change the source design.
3. **Campaign world:** game-owned, explicitly simulated construction and operational state derived from the definition. Building a new room creates a semantic construction transaction, not only meshes, and does not claim that real equipment was installed or that a GrowBIM HEAD changed.
4. **Export candidate:** an explicit campaign design export through authoring tools, with source revision, generated identities and reviewable diff. Only upstream review/acceptance creates a new GrowBIM revision. It never rewrites the campaign's historical source binding.

Game construction works offline on all three platforms. It must not launch Blender or the POSIX-only GrowBIM local store whenever a player moves a bench. Importing/exporting engineering artifacts is a creator workflow; playing is not contingent on that workflow being installed. Immutable imported buildings can be designated protected by scenario policy; changes create a simulated derivative branch with origin recorded.

IDs are typed 128-bit identifiers: project, source element, product type, game instance, room/zone, cohort, task, connector and contract. A game `instance_id` is not an OpenCEA reported installed `asset_id`. Persist the crosswalk to IFC GlobalId and original element UUID where one exists. New game instances derive unique IDs from campaign namespace plus an authoritative monotonic creation counter. Copy allocates a new instance ID while sharing type/mesh; replacement preserves the functional role only by an explicit operation.

## A03 — Repo/module map

These paths are planned, not existing implementations:

```text
contracts/                  JSON Schemas + FlatBuffers game-only contracts
core/include/canopy/         Pure domain API, IDs, units, views, commands
core/src/                   Clock, facility, networks, climate, crop, jobs, economy
core/tests/                 Consumer-visible domain/contract regression tests
apps/canopy-headless/        CLI runner, benchmark and replay verification
apps/canopy-author/          Python package compiler and external BIM adapters
game/CanopyFoundry.uproject  Native game project
game/Source/CanopyFoundry/   Player, interaction, construction, missions, UI
game/Plugins/CanopyRuntime/  Thin bridge + external C++ library build bindings
game/Content/               Approved Unreal assets/maps, Git LFS
content/definitions/        Original equipment, crop, scenario and economy inputs
content/approved-sources/   Redistributable source art only, Git LFS
benchmarks/                 Original generated facility workloads + camera routes
scripts/                    Reproducible build, cook, package and qualification entrypoints
tests/                      Pack/authoring/packaged-game integration tests
.github/workflows/          Source-only, authoring and trusted native-build lanes
```

Feature ownership is in the subsystem plans. Do not copy upstream JSON schemas into `contracts/`; reference upstream validators in the authoring environment and version the game's projection separately.

## A04 — Runtime interface and thread ownership

The pure core has no UObject, rendering, sockets, Python, OS-store, wall-clock or GPU dependency. Header dependencies are the standard library plus explicit serialization types. No per-step heap allocation in normal simulation after scenario load; reserve bounded buffers and validate capacities before admission.

Boundary sketch; task F02 defines these concrete types and implementations:

```cpp
using SimSecond = std::int64_t;
struct Id { std::uint64_t high; std::uint64_t low; };
struct CommandHeader {
    Id command_id;
    Id actor_id;
    std::uint64_t expected_revision;
    SimSecond issued_at;
};
struct AdvanceBudget {
    SimSecond target_second;
    std::uint32_t max_substeps;
};
enum class AdvanceStop { ReachedTarget, BudgetExhausted, DecisionRequired, InvalidModel };
struct AdvanceResult { SimSecond reached_second; AdvanceStop stop; };
```

The command variant contains typed payloads: place/move/connect/commission equipment, set recipe/schedule, seed/transfer/harvest cohort, reserve/complete job, purchase/pay/dispatch, maintain equipment and scenario controls. Acceptance returns a receipt and state revision or a typed rejection; retries with the same command ID and payload are idempotent, changed payload reuse conflicts. No caller writes state fields directly.

One domain worker owns mutable state. The game thread enqueues validated intent through a bounded single-producer/single-consumer queue (later network requests are serialized by the host). The worker produces changed-entity views and immutable event batches in preallocated double buffers. No UObject is touched off the game thread; no save or IFC work blocks the render/game thread. Requests that exceed queue capacity are visibly rejected/back-pressured, not dropped silently. Background disk writers receive stable snapshots, not pointers into changing arrays.

Structure-of-arrays stores keyed through stable ID-to-index tables cover zones, equipment, cohorts and jobs. Arrays are dense; stable IDs survive compaction. Bulk physics solves operate on connected networks and zones, not on Actors. Actor pooling is a presentation policy only.

## A05 — Time, ordering and determinism

Use integer simulation seconds with integer substep ticks where needed. Render delta time never enters domain equations directly. Controls, reservations and energy metering update on 1-second boundaries; climate/air transport on 5-second boundaries; crop integration on 60-second boundaries using accumulated exposure; long-horizon growth summaries and finance postings at declared event boundaries. A control change, failure, topology change or harvest splits the integration interval exactly at its timestamp.

At one timestamp use this stable order: external commands and failures; topology/control decisions; shared utility allocation; energy/water/mass integration; crop exposure/development; task completion and inventory moves; contract/finance posting; alarms and explanatory events. Stable entity-ID order breaks ties. Solve cycles such as cooling/dehumidification feedback with a bounded convergence algorithm rather than arbitrary engine Tick order.

1×/5× are hands-on modes. 60×/360×/1440× and skip-to-event are management/time-lapse modes; direct carrying/driving is suspended with a clear mode transition while abstract tasks continue. Single-player pause freezes domain time. Never accelerate by dropping crop/failure events or increasing the numerical step because the camera is far away. Work budgets may yield across frames with an honest achieved-speed display. Fast-forward can stop at an authored decision or contract/failure boundary.

Same model pack, seed, build/toolchain and accepted command/event stream must replay identically. Counter-based per-entity random streams avoid draw-order coupling. Disable unsafe fast-math/FMA-contraction changes in qualification builds and sort reductions. **Do not promise bit-identical floating-point results across ARM/x86/compiler versions.** Cross-platform parity uses declared quantity tolerances and identical discrete outcomes; saved snapshots are authoritative. Co-op sends host outcomes, not peer lockstep predictions of the solver.

Rendering/streaming distance must not change domain fidelity. Distant workers use the same reserved task/path durations; close-up animation illustrates progress, not an alternate simulation.

## A06 — Coupled balances, not independent bonuses

Represent each well-mixed climate zone by air dry-mass/volume, moist-air energy or enthalpy, water-vapor mass, CO₂ mass and thermal-mass/envelope state. Temperature/RH/VPD are derived views with model-domain checks. Crop cohorts accumulate light exposure, water uptake, development and stress.

Conservation interface:

```text
Δ zone energy = electric + solar + boundary/air enthalpy inflow
                - boundary/air enthalpy outflow - exported heat - stored chemical energy
Δ water vapor = crop evaporation + vapor inflow - condensation/removal - vapor outflow
Δ tank water  = refill + valid return - irrigation delivery - bleed/leak
Δ CO₂ mass    = supply + inflow + respiration - assimilation - outflow
crop growth   = pack-defined response(light, CO₂, temperature, water/nutrient state, history)
cost          = metered resource use × active tariffs + wages + inputs + capital/finance
```

Do not add lamp electricity as sensible heat and independently add all absorbed light again. Transpiration transfers energy between sensible and latent stores; condensation and dehumidifier condenser location determine where heat returns. Indoor standalone dehumidification and external heat rejection therefore differ. Network electricity includes pumps/fans/plant equipment; requested capacity is not automatically delivered capacity.

Topology is a typed graph of ports and edges. Separate electrical, water, drain, air and control networks with explicit units, media, connection direction, rated operating domains and service zones. A fan's operating point intersects its curve with network pressure loss; a manufacturer's family-wide max flow and max pressure are not one point. Shared cooling/moisture capacity allocates through deterministic priorities and reports unmet demand; it is not duplicated into each served room.

Initial solver: well-mixed lumped zones, semi-implicit energy exchange and bounded monotone network operating-point solves. Use 5-second climate steps with deterministic subdivision for stiff transitions. Reject negative mass, impossible states or failed convergence; roll back the failed step, emit a diagnostic and pause if the minimum permitted step still fails. Numerical clamping may only remove documented floating-point residuals, never conceal broken conservation.

At least two published operating conditions are required before calling an equipment performance fit calibrated; validation additionally requires held-out independent comparison and a stated operating domain/tolerance. Empty/out-of-range tables cannot silently extrapolate. An illustrative substitute is a separate explicit generic definition, not a branded fallback pretending to be measured.

Crop response coefficients are model-pack inputs, not hardcoded renderer curves. Initial lettuce/tomato/cannabis packs are illustrative until individually qualified. Visual leaf size, color, density and stage derive from cohort state but do not introduce additional invisible yield logic.

## A07 — Logistics and rolling equipment

There are three connected but distinct representations: semantic access graph (doors, aisles, lifts, stations); logical equipment motion constraints (rail travel, open aisle, service reach); and local collision/navigation for the player/near workers. A rolling bench changes aisle availability and swept clearance without changing its catalog dimensions or source identity.

Routes reserve edges with capacities, direction, width/height and transport class. Multi-storey transfers include lift travel/loading/unloading/queue capacity. Remote travel time uses the same authored distances/speeds as near work, not teleportation based on camera position. Moving a bench/partition invalidates only impacted route/network caches at an atomic topology revision. Reject construction that traps a required job or player; offer an explicit move/evacuate plan before committing an obstructing change.

Person/cart collision uses simple capsules/convex shapes and validated sweep envelopes. Chaos interactions are not a scientific structural/load simulator. Rated rack loads remain sourced declarations; gameplay storage limits are labeled rules unless engineering qualification exists.

## A08 — Rendering and high-resolution strategy

Unreal consumes cooked assets; no IFC parsing, Blender subprocess or CAD tessellation during normal gameplay. Render packs provide PBR materials, authored pivots/motion, collision, ordinary LODs and optional Nanite variants. Use instanced static meshes/HISM or measured engine GPU instance paths for repeating benches, fixtures, pots and plants. Nearby cohorts generate representative plant clusters; no Actor/Tick/material instance per plant.

World Partition/cells and HLOD stream geometry and presentation, not simulation authority. Facility/storey/compartment IDs define meaningful visibility groups. Window/glass translucency, leaves and shadows receive explicit overdraw/LOD budgets. Keep plant silhouettes at distance using simplified meshes/impostors; use masked foliage only when measured against geometry-based alternatives. Do not assume Nanite makes greenhouse glazing, alpha foliage, animated plants or draw calls free.

Lighting fixtures are visually emissive while a bounded set of room/proximity lights produces illumination. The simulation uses catalog power and crop-relevant light distributions, not rendered exposure or screen brightness. Lumen/TSR are optional quality tiers; baseline conventional LOD/raster lighting works without hardware RT, Nanite or an NVIDIA plugin. Local purchased lights must still visibly turn on/off in the baseline; use dynamic direct lights plus measured ambient/reflection approximations rather than baking an immutable final layout.

Choose 4K output with declared internal render resolution/TSR as the high-end target. Native 4K is a separately measured quality option. UI remains native-resolution and readable; 4K textures are reserved for close hero surfaces, not every repeated shelf or leaf. [Performance budgets](performance-and-qualification.md) control asset admission and capture settings.

## A09 — Packs, save/replay and compatibility

Authoring inputs are validated JSON plus original/licensed source artifacts. Runtime semantic packs use versioned FlatBuffers with checked lengths/enums/references; cooked Unreal content is referenced by stable catalog keys and pack hashes, not saved UObject pointers. FlatBuffers and zstd are explicit third-party dependencies locked at F01; generated schemas and compiler versions participate in the pack hash.

A content manifest includes schema version, source revision/digests, converter/engine toolchain versions, build recipe, neutral equipment IDs, optional vendor SKU, rights approval ID, model-validity domain, platform variants and dependencies. No arbitrary remote URL execution or native plugin loading from user packs.

Saves contain: campaign ID/schema; source and pack hashes; authoritative time/revision; model/scenario seed and per-stream counters; constructed layout/topology; equipment/cohort/job/inventory/finance state; scheduled events; accepted/rejected command receipts; checkpoint/journal references. Journals record commands and external outcomes, not a copy of every array each tick. Display trends are separately quantized/compressed and never used as replay authority.

Write a new checksummed snapshot and append journal segment, flush, then atomically replace the save manifest. Preserve the previous valid manifest until durability is confirmed. POSIX uses file/directory fsync and atomic rename; Windows uses the qualified replacement/flush APIs. Interrupted saves recover the last committed revision; corrupt/missing assets produce a specific load error, not reset-to-empty success.

Keep seven rolling daily checkpoints plus explicit season/branch checkpoints; cap automatic retention by configured disk budget, without deleting manual saves. Retain all game-economic/construction decisions needed for the selected replay window; older climate trends become daily aggregates. Surface available replay range. Forward migration writes a new save and never overwrites the only old-version copy. Tests cover old-save load, missing optional branded mesh, incompatible mandatory model pack, duplicate purchase/dispatch and interrupted writes.

Render-only sponsor removal substitutes a rights-cleared generic presentation with the same envelope/pivots/ports. Saved performance definitions remain pinned only where their license permits perpetual existing-save use. If performance data must legally be removed, use an explicit versioned migration with a changed-model notice and preserved original save subject to legal obligations; do not claim bit-identical physics after substitution.

## A10 — Tools and upstream extension boundary

Existing OpenCEA program 0.1/design-envelope 0.2 validation is used as supplied. Bonsai is a separate authoring program, not the game renderer. Existing placement-only reconciliation does not support arbitrary room, duct or equipment edits; extend upstream through new profile modules and acceptance tests instead of weakening its checks.

The authoring compiler is a separate Python environment using pinned GrowBIM/OpenCEA artifacts and optional IfcOpenShell. Blender executes an isolated headless script with explicit paths, no arbitrary uploaded Python, and versioned export settings. STEP/RFA/DWG support depends on authorized conversion tools/vendor exports; Bonsai is not described as a general RFA importer. GLB plus explicit semantic sidecars is the default mesh exchange. Datasmith is an optional offline accelerator that must pass Windows/Mac/Linux creator tests before adoption, never a shipping runtime dependency.

Windows users play native builds without `fcntl` or a GrowBIM local store. Creators can use a supported local/service authoring host. Native Windows authoring support is a separate upstream locking/backend qualification task, not an ImportError bypass. Commercially redistributable converter tooling remains separate from Unreal engine code where licenses require that boundary.

## A11 — Business, co-op, mods and external services

Currency uses signed 64-bit integer minor units with overflow checks and balanced ledger postings. Stock carries batch, quantity, unit, grade, age, location and reservations. Transfers conserve stock; dispatch is an atomic inventory/ledger/contract transaction. Currency/market values are fictional balance unless explicitly sourced/licensed.

Launch is offline single-player. Later co-op has one host running the same core; clients request commands with permissions and intent revisions. Host assigns authoritative ordering/time, validates reach/ownership and sends interest-filtered snapshots/events. Shared construction/purchases require reservations and exactly-once receipts. Fast-forward requires a host policy and consent; reconnect gets a bounded fresh snapshot. No platform lockstep, no client-authoritative money, no host migration in the first co-op gate.

Mods are declarative equipment/scenario/model parameters and approved cooked assets with schema, budget and rights checks. No native code or arbitrary scripts in launch mod packs. Steam Workshop is not needed to run the game; distribution integration comes after local signed/unsigned trust-policy tests. Restricted vendor models are excluded from the public mod SDK.

Grownetics OS, live telemetry and external AI are not runtime prerequisites. Read-only historical data adapters may produce explicit replay packs after permission review. This game never writes to real actuators. A future real-controls product requires a separate safety/security architecture and authorization.

## A12 — Delivery and licensing

Git holds source, plans, schemas, original definitions and approved LFS art; content-addressed object storage holds restricted masters, builds and profiling evidence. Each sponsor has access isolation and retention rules. Self-hosted trusted Windows, Linux and Mac runners cook native builds with licensed SDKs; untrusted pull requests never access them or secrets. No engine binaries or restricted source assets go to public CI artifacts.

Code signing/notarization, store/age ratings, SBOM and notices, engine royalties, trademark clearance, content rights and privacy review are launch gates. Engine licensing is an accepted commercial dependency, not proof all Unreal-adjacent assets are covered. See [licensing](licensing.md) for the separation.

## A13 — One actor, three presentation views

F03 owns a `FacilityCameraComponent` on the possessed character/vehicle presentation, not inside CanopySim. Use explicit `FirstPerson`, `ThirdPerson` and `Overhead` view states with remembered on-foot/vehicle hands-on preference. Third-person is default; a collision-tested spring arm retracts against walls/benches/ceilings and restores distance without changing the actor position. First-person uses authored eye/seat anchors, a local mesh/head visibility policy and correct carried-tool/tray presentation. Camera collision and smoothing must not conceal geometry or erase a world obstruction.

Enhanced Input provides `TogglePerspective` separately from `ToggleManagementView`; both remap on keyboard/controller. Overhead preserves the prior hands-on state; management time-lapse suspends physical carrying/driving according to A05 and restores the prior view on safe return. Camera transitions never issue purchases, complete jobs, re-possess/duplicate an actor or change time rate. Store FOV/distance/input preferences in local user settings; save campaign position/vehicle/work state through its existing ownership path. A multiplayer client's view remains local and does not alter host authority.

Aim/focus may originate at the camera, but interaction acceptance checks avatar reach, target ownership and unobstructed actor-to-target geometry. A camera that can see around a corner does not confer remote reach. F03/F06 qualify on-foot view/collision/persistence, G05 qualifies carried objects and tasks, X04 integrates real seat/chase vehicle views, and P06/R03 exercise all available views on each native platform and controller.

