# First Shipment Playable Game Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A player builds and commissions a small operation, grows a crop, diagnoses a problem, completes a sale and chooses the next investment through actual gameplay.

**Architecture:** Unreal presents the C++ domain state and submits the same validated commands used by the headless runner. Spatial work, workers, inventory and business share one authority; no special scripted-success path.

**Tech Stack:** Unreal C++/Blueprint presentation, Enhanced Input, CommonUI/UMG, authored original/approved equipment and crop art, CanopyRuntime, native functional qualification.

---

Dependencies: F03/F06, B03/B04/B06, S01–S08. Art and interaction prototypes can run alongside domain work, but G08 cannot pass with mocked climate, a fake sale or placeholder hero equipment. All checks below describe future implementation.

## G01 — A facility the player owns and can inspect

**Create:** `game/Content/Maps/FirstShipment.umap`, `game/Source/CanopyFoundry/Interaction/{Inspectable,InteractionResolver}.{h,cpp}`, `game/Source/CanopyFoundry/UI/{InspectionViewModel,FacilityHUD}.{h,cpp}`, `game/Content/UI/Inspection/`, `tests/qualification/first_person.py`.

- [ ] Assemble the exact 216 m² facility with real doors, benches, rails, utility connections, service access and processing. Preserve semantic selection in third-person, first-person and overhead. Render actual character locomotion/work and carried items in third-person; first-person local visibility must not hide the held tray/tool or change its ownership.
- [ ] Implement contextual focus and reach rules: identify nearby equipment, cohort, tray or station; show compatible actions with reasons for unavailable ones. Prevent interaction through a wall/locked service panel solely because an ID is known.
- [ ] Display short name, operating state and one actionable issue first. Expose source/model validity and technical IDs only in deeper inspection. Show owned versus ordered versus preview equipment distinctly.
- [ ] Add believable scale cues, lighting, footsteps, equipment audio and interactable doors. Reserve collision for relevant geometry; no collision on every bolt or visible leaf.
- [ ] Register `floor-inspection`: walk/operate/inspect the same bench in third-person, first-person and overhead with both input devices; exercise view changes beside walls and service panels, safe camera collision and avatar-bound reach. Capture 1080p/4K UI and body/tool readability.
- [ ] Commit the playable map and approved assets; record first-person camera/art/interaction issues before increasing facility size.

**Check:** `python scripts/qualify.py --case floor-inspection`. Expected: the owned room is navigable and understandable without the old shift dashboard or UUID navigation.

## G02 — Construction, previews and usable layouts

**Create:** `game/Source/CanopyFoundry/Construction/{BuildController,PlacementPreview,UtilityRouteTool,LayoutTransactionView}.{h,cpp}`, `game/Content/UI/Construction/`, `tests/qualification/construction_play.py`.

- [ ] Implement purchase/stock selection, snap/rotation, continuous placement preview, clearance overlays and cost summary over B06 transactions. Preview cannot create inventory or spend funds.
- [ ] Show why a placement fails: service clearance, rail sweep, blocked access, structural storey bounds, unsupported connector or insufficient utility capacity. Unsupported engineering checks must be labeled outside the model, not passed automatically.
- [ ] Implement route drawing for supported ducts/pipes/cables with visible connectors and bend/length parameters; preserve actual edge IDs/units and design costs. Shared plant connection is not duplicated by drawing a second room branch.
- [ ] Support atomic confirm/cancel and safe construction undo before execution. After equipment is installed/used, a reversal is a new demolition/relocation transaction with costs and dependent-job checks, not history erasure.
- [ ] Make room-B commissioning an explicit paid expansion decision. A populated equipment preview is not productive until purchased, connected and commissioned.
- [ ] Register `build-connect-commission`: deliberately obstruct a rolling-bench aisle and misconnect a duct, observe rejection, correct them, purchase once, save/reload and verify source revision/derived layout separation.

Concrete scenario for the native test:

```text
preview bench in required cart aisle -> blocked; money unchanged
cancel preview                       -> no instance created
confirm valid purchase twice         -> one owned instance/one charge
connect incompatible water/air ports  -> rejected; topology unchanged
commission without power/drain       -> unavailable with named reasons
```

## G03 — Operable equipment and commissioning

**Create:** `game/Source/CanopyFoundry/Equipment/{EquipmentPresenter,RollingBenchPresenter,ControlPanel,CommissioningView}.{h,cpp}`, `game/Content/Equipment/Animations/`, `tests/qualification/equipment_work.py`.

- [ ] Bind power indicators, fan rotation/audio, light emission, pump state and panel controls to authoritative delivered/operating state. Visual motion cannot force capacity when the network is disconnected.
- [ ] Implement rolling-bench motion with its actual rail constraints, end stops and swept collision. Recompute access routes on domain-confirmed movement; stop motion safely when the path is occupied.
- [ ] Implement hands-on commissioning: inspect connections, power on, run a short functional check, observe available plant capacity, acknowledge unresolved failures. Passing a checklist cannot override a failed simulation precondition.
- [ ] Implement inspection/replaceable filter access with original/approved detailed geometry. Service panels and connector pivots must agree with the equipment's semantic and collision model.
- [ ] Show desired versus delivered climate/flow and a concise capacity explanation; use generic illustrative units rather than invented vendor-certified values when data is not qualified.
- [ ] Register `equipment-work`: operate the bench across its allowed travel, commission with/without supply, replace a filter and observe the network operating point recover. Commit visual and logical evidence together.

## G04 — Crops that visibly develop and reflect their history

**Create:** `game/Source/CanopyFoundry/Crops/{CohortPresenter,PlantClusterRenderer,CropInspection}.{h,cpp}`, `game/Content/Crops/Lettuce/`, `content/definitions/visuals/lettuce-v1.json`, `tests/qualification/crop_visuals.py`.

- [ ] Author rights-cleared propagation, juvenile, mature and stressed lettuce representations with PBR materials, consistent scale, collision policy and LODs. Use individual-looking representatives of the cohort, not a simulation object per leaf/plant.
- [ ] Map stage/development/biomass/stress to appearance through a versioned visual profile. Interpolate appearance in render time; do not interpolate away a harvested/dead state transition.
- [ ] Show batch identity, planting date, readiness, quality risks and model-validity label in inspection. Missing data remains unknown; no fake precise nutrient diagnosis from a color tint alone.
- [ ] Implement transplant/harvest visual ownership: a tray represents real reserved inventory; a harvested bed becomes empty when the core transaction commits.
- [ ] Register `crop-visual-history`: compare healthy and stress-history branches at the same final climate, then harvest/save/reload. Validate silhouettes, readable distinctions and state consistency at near/far LOD.
- [ ] Profile one populated room before filling the greenhouse; commit approved cluster art and its budget report.

**Check:** `python scripts/qualify.py --case crop-visual-history`. Expected: visible change over the cycle, no duplicated plants/inventory on reload, no Actor-per-plant growth.

## G05 — Personal work, hired workers and earned automation

**Create:** `game/Source/CanopyFoundry/Work/{PlayerWork,CarryableInventory,CartInteraction,WorkerPresenter,JobBoard}.{h,cpp}`, `game/Content/Workers/`, `content/definitions/progression/labor.json`, `tests/qualification/manual_and_worker.py`.

- [ ] Implement carrying one tray, cart loading, nursery-to-room transfer and harvesting/processing station actions against S05 reservations. Work progress and input consumption are explicit and interruptible.
- [ ] Switch first-/third-person during tray carrying, cart pushing and active station work; preserve the same held item, reservation, progress and movement authority. Overhead/time-lapse returns to the remembered hands-on view. No duplicated mesh-owned stock, lost held object or restarted work animation may complete another task.
- [ ] Add one hired helper able to perform the same supported jobs. The player can inspect job/route/blocking cause and reassign priorities; hiring creates wages and shift limits.
- [ ] Build pooled nearby worker animation/navigation from logical routes. Despawn/respawn distant visual workers without changing arrival times, inventory or job progress.
- [ ] Add a purchasable automation rule for a repeated transfer task after the player has completed that workflow. Automation respects stock, capacity, schedules and shared resources; no free off-map labor.
- [ ] Make fast-forward mode hand off or safely pause personal physical work. Do not allow a cart or held tray to continue under uncontrolled 1440× physics.
- [ ] Register `manual-versus-worker`: perform a transfer personally, then with a worker and an automation rule; compare conserved stock, earned work and costs. Commit the full repeatable operation, not only a worker walking animation.

## G06 — Contracts, purchasing and the first shipment

**Create:** `game/Source/CanopyFoundry/Business/{CatalogViewModel,ContractBoard,InventoryViewModel,DispatchInteraction,OperatingStatement}.{h,cpp}`, `game/Content/UI/Business/`, `tests/qualification/first_sale.py`.

- [ ] Present equipment ownership and variant-specific total cost, service needs and validity metadata. Sponsored placement is labeled and cannot replace normal sorting or generic alternatives.
- [ ] Implement accept/decline contract, inspect quality/deadline/quantity, buy supplies and view expected versus actual resource costs. Clearly distinguish game credits/fictional pricing from sourced vendor specifications.
- [ ] Make processing and packing consume actual stock and station time. A shipment leaves the world only after the validated dispatch transaction commits.
- [ ] Show a concise operating statement: revenue, crop loss, electricity, water/inputs, wages, maintenance, capital/cash and remaining orders. Explain a change using causal events rather than a decorative profit chart.
- [ ] Implement a funded rescue/downsizing offer after a recoverable first failure. Its debt/equipment sale consequences are explicit in the ledger; no hidden balance reset.
- [ ] Register `first-sale-native`: harvest, grade, dispatch, reload/retry and verify a single payment and stock deduction. Capture the actual departing shipment and resulting investment choices.

## G07 — Diagnosis through the world, not a quiz

**Create:** `game/Source/CanopyFoundry/Diagnostics/{RoomOverlay,TrendView,FaultInspection,CausalSummary}.{h,cpp}`, `game/Content/Audio/Equipment/`, `tests/qualification/filter_diagnosis.py`.

- [ ] Implement temperature/moisture/light/utility-load/access overlays with independent legends, units, timestamps and color-independent symbols. Label well-mixed zone results; do not draw fabricated CFD detail.
- [ ] Add short trend views that show setpoint, delivered state, sensor observation and source freshness distinctly. Underlying truth is exposed only according to scenario/difficulty policy.
- [ ] Present the first filter obstruction through load/airflow changes, fan/audio state and inspectable filter condition. Do not display a universal diagnosis banner before the player investigates.
- [ ] Offer physically meaningful actions—service the filter, temporarily reduce loads, or change schedule—with prices/time and predicted direction of effects, not guaranteed exact outcomes.
- [ ] Register `filter-diagnosis-native`: inject the authored seed, inspect, choose a corrective action, watch recovery and explain its effect on crop/cost. Run a second strategy from the same save branch for comparison.
- [ ] Commit the explanatory UI and sensory cues; retain model-limit labeling and avoid exact-message/source-text tests.

## G08 — Complete first-cycle acceptance and reinvestment

**Create:** `game/Source/CanopyFoundry/Career/{CareerDirector,ProgressionGoals,InvestmentComparison}.{h,cpp}`, `tests/qualification/first_shipment_native.py`, `tests/playtest/first-shipment-protocol.md`.

- [ ] Integrate G01–G07 into a continuous career: construction, commissioning, nursery/crop work, diagnosis, processing, sale and reinvestment. Goals respond to actual domain state rather than executing the actions for the player.
- [ ] Add a second crop cycle and mutually affordable upgrade choices: commission room B, increase shared capacity or automate transfers. The purchased change affects the next cycle through the real simulation and ledger.
- [ ] Run controller and mouse/keyboard walkthroughs on every native target in both hands-on views, including repeated view changes during work, overhead return, camera obstruction, pause, fast-forward, save/reload and recovery. No view requires debug commands or permits reach through walls.
- [ ] Register `first-shipment-native`; capture a full playable trace with a real failure, correction, sale and next investment. Compare its domain outcome with the equivalent headless command stream.
- [ ] Conduct the five-person formative playtest specified in the product spec. Record task completion, unprompted causal explanation, confusion, repeated-action burden and desire/choice to start another cycle. Fix failed usability/loop goals before producing flagship art.
- [ ] Commit the release candidate for the small facility and its evidence. Only this gate establishes the first game loop; F06's walkable room and S08's headless sale are not substitutes.

**Checks:** `python scripts/qualify.py --case first-shipment-native --platform win64` plus Linux/Mac; playtest protocol with actual participants. Expected: one continuous, understandable, replayable crop-to-sale-to-next-cycle experience. No performance or educational-effectiveness claim beyond the recorded experiment.
