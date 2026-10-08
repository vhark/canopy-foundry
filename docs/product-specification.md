# Canopy Foundry — product specification

Decision baseline: 2026-10-07. These are implementation requirements, not claims about current software, partners, crop accuracy or achieved performance. The user delegated architecture and product choices. The existing one-room study is evidence for narrow BIM exchange only.

## 1. Identity and promise

**Grownetics: Canopy Foundry** is a growing-facility construction, operations and business game with switchable third-person and first-person gameplay. Build an operation, engineer its environment, grow valuable crops and expand into an automated horticultural business. The working title requires commercial name clearance before announcement.

Reference principles: Farming Simulator's equipment ownership and hands-on work; Satisfactory's spatial construction; Stationeers' commissioning and environmental dependencies; Oxygen Not Included's readable consequences; crop-business progression without making cannabis the whole product. Do not copy their assets, interfaces, names or balance tables.

The world is primary. Players walk through rooms, inspect plants, operate rolling benches and carts, connect equipment, replace consumables and see shipments leave. Management panels support that world. A task journal, training debrief or equipment record is not the main game loop.

## 2. Decided scope

| Topic | Decision |
|---|---|
| Engine | Unreal Engine 5.8.1 baseline; C++ for domain/integration, Blueprints for presentation and authored interactions |
| Simulation | Engine-independent C++20 library, single authoritative process, fixed domain clock, headless executable |
| Authoring | GrowBIM/OpenCEA semantic authority; external Blender/Bonsai/IfcOpenShell; game-specific derived packs |
| Platforms | Windows 11 x64, macOS 15+ Apple Silicon with M2 Pro reference tier, Linux x64 Ubuntu 24.04; Ubuntu 22.04 is additional qualification, not an untested launch promise |
| Launch | Paid offline single-player desktop game; no account required; no always-online economy |
| Multiplayer | Separate post-single-player gate for host-authoritative 2–4 player co-op; no deterministic peer lockstep |
| Controls | Mouse/keyboard and controller, rebindable; switchable third-person/first-person on foot and in vehicles plus overhead design/operations view; third-person default |
| Modes | Career, Sandbox and Training on the same simulation; Watch/replay is a tool, not the game's identity |
| Initial crop | Whole-head lettuce with propagation, transplant, growth, harvest, grading and dispatch |
| Contrasting crop | Greenhouse tomato with support/training labor, fruit cohorts and repeated harvests |
| Later indoor crop | Cannabis-specific phenology and drying/curing/packing; age-rating and jurisdiction review before distribution |
| Commercial model | Base game plus optional content expansions; no paid physics advantages, loot boxes or sponsor-dependent progression |
| Out of launch scope | Mobile/browser game client, consoles, VR, public MMO, mandatory AI/LLM workers, live facility control, continuous CFD |

Excluded launch platforms are not being promised as future deliveries. Controller support and platform-neutral simulation reduce later porting cost, but consoles require a separate funded approval/certification plan. Linux and Mac are release gates, not aspirational labels on a Windows-only build.

## 3. The player loop

**Build → commission → plant → operate → diagnose → harvest → sell → improve → expand.**

Three perspectives share one world and inventory:

1. **On the floor:** inspect crops and equipment; carry a tray; roll a bench within its rail travel; drive/push a cart; transplant, harvest, replace a filter and verify recovery. Interaction targets identify the actual world instance and compatible action. No arbitrary button animation that secretly completes a different task.
2. **Operations:** assign jobs, choose environmental recipes and batch timing, allocate shared capacities, inspect trends, buy supplies, negotiate/accept contracts and route processing work.
3. **Design:** preview placement, change room partitions and racks, route utilities, reserve maintenance/aisle clearances, compare power/water/climate capacity and commission changes. Reusable room templates retain connections and semantic meaning.

Perform useful work personally before purchasing labor or automation for it. Automation replaces repeated actions, not the need to make decisions. At industrial scale, workers and crop cohorts resolve through scheduled work and logistics rather than requiring every tray to be manually moved.

## 4. First complete playable scenario: First Shipment

An original 18 × 12 m, one-storey indoor shell has **216 m² footprint and gross floor area**. Allocation: nursery 18 m²; production rooms A/B 36 m² each; shared mechanical 18 m²; processing/dispatch 30 m²; support/wash 18 m²; circulation 60 m². These are authored level dimensions, not a compliant commercial design. Room height and equipment clearances are explicit in the source model.

Room A is commissioned by the player. Room B is an empty expansion opportunity, not free productive capacity. A small equipment catalog contains two original rolling-bench/rack configurations, two lighting choices, a ducted fan/filter assembly, a shared cooling/dehumidification plant, irrigation reservoir/pump/dosing unit, sensor kit, cart and processing station. All have usable visuals and interaction surfaces; a box envelope is allowed for import diagnostics, not a shippable hero asset.

Career opening:

1. Tour the owned shell; inspect equipment offers, utility limits and the first sales contract.
2. Place benches and lights, route/connect the room, inspect failures in commissioning, and correct them.
3. Plant one lettuce cohort in the nursery; personally perform its first transfer with a tray/cart.
4. Configure a working recipe and time-lapse through development. Crop appearance, pump/fan audio, lights and gauges reflect domain state.
5. A seeded filter obstruction reduces available airflow and changes moisture/temperature response. Inspect trend versus sensor reading and actual filter condition. Clean/replace the filter, shed lighting or revise schedules; observe different cost/production consequences.
6. Harvest, grade, process and dispatch actual inventory against the contract. Cash changes only when the accepted transaction and shipment complete.
7. Compare the cost of commissioning room B with improving the shared plant or automating transfers. Start the next cycle using the resulting state.

Economic values, growth coefficients, weather and failures begin as visibly labeled **illustrative game balance**. They are not disguised vendor performance or agronomic recommendations. The starter contract and reserve cash must allow recovery from the authored failure without forcing a restart. Bankruptcy offers a transparent rescue/downsizing path, not an invisible cash injection.

### First Shipment acceptance

- A new player can complete commissioning, a crop cycle and sale without opening technical IDs or following an external tutorial.
- Equipment placement, operation and one failure materially affect crop history, energy/water consumption, tasks and cash.
- At least two viable corrective strategies exist; neither is always dominant across the two contract/price presets.
- The completed sale consumes inventory exactly once; saving/reloading before dispatch cannot duplicate inventory or money.
- A second crop cycle and one expansion/efficiency decision are playable, not a cutscene or an end-of-demo label.
- In a five-person formative playtest, at least four finish the loop and correctly explain a climate–equipment–crop–cost consequence without coaching. Record observations rather than calling this proof of broad educational effectiveness.

## 5. Facility families and area accounting

Every facility records site footprint, building footprint, gross floor area, production-room area, support/circulation/mechanical area, and cultivated canopy area. Units are SI internally; ft²/imperial display is reversible. Building storeys are explicit containment objects; cultivation tiers are equipment/production surfaces inside rooms. Gross floor area is the sum of storey floor polygons, never rack tiers multiplied by footprint.

| Scenario | Definition | Gameplay distinction |
|---|---|---|
| First Shipment | 216 m², one storey, two production rooms | Shared plant, hands-on work, first sale and expansion |
| Greenhouse contrast | Original modular tomato greenhouse, 8 initial compartments | Solar gain, outdoor weather, screens, vents, heating, supplemental light, repeated picking and packing |
| Flagship greenhouse | **1,000,000 ft² (92,903.04 m²) total ground-level building footprint**, including support space; no automatic claim this is canopy | 64 authored production compartments plus support/utility/logistics zones; staged construction and central-utility bottlenecks |
| Flagship indoor | **60,000 ft² (5,574.1824 m²) TOTAL gross floor area**, three equal 20,000 ft² (1,858.0608 m²) storeys | 12 production rooms per storey plus nursery/processing/mechanical/circulation; lifts, queues, separation and shared equipment |

This explicitly chooses total area—not a 60,000 ft² footprint—for the indoor flagship. A 180,000 ft² alternative can be authored as a separate scenario; it is not silently substituted. For both flagships, actual canopy is calculated from accepted usable bed/tier polygons and layouts, not a marketing area multiplier.

The greenhouse has envelope transmission, solar/weather exposure, ventilation and screens as genuine modeled controls. It cannot be the indoor ruleset with glass materials. Storey/lift bottlenecks in the indoor scenario must alter construction and production strategy.

## 6. Coupled systems

- **Climate:** zone sensible/latent energy, humidity ratio, CO₂ mass, light exposure, envelope/weather exchange and bounded inter-zone transport.
- **Crop:** cohorts, stage/age, biomass proxy, leaf/canopy proxy, water use, nutrient availability, stress history, quality and yield; crop packs define rates and response domains.
- **Equipment:** capacity/operating maps, state, controls, wear/maintenance, topology and failures; pressure/flow and moisture-removal curves retain their ambient conditions.
- **Water/nutrients:** tanks, pumped delivery, return/drain, consumption, dosing and simplified solution bookkeeping. No claim of chemically complete nutrient speciation.
- **Operations:** tasks, skill/availability, inventory reservations, paths, door/lift/cart capacity, processing queues and schedules.
- **Business:** integer-minor-unit cash, capital purchases, wages, tariffs, water/consumables, contracts, quality grades, inventory aging and finance.

Increasing lighting must affect useful light, electrical load, heat balance, crop development/water use and cost through those systems. Display why a limit was reached and which upstream/downstream entities are involved. Do not implement disconnected upgrade bonuses.

Weather records and recorded telemetry are inputs with source/rights metadata. A replayed sensor trace is not a counterfactual climate model. Simulated sensor readings include calibrated error/failure policies separately from the underlying state.

## 7. Realistic equipment and vendor participation

Use SKU-specific, rights-cleared masters where available. Otherwise create detailed original generic equipment with credible dimensions/mechanics and explicitly illustrative operating profiles. Pursue rolling benches, mobile vertical racks, ducted fans, filtration, HVAC/dehumidification, luminaires, fertigation, sensors, pumps, tanks, carts/lifts and processing equipment.

Four fidelity axes are independent: geometry; motion/interaction; operating data; model validity. A beautifully rendered manufacturer mesh does not establish capacity, safe loads or crop performance. Published BIM LOD and game rendering LOD are different classifications.

Sponsors can fund licensed equipment packs, approved branding and clearly marked showcase scenarios. They cannot secretly improve performance, alter scoring or force exclusive equipment progression. A real product can legitimately differ when supported by approved data. Sponsorship status is never a solver input.

The [vendor program](vendor-program.md) and [BIM/asset pipeline](bim-and-asset-pipeline.md) define rights, acquisition, conversion, approval, removal and save-compatibility gates.

## 8. Modes and progression

**Career:** fictional company, contracts, equipment ownership, staged expansion, staff/automation unlocks and recoverable setbacks. Progress follows operating capability rather than arbitrary experience points for clicking controls.

**Sandbox:** choose facility, crop packs, money, weather, failure seed and time policy. Experimental changes create a labeled branch; presets can be shared without copying restricted vendor sources.

**Training:** bounded diagnosis objectives, reproducible initial state/seed, recorded player actions and causal debrief. Cases: stale sensor; undersized shared utility; simultaneous harvest processing queue; failed pump/fan; expansion versus efficiency; recipe transfer to a different climate. No quiz interrupts the main career loop. Alternative-action comparisons rerun a model branch and disclose uncertainty; they are not claims about what a real farm would have done.

Replay supports pause, timeline inspection and a new scenario branch. Inspecting the past never mutates the current campaign. Skip-to-event and accelerated time preserve failures, contracts, worker work and equipment limits; there is no invisible background completion.

## 9. Presentation and accessibility

Third-person character/work animation, first-person sight lines, crop development and equipment motion convey state before a table does. Inspection has three depths: short actionable status; diagnostic overlay; sourced trends/technical record. Technical identifiers are available for engineering/debugging but never default player labels.

Use Grownetics visual identity for instruments/menus while the world uses believable metal, glazing, hoses, rails, ducts, machinery and crops. Branded assets follow their own approved brand treatment. Audio distinguishes pumps/fans, load and faults without falsely presenting sound as a measurement.

Controller parity; scalable text/UI at 1080p through 4K; subtitles; color-independent alarms; rebindable controls; adjustable FOV; toggle/hold options; head-bob and motion-blur controls; no mandatory rapid input. Localize using message keys and locale-aware unit/currency formatting. English ships first; no localized text is baked into core simulation or textures where avoidable.

Camera interaction follows GTA/Farming Simulator's viewpoint flexibility, not their assets or unrelated combat/crime mechanics: a single rebindable action switches third-person/first-person in the current on-foot or vehicle context, while overhead management is a separate action. Persist on-foot and vehicle preferences separately. Third-person provides orbit/chase distance and obstruction handling; first-person uses the actual eye/seat position; supported vehicles offer operator/cab and chase views. Adjustable per-view FOV, sensitivity/inversion and reduced-motion settings apply. Return from overhead/time-lapse to the previous hands-on view safely. Switching never respawns the actor, teleports it, duplicates carried inventory, resets work or changes speed/control authority; avatar reach/line-of-sight remains authoritative even when a third-person camera sees around a corner. All actions must remain usable in both hands-on views with controller and mouse/keyboard.

## 10. Guardrails

No public asset redistribution without rights. No claim of vendor partnership until a signed agreement. No confidential real farm model in test fixtures. No bundled Blender/Bonsai engine dependency. No live equipment writes. No requirement for a cloud AI service or real-time LLM staff. No certification claims for a simulation, layout or learning outcome without independent stated-use evidence.

All performance numbers are targets until the [qualification protocol](performance-and-qualification.md) records the build, machine, settings and result. Release can be blocked by missing platform tests, rights, accuracy claims or playable-loop evidence; those are not silently waived to hit a date.
