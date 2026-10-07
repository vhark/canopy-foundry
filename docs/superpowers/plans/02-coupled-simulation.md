# Coupled Facility Simulation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make crop outcomes, shared equipment limits, environmental state, labor and cash arise from one reproducible causal simulation.

**Architecture:** Zone/cohort/network domain arrays in the C++ core; typed commands and conserved resource flows; model parameters are versioned packs with explicit validity labels. Rendering consumes state and cannot create yield, inventory or money.

**Tech Stack:** C++20, FlatBuffers, CTest/Catch2, headless scenario runner, JSON authoring definitions, Python qualification harness.

---

All commands below are future implementation checks. Depends on F02/F05 and B02/B04 contracts. Start with original illustrative equipment/crop definitions; neither numerical realism nor manufacturer certification is implied by a fitted-looking curve.

## S01 — Utility topology and equipment operating points

**Create:** `core/include/canopy/networks.hpp`, `core/src/networks/{graph,air,electrical,water,capacity}.cpp`, `core/tests/networks_test.cpp`, `content/definitions/equipment/generic-inline-fan.json`, `tests/qualification/networks.py`. Consume B04's `equipment_ports.hpp` and equipment contracts rather than defining a second port/units model.

- [ ] Build connected components and capacity/control behavior on B04's typed ports/media/units and operating-domain contracts. Electrical, air, water and drain networks retain distinct compatibility rules; no untyped universal connection edge.
- [ ] Add tests for incompatible ports, disconnected supply, wrong units, loop/valve closure, two rooms competing for one capacity, and an equipment replacement retaining its role but not its retired instance ID.
- [ ] Implement deterministic shared-capacity allocation with declared priorities and unmet-demand output. Use a bounded monotone operating-point solve for air networks; reject missing/out-of-domain curves and convergence failures visibly.
- [ ] Recompute connected components and factorization/cache only on topology changes; update operating points on controls/demand changes. Preallocate edge/work buffers. Add stage counters to prove no per-equipment UObject/Tick is needed.
- [ ] Meter delivered/consumed quantities, not requested setpoints. Preserve explicit outdoor versus indoor condenser heat rejection and fan/pump electric consumption.
- [ ] Register `network-capacity` and run headless topology changes/faults; commit tests, definitions and residual report.

Use this deliberately synthetic fixture, never attribute it to a vendor:

```text
fan: Δp = 400 - 100*q² Pa, q in m³/s, domain 0..2
system: Δp = 100*q² Pa
expected operating point: q = sqrt(2), Δp = 200 Pa
shared plant: 10 kW delivered maximum
room A demand = 6 kW, room B demand = 6 kW, equal priority
expected allocation: 5 kW each; total unmet = 2 kW
```

**Checks:** `ctest --test-dir .build/core -R Network --output-on-failure`; `python scripts/qualify.py --case network-capacity`. Expected: mass/flow residual within declared tolerance, no double-counted plant capacity, disconnected rooms lose service.

## S02 — Energy, moisture and gas balances

**Create:** `core/include/canopy/climate.hpp`, `core/src/climate/{psychrometrics,zone_balance,envelope,exchange,solver}.cpp`, `core/tests/{climate,conservation}_test.cpp`, `content/definitions/models/well-mixed-zone-v1.json`, `tests/qualification/climate.py`.

- [ ] Define state units: kg dry air/water/CO₂, J energy, K temperature, W power, kg/s mass flow, m³ volume. Derived percent RH/ppm are presentation conversions with documented pressure assumptions, not alternate authoritative stores.
- [ ] Implement closed-box known-input tests before coupled crop work. For a synthetic dry zone with effective capacity 1,000,000 J/K, 1,000 W for 60 s and no losses, ΔT must be 0.06 K. For a mass store, adding 0.001 kg/s for 60 s yields 0.06 kg; a metered drain subtracts exactly its transferred mass.
- [ ] Implement semi-implicit envelope/thermal exchange with bounded deterministic subdivision. Use enthalpy-consistent moisture exchange and condensate routing; separate source input, internal transfer, storage and exported heat.
- [ ] Implement an equipment interface that returns electric power, heat rejection, air/water flow and removed/added moisture/CO₂. A standalone dehumidifier returns appropriate condenser heat to its served environment; external rejection leaves that zone.
- [ ] Add invariants: positive masses/volumes; bounded model domain; no double-counted latent heat; symmetric inter-zone exchange; closed-system energy and mass residual reports. Reject invalid source packs before stepping. Persistent nonconvergence pauses with a specific diagnostic rather than clamping into a plausible state.
- [ ] Register `climate-conservation`; compare whole/partitioned advancement and a hot/cold load transition. Commit numeric tolerances and their rationale with tests.

**Checks:** `ctest --test-dir .build/core -R 'Climate|Conservation' --output-on-failure`; `python scripts/qualify.py --case climate-conservation`. Analytic fixture tolerance: 1e-8 relative or 1e-10 absolute in normalized test units. Coupled integration tolerance is separately declared and justified; do not apply an arbitrary sensor-level tolerance to hide numerical leakage.

## S03 — Crop cohorts, development and exposure history

**Create:** `core/include/canopy/crop.hpp`, `core/src/crop/{cohort,development,uptake,stress,harvest}.cpp`, `contracts/crop-model.schema.json`, `content/definitions/crops/lettuce-illustrative-v1.json`, `core/tests/crop_test.cpp`, `tests/qualification/crop.py`.

- [ ] Define cohort count/area/stage, development, biomass proxy, root water state, nutrient availability, accumulated light/temperature exposure and stress history. Record mortality/losses as explicit inventory/cohort transitions.
- [ ] Define a bounded illustrative response model: pack-specified stage rates; light response saturates; temperature/CO₂/water response multiply within a stated operating domain; stress integrates exposure duration and recovers according to pack coefficients. Put every coefficient and unit in the model pack with source/illustrative status.
- [ ] Integrate exposure over climate steps, then crop updates over 60-second boundaries. A one-hour stress event must not be replaced by only its end-of-hour reading. Splitting a cohort partitions mass/count/history consistently rather than resetting stress.
- [ ] Implement seed/transplant/harvest with area/capacity and task preconditions. Harvest creates actual lot quantity/grade and consumes the harvested cohort once. No renderer-visible leaf count drives yield.
- [ ] Test constant-environment convergence, zero-water limitation, saturated-light behavior, different stress histories ending at the same conditions, cohort split/merge conservation and duplicate harvest receipt behavior.
- [ ] Register `crop-history`; headlessly run an entire lettuce cycle and compare an identical final climate with different prior stress. Commit the illustrative label and response-domain display metadata.

Concrete acceptance cases:

```text
same final T/RH, different 6-hour water deficit -> different accumulated stress/grade
split 100 plants into 40 + 60 -> counts/biomass/stock conserved
harvest command retried twice -> one inventory lot, one cohort removal
double requested light above model saturation -> not double biomass
unknown/out-of-domain coefficient -> model error, not a default yield bonus
```

## S04 — Coupling, metering and recipes

**Create:** `core/src/systems/{coupled_step,recipes,meters}.cpp`, `core/include/canopy/recipe.hpp`, `content/definitions/recipes/lettuce-starter.json`, `core/tests/coupling_test.cpp`, `tests/qualification/coupling.py`.

- [ ] Connect crop vapor/water/CO₂ exchange to S02, and delivered equipment power/heat/flow to S01. Define and test architecture A05's exact timestamp ordering and bounded feedback convergence.
- [ ] Implement schedule/recipe changes as authoritative commands with monotonic revisions, setpoint limits and source/validity metadata. Delivered conditions remain distinct from desired conditions.
- [ ] Accumulate resource meters at their native integration boundaries. Changing time speed or render FPS cannot change cumulative kWh, water use or crop outcome.
- [ ] Test a 1,000 W synthetic lamp at full output for 3,600 s: 1 kWh electrical use before any other equipment. Add the shared plant and confirm its additional draw appears separately; do not hardcode a fixed bill for a lighting upgrade.
- [ ] Run the causal pair: more light changes exposure/electricity/heat; subsequent crop vapor and equipment loading change; limited shared capacity creates unmet demand and possibly stress. The magnitude follows the pack, not an assumed universally monotone yield increase.
- [ ] Register `lighting-causal-chain`; compare 1× and target-based 1440× advancement with the same commands. Commit meter reconciliation and explanation events.

**Check:** `python scripts/qualify.py --case lighting-causal-chain`. Expected: complete trace from control change to meters, environment, crop and costs; no double counting. A more expensive light can legitimately produce worse profit under an undersized shared plant.

## S05 — Tasks, workers, movement and reservations

**Create:** `core/include/canopy/operations.hpp`, `core/src/operations/{tasks,workers,reservations,routes,lifts,processing}.cpp`, `core/tests/{operations,routing}_test.cpp`, `tests/qualification/operations.py`.

- [ ] Implement task states offered/reserved/travel/active/blocked/completed/cancelled with skill, shift, equipment, inventory and station requirements. The player and hired workers reserve through the same authority.
- [ ] Build the initial access graph from B02's semantic layout, including doors/aisles, cart class, lifts and traversal durations. Define the validation/invalidation API that B06 will later call for atomic layout changes; S05 does not depend on B06 implementation. Do not reuse a straight one-room path as a general pathfinder.
- [ ] Implement atomic multi-resource reservations with a stable ordering, bounded retries and visible blocking cause. Cancellation releases reservations; interrupted work retains defined progress and consumes only posted inputs.
- [ ] Integrate near animation with logical task progress; far resolution uses identical routes/work durations. Camera position cannot complete work early. A moved rolling bench invalidates affected paths and forces safe replanning.
- [ ] Test competing harvest/transfer jobs, unavailable lift, blocked aisle, worker shift change and one player/worker racing to consume the same tray. Processing queues limit shipment timing.
- [ ] Register `logistics-reservations`; run a two-storey queue case with the same work in near and far presentation modes. Commit operation explanations and queue metrics.

**Expected observable result:** two tasks needing one cart cannot both own it; a failed lift delays upstairs processing; widening capacity improves queue time without changing crops' biological age or creating inventory.

## S06 — Inventory, contracts and business ledger

**Create:** `core/include/canopy/business.hpp`, `core/src/business/{inventory,ledger,tariffs,contracts,purchasing,finance}.cpp`, `contracts/economy.schema.json`, `content/definitions/economy/career-start.json`, `core/tests/{inventory,business}_test.cpp`, `tests/qualification/business.py`.

- [ ] Define integer minor-unit money, quantity+unit stock, batch/grade/age/location and encumbrances. All cash postings have balanced entries and an origin command/event; overflows and mixed units fail.
- [ ] Implement equipment purchase/delivery, consumables, wages, metered tariffs, contract acceptance and penalties. Purchases create game-owned equipment instances, not reported real-world installed assets.
- [ ] Implement dispatch as one transaction: validate grade/deadline/reserved stock, consume lot quantity, post revenue/fees and close/reduce the contract. A saved in-flight dispatch replays exactly once.
- [ ] Test partial shipments, spoiled stock, wrong grade, competing reservations, meter-tariff boundary changes, insufficient funds and command retry/conflict.
- [ ] Use this illustrative bookkeeping fixture: start 100,000 minor units; purchase 12,500; ship 20 accepted units at 300 each. Final cash before other costs = 93,500. Dispatch retry leaves it unchanged; rejected quantity produces no sale.
- [ ] Register `sale-exactly-once`; run an actual headless crop-to-stock-to-dispatch sequence. Commit fiction-versus-real-price labeling and ledger views.

**Check:** `python scripts/qualify.py --case sale-exactly-once`. Expected: stock and cash reconcile after save/reload/failure; no free purchase or double payment path.

## S07 — Failures, sensors and model validity

**Create:** `core/src/scenario/{failures,sensors,validity,weather_input}.cpp`, `contracts/{failure,validity}.schema.json`, `content/definitions/failures/filter-obstruction.json`, `core/tests/failures_test.cpp`, `tests/qualification/failures.py`.

- [ ] Separate underlying state from measurement value/source time/publication time, sensor error, stale reading and missing data. Display last-known observations without fabricating live freshness.
- [ ] Add deterministic per-instance failure streams, authored triggers and explicit repair/reset actions. Filter obstruction changes pressure loss; a pump fault changes delivery; a sensor fault changes observation, not the underlying temperature by magic.
- [ ] Define validity labels `illustrative`, `calibrated`, `validated_for_use`, each with version, source records, fitting data, independent evaluation where required, ranges and excluded uses. The UI cannot upgrade a label based on visual quality or sponsorship.
- [ ] Define weather input units, source time, repeat/fictional-year policy, rights and missing-data handling. Recorded values and synthesized gap fill are distinguishable; no hidden extrapolation into a purported measured year.
- [ ] Test persistent stale measurement during a real climate change; repaired sensor restores observation while the plant remains stressed; replay draws the same fault stream without depending on unrelated cohort count.
- [ ] Register `sensor-versus-state`; commit diagnosis explanations and provenance records. No autonomous real-equipment write endpoint is introduced.

## S08 — Integrated small-facility proof and calibration harness

**Create:** `content/definitions/scenarios/first-shipment.json`, `tests/scenarios/{first-shipment,shared-plant,filter-recovery}.json`, `tests/qualification/integrated_sim.py`, `apps/canopy-author/canopy_author/model_validation.py`, `contracts/model-validation-report.schema.json`.

- [ ] Assemble the product specification's exact small facility, one crop, shared plant, stock, staffing and contract through valid definitions; no alternate hardcoded path for the demonstration.
- [ ] Provide explicit command streams for working baseline, filter repair, temporary load shedding and premature room-B expansion. All follow normal command validation and accounting.
- [ ] Run full/partitioned/saved-resumed trajectories and compare exact discrete outcomes on the same build, numeric state within cross-platform tolerances, and conservation/meter/ledger residuals.
- [ ] Implement fitting/evaluation report ingestion that separates training samples from held-out validation. Unknown measurements remain unknown. Model acceptance requires a recorded reviewer and domain/tolerance; it does not grant product-wide engineering approval.
- [ ] Compare response pairs: repaired airflow restores capacity at repair cost; load shedding changes light exposure and cost; premature expansion exceeds the same shared capacity and has visible consequences. Do not tune a test to guarantee an unsupported universal winner.
- [ ] Register `first-shipment-headless`; record the model pack hash, seed, commands, outcomes and remaining validity limits. This passes into G01–G08 for hands-on playable proof, not a declaration that a headless simulation is already a game.

**Checks:** `python scripts/qualify.py --case first-shipment-headless`; `ctest --test-dir .build/core --output-on-failure`. Expected: one completed sale, reconciled accounts, meaningful recovery choices and a runnable second cycle, with all initial models honestly illustrative.
