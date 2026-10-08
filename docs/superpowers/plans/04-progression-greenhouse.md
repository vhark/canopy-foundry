# Greenhouse, Expansion and Flagship Content Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the proven crop-to-sale loop into genuinely different greenhouse strategies, scalable business operations and the two explicitly defined flagship facilities.

**Architecture:** New facility/crop/equipment/scenario packs exercise the same semantic construction, coupled balances and logistics. Add domain capabilities where the new production process actually differs; do not fork a second simulation per facility type.

**Tech Stack:** CanopySim C++20, Unreal presentation/streaming, GrowBIM creator pipeline, versioned model/content packs and native/headless qualification.

---

Dependencies: G08's playable-loop gate and S08. P01/P04 scale workloads start earlier in parallel; do not defer all performance risk until X07. Each command below is a future check.

Base release includes X01–X05 and X07. X06 is the separately cleared cannabis crop/processing pack, not a prerequisite for X07's lettuce-based indoor campaign. Numbering groups content domains rather than imposing a false dependency.

## X01 — Weather-driven greenhouse envelope and controls

**Create:** `core/src/greenhouse/{solar,glazing,screens,ventilation,heating}.cpp`, `contracts/weather.schema.json`, `content/definitions/weather/synthetic-temperate-year.json`, `content/definitions/equipment/greenhouse-envelope.json`, `core/tests/greenhouse_test.cpp`, `tests/qualification/greenhouse_climate.py`.

- [ ] Define latitude/orientation/time basis and weather channels with units, provenance, missing-data and rights rules. Begin with an explicitly synthetic temperate series; an imported measured year retains its source dates and gap flags.
- [ ] Implement solar transmission/shading and envelope exchanges against S02 balances. Screens alter light/heat transport; vents couple inside and outside temperature/moisture/CO₂; heating and supplemental lighting consume metered resources.
- [ ] Author the contrast greenhouse as an original 60 × 40 m, 2,400 m² ground-level building: eight 12 × 20 m production compartments total 1,920 m², with 480 m² explicitly assigned to support/processing/mechanical/circulation. Connect its plant and logistics through the same B01/B02 source pipeline. It is smaller than the separately dimensioned X07 flagship, not a changed interpretation of the million-ft² footprint.
- [ ] Implement greenhouse recipes with weather-responsive screen/vent/heating limits and priority conflicts. Ventilation that cools may lose CO₂ or admit moisture; do not give every action only a positive effect.
- [ ] Add two cases with identical plant/indoor equipment but different outdoor weather: changing vent position produces different direction/magnitude of exchange; closed screens reduce transmitted light and alter thermal demand.
- [ ] Register `greenhouse-weather`; run hot/sunny, cool/cloudy and humid-night periods with the same model pack. Explain weather-driven differences and conserve mass/energy.
- [ ] Commit model-domain limitations; no continuous CFD or claimed site-calibrated weather without evidence.

**Check:** `python scripts/qualify.py --case greenhouse-weather`. Expected: the greenhouse cannot be reproduced by changing only the indoor room's wall material.

## X02 — Tomato cohorts, repeated harvest and packing

**Create:** `core/src/crop/{fruit_cohorts,repeat_harvest}.cpp`, `content/definitions/crops/tomato-illustrative-v1.json`, `content/definitions/tasks/tomato-production.json`, `game/Content/Crops/Tomato/`, `tests/qualification/tomato_cycle.py`.

- [ ] Add fruit cohorts/age/grade separate from standing crop biomass, with pack-defined development and source/validity labels. Repeated picking removes mature fruit, not the whole crop.
- [ ] Define support/training/pruning/picking tasks and tool/station needs. Missed labor affects operational outcomes through explicitly labeled game rules; no hidden arbitrary yield bonus for a cosmetic trellis.
- [ ] Author realistic original tomato development/support assets and representative cluster LODs, including picked versus unpicked fruit states.
- [ ] Implement grading/packing throughput and multiple dispatches from one standing crop. Packing queues can cause delay/quality loss and contract penalties.
- [ ] Test repeated harvest, no double picking, standing-crop continuity, labor shortage and processing-capacity bottlenecks.
- [ ] Register `tomato-repeat-harvest`; play two successive pick/pack/dispatch events in the greenhouse and compare headless stock/ledger totals. Commit both visual and domain evidence.

## X03 — Reusable layouts and staged expansion

**Create:** `contracts/room-template.schema.json`, `core/src/construction/{templates,staging,utility_expansion}.cpp`, `game/Source/CanopyFoundry/Construction/TemplateBrowser.{h,cpp}`, `content/definitions/templates/`, `tests/qualification/expansion.py`.

- [ ] Save reusable room/equipment templates with relative transforms, topology and neutral type IDs. Instantiation creates new world IDs and role bindings; it cannot reuse the original room's crop/worker/asset identities.
- [ ] Add staged orders, deliveries, construction time, cost and outages. A drawn expansion is not commissioned capacity; planned versus funded versus installed versus operational states are distinct.
- [ ] Compare new production area with shared-utility headroom, processing capacity and logistics before purchase. Show estimated limits and model uncertainty, not a guaranteed ROI.
- [ ] Implement central plant upgrade, modular compartment addition and indoor floor allocation through B06 transactions, preserving accepted-source provenance and replay.
- [ ] Test copying a populated-room template without copying crops/inventory, cancelling before purchase, partial construction/outage recovery and unexpected shared-capacity shortfall.
- [ ] Register `expansion-versus-retrofit`: the player can choose room expansion or efficiency/capacity investment and run the next cycle. Commit differences in production, labor, resources and cash.

## X04 — Staff, vehicles and scalable automation

**Create:** `core/src/operations/{staffing,maintenance_schedule,automation_rules,fleet}.cpp`, `content/definitions/equipment/logistics.json`, `game/Source/CanopyFoundry/Work/{FleetController,AutomationEditor}.{h,cpp}`, `tests/qualification/industrial_work.py`.

- [ ] Expand staffing into roles/skills/shifts, onboarding costs, fatigue/availability game rules and task priorities. Avoid simulating every thought; jobs/reservations remain the authority.
- [ ] Add drivable/pushable carts and a licensed/original lift/forklift interaction model with the same transport/load/clearance rules as automated fleet work. State clearly that this is not an operator safety certification simulator.
- [ ] Extend F03's camera component for actual vehicle seat/operator and third-person chase views, shared driving input and context-specific saved preference. Exercise enter/exit, drive with load, switch views while moving, obstruction recovery and save/reload without resetting throttle/load/ownership. Seat view follows the real operator anchor even when a machine has no enclosed cab. Never build a second vehicle simulation for a different camera.
- [ ] Add maintenance scheduling, spares, contractor callouts and equipment downtime. Repairs consume time/parts/funds; deferred maintenance affects a seeded failure model, not secretly random punishment.
- [ ] Implement declarative automation rules for transfers, harvest queues and condition-based maintenance. Bound rule execution and detect loops/conflicting control actions; avoid a general code-execution language at this stage.
- [ ] Test staff shortage, simultaneous harvest queues, failed lift recovery, automation cancellation and camera-independent completion times.
- [ ] Register `industrial-workflow`; run with 1,000 logical workers while limiting detailed visual workers according to P02. Commit progression costs and measured runtime evidence.

## X05 — Career economy and production-chain progression

**Create:** `core/src/business/{market,credit,depreciation,forecast}.cpp`, `content/definitions/career/{contracts,unlocks,recovery,prices}.json`, `game/Source/CanopyFoundry/Career/CompanyOverview.{h,cpp}`, `tests/qualification/career_balance.py`.

- [ ] Add multiple contracts, recurring buyers, inventory aging, supplier lead times, maintenance costs, tariffs, explicit loans/interest and asset resale. Prices are fictional balance unless separately sourced; no promise of real commercial ROI.
- [ ] Define unlocks through demonstrated operational capability and purchased infrastructure. No pay-to-win sponsor dependency or compulsory repetitive personal action at industrial scale.
- [ ] Forecast using a labeled simulation branch with declared assumptions, not a static profit multiplier. Keep the live campaign unchanged; expose capacity and price uncertainty.
- [ ] Add recovery options: sell/idle equipment, defer an expansion, refinance under stated terms or accept smaller orders. Bankruptcy and restart remain explicit outcomes.
- [ ] Run normal, high-energy-price, low-price and capacity-stressed scenario seeds across repeated cycles. Verify no infinite purchase/refund/credit arbitrage or forced sponsor-only optimal path.
- [ ] Register `career-balance`; use actual playtest choices and ledger traces to tune values. Commit balance changes as pack versions so older replays retain their old rules.

## X06 — Cannabis and downstream processing pack

**Create:** `content/definitions/crops/cannabis-illustrative-v1.json`, `content/definitions/tasks/cannabis-processing.json`, `core/src/processing/{drying,curing,grading}.cpp`, `game/Content/Crops/Cannabis/`, `tests/qualification/indoor_batch_processing.py`.

- [ ] Define stage/batch movement and downstream processing as distinct crop/model states with capacity, time, mass/quality and environmental history. Avoid copying lettuce's whole-head sale into a renamed crop.
- [ ] Allocate real nursery/production/drying/curing/packing rooms and shared utilities. Simultaneous harvests can saturate processing and moisture-removal capacity; moving a batch preserves provenance and stress history.
- [ ] Author rights-cleared visual growth/harvest/processing states and equipment. Do not label illustrative environmental response or product quality as validated horticultural or medical guidance.
- [ ] Implement dry/processing inventory mass accounting with explicit modeled loss; no duplication by moving batches among stages.
- [ ] Conduct age-rating, store-policy, jurisdiction/marketing and crop-content legal review before distribution; this is a lawful facility-operations pack, not an illegal-evasion mechanic.
- [ ] Register `indoor-batch-processing`; complete a production-to-processing-to-sale cycle with two competing batches and an equipment fault. Commit model limits and exact inventory/utility outcomes.

## X07 — Flagship facility campaigns

**Create:** `content/definitions/scenarios/{flagship-greenhouse,flagship-indoor}.json`, `apps/canopy-author/canopy_author/facility_recipes/{flagship_greenhouse,flagship_indoor}.py`, `game/Content/Maps/Flagships/`, `tests/qualification/flagship_campaigns.py`.

- [ ] Author the greenhouse as an 800 × 1,250 ft ground-level building envelope (1,000,000 ft²), with 64 initial full-build production compartments totaling 768,000 ft² and 232,000 ft² explicitly allocated to support/mechanical/circulation/processing. The career starts with only a funded subset operating; the benchmark loads the full build-out.
- [ ] Author the indoor facility with a 100 × 200 ft footprint across three storeys: 60,000 ft² total. Each storey has twelve 1,000 ft² production rooms and 8,000 ft² of explicit nursery/support/mechanical/processing/circulation allocation. Assign different support uses by floor and include real vertical circulation/queues. Tiered canopy is calculated separately from actual bench surfaces.
- [ ] Use the base lettuce/multi-tier workflow for the indoor launch campaign. X06 later supplies dedicated cannabis production/processing layouts and contracts without silently replacing the base crop or bypassing its own rating/model gates.
- [ ] Validate all area sums, openings, room containment, storey/shaft paths, utility capacity/connection semantics, identity maps and source/license provenance. These are original game facilities, not engineering-approved commercial projects.
- [ ] Dress the facilities using admitted modular equipment and plant representations rather than importing an enormous unoptimized vendor model as one Actor. Connect all installed systems to real domain networks.
- [ ] Run P01–P06 benchmarks and complete production/dispatch/failure/expansion gameplay in each flagship. Capture multiple storeys, distant/near geometry and high-speed operations; compare domain results independent of viewing position.
- [ ] Register `flagship-campaigns`; block release if either facility is merely an empty render, if Linux/Mac cannot play it, or if the declared area/canopy accounting differs. Commit qualified campaign/model/render pack versions together.

**Final checks:** `python scripts/qualify.py --case flagship-campaigns` and `python scripts/qualify.py --case career-balance`. Passing the generated synthetic scale workload alone does not qualify these actual authored facilities.
