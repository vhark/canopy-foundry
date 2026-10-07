# Modes, Training, Modding and Cooperative Play Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide Career/Sandbox/Training, inspectable replay and safe creator tools on the common simulation, then deliver a separately qualified 2–4 player cooperative extension.

**Architecture:** Mode/scenario policies configure the same core rather than forking physics. Replay/debrief branches retain their model provenance. Multiplayer keeps one authoritative host and transmits validated intent plus snapshots/events, not floating-point lockstep.

**Tech Stack:** CanopySim, Unreal UI/input/networking, declarative content packs, Python creator/qualification tools; EOS sessions/P2P for optional internet co-op, native LAN testing.

---

T01–T05 are part of the single-player product plan. T06 controls any training-validity claims. C01–C04 are an explicitly separate post-single-player delivery gate; the launch game works without internet accounts, EOS or a co-op service. All commands below are future checks.

## T01 — Shared mode policies and sandbox

**Dependencies:** F04/S08/G08.

**Create:** `contracts/mode-policy.schema.json`, `core/src/scenario/mode_policy.cpp`, `game/Source/CanopyFoundry/Modes/{ModeSelection,SandboxSetup}.{h,cpp}`, `content/definitions/modes/{career,sandbox,training}.json`, `tests/qualification/modes.py`.

- [ ] Define rules for initial resources, allowed commands, failure/weather seeds, time controls, recovery, objectives and visibility of ground truth. Keep equipment/crop definitions identical when comparing modes.
- [ ] Implement Career selection and Sandbox facility/crop/money/weather/failure/time configuration. Apply configuration atomically at new-campaign creation or as a labeled experimental branch, not by silently rewriting an old save.
- [ ] Make infinite-money/no-failure settings explicit in save/export/UI; do not let a Sandbox result masquerade as a scored training run or career achievement.
- [ ] Test the same physical scenario and commands across mode policies: unchanged physics when policy does not intentionally alter inputs, different finance/failure permissions where configured.
- [ ] Register `mode-policy`; create/save/reload each mode and verify settings/labels/allowed actions remain consistent. Commit supported presets, not empty menu entries.

## T02 — Scenario editor and operational diagnosis cases

**Dependencies:** T01/S07/B06.

**Create:** `contracts/training-scenario.schema.json`, `apps/canopy-author/canopy_author/scenarios/{compile,validate}.py`, `game/Source/CanopyFoundry/Training/{ScenarioEditor,ObjectiveRuntime}.{h,cpp}`, `content/definitions/training/`, `tests/qualification/training_cases.py`.

- [ ] Define scenario initial state, facility/model/pack hashes, seed, permitted interventions, triggers, success/failure windows, scoring dimensions and source/validity claims. Objectives query domain events/state, not UI clicks or hardcoded success buttons.
- [ ] Build an editor for selecting facility/room/equipment, adding supported triggers, configuring objective thresholds and previewing a headless run. Reject dangling IDs, impossible required actions, undisclosed synthetic readings and conflicting clocks.
- [ ] Author six complete cases: stale sensor; undersized shared utility; simultaneous harvest processing bottleneck; pump/climate-control failure; expansion versus retrofit; recipe transfer between weather/site conditions.
- [ ] Provide at least two valid strategies where physically/model-wise meaningful. Preserve unsafe/out-of-domain model warnings and let the scenario fail honestly rather than forcing every sequence to succeed.
- [ ] Register `training-case-matrix`; play/verify each case, record the accepted actions and actual objective transitions, then save/share an original rights-cleared scenario pack. Commit reproducible case fixtures.

## T03 — Causal debrief and alternative-action branches

**Dependencies:** T02/S08/F05.

**Create:** `core/src/diagnostics/debrief.cpp`, `game/Source/CanopyFoundry/Training/{DebriefView,AlternativeRunView}.{h,cpp}`, `contracts/debrief.schema.json`, `tests/qualification/debrief.py`.

- [ ] Summarize what changed, which commands were accepted/rejected, underlying resource/capacity limits, crop history, task delays and costs. Link conclusions to recorded events/meters, not a language model's unsupported explanation.
- [ ] Implement an explicit alternative-action branch from a checkpoint using the same known exogenous inputs/seed and selected changed commands. Clearly label the result as simulated and identify model/validity differences.
- [ ] Show directional comparisons and uncertainty/domain exclusions. Do not invent a precise real-world loss avoided from an illustrative model or claim a counterfactual from a fixed sensor replay.
- [ ] Test that generating the debrief/alternative branch leaves the live campaign hash, money, inventory and accepted journal unchanged; an alternate model hash cannot be compared as if it were the same experiment.
- [ ] Register `causal-debrief`; verify a filter repair versus load-shedding case and expose its crop/resource/cost tradeoffs. Commit the explanation evidence and short player-facing presentation.

## T04 — Replay timeline, bookmarks and read-only history

**Dependencies:** F04/T01.

**Create:** `game/Source/CanopyFoundry/Replay/{ReplayController,TimelineView,BookmarkStore}.{h,cpp}`, `core/src/persistence/replay_window.cpp`, `tests/qualification/replay_ui.py`.

- [ ] Add timeline navigation over the retained replay window, meaningful event markers, bookmarks and read-only past inspection. Historical construction and room/equipment identity must match the checkpoint, not today's layout.
- [ ] Disable live mutation while inspecting history; Return to Live restores the original authority. Creating a branch is explicit and allocates new identity, with source/model hashes retained.
- [ ] Reconcile pending commands/advances before pause, seek or speed change. A stale response cannot re-anchor the clock to the wrong campaign/time or overwrite a newer view.
- [ ] Test backward inspection without live mutation, forward branch execution, changed content versions, retention-window boundaries and rapid pause/seek/rate changes under delayed processing.
- [ ] Register `replay-ui`; perform the actual native interactions with mouse and controller. Commit the retained-history policy and observed behavior.

## T05 — Safe data-driven mod/creator SDK

**Dependencies:** B02/B03/V01 and F03/F04. Implement to the R02 security requirements; R02 subsequently audits this implementation and is not a prerequisite for writing it.

**Create:** `apps/canopy-author/canopy_author/mods/{validate,build,sign}.py`, `contracts/mod-manifest.schema.json`, `content/sdk/original-example/`, `game/Source/CanopyFoundry/Content/PackBrowser.{h,cpp}`, `tests/qualification/modpacks.py`.

- [ ] Publish a complete original example equipment/scenario pack with semantic definitions, approved mesh, ports, model domain and content budget. Exclude vendor masters, logos, restricted performance maps and engine-source redistribution.
- [ ] Define local unsigned versus curated signed pack trust policies. Both validate schemas, units, bounds, IDs, archive paths/sizes, dependency hashes, model domains and content budgets. No native code, uploaded Python, Blueprint bytecode from an untrusted origin or arbitrary filesystem/network access.
- [ ] Build packs using the separate creator toolchain, not runtime CAD conversion. Record input/artifact hashes, toolchain and rights provenance. Require compatible physics pack hashes for network/scored training use.
- [ ] Implement install/disable/version rollback and missing-content diagnostics without corrupting saved worlds. A render-only neutral fallback is allowed; changing physics requires an explicit migration/branch.
- [ ] Register `modpack-admission`; reject traversal/zip-bomb/oversized texture/invalid port/unknown executable cases and run the original admitted pack in all target packages. Commit a usable SDK recipe, not just an empty extension interface.

## T06 — Stated-use training and model validity review

**Dependencies:** T02/T03 plus qualified datasets and reviewers when stronger claims are sought.

**Create:** `content/validation/model-register.json`, `contracts/stated-use-review.schema.json`, `tests/playtest/training-transfer-protocol.md`, `apps/canopy-author/canopy_author/model_claims.py`.

- [ ] Record each model's source/license, fitted range, fitting data, held-out evaluation, exclusions, error measures, intended use and named review. Unknown/unqualified entries remain illustrative and cannot be upgraded by sponsorship or a passing software test.
- [ ] Keep calibrated and validated-for-stated-use claims distinct. Independent validation compares against data not used in fitting; successful in-game play does not certify a real facility design.
- [ ] Define a consented learner study with pre/post diagnosis tasks, a changed scenario, recorded assistance and declared limitations. A developer walkthrough is recorded as such, not counted as an independent learner.
- [ ] Implement claim admission checks: missing evidence/reviewer/validity domain blocks a stronger label while permitting honestly illustrative gameplay. Review dataset permissions before distributing any evidence.
- [ ] Register `model-claim-admission`; test that incomplete or contradictory evidence cannot produce a validated label. Keep unsupported professional claims out of store pages and sponsored materials.

## C01 — Host authority and network command protocol

**Dependencies:** single-player release gates, F02/F04, P06.

**Create:** `contracts/network.fbs`, `game/Source/CanopyFoundry/Coop/{SessionAuthority,CommandChannel,ReplicationView}.{h,cpp}`, `core/tests/network_commands_test.cpp`, `tests/qualification/coop_authority.py`.

- [ ] Assign session/player IDs and permissions; the host serializes all commands and assigns authoritative domain ordering. Validate reach, ownership, stock, command revision and payload bounds server-side.
- [ ] Reuse the exact single-player command receipts and domain core. Clients may predict avatar movement/visual previews, never crop output, inventory, purchases, construction acceptance or shared time.
- [ ] Send interest-filtered snapshots/deltas and event receipts at bounded rates; a new viewer receives a consistent snapshot plus subsequent events without advancing a second authority.
- [ ] Test two players purchasing the last item, moving one bench, dispatching one lot and issuing conflicting control changes. Duplicate/reordered packets cannot duplicate outcomes.
- [ ] Register `coop-authority`; compare host domain trace with equivalent single-player commands. Commit protocol/schema compatibility checks and host permission policies.

## C02 — LAN and optional internet sessions

**Create:** `game/Source/CanopyFoundry/Coop/{SessionDirectory,LanTransport,EosSessionAdapter,JoinFlow}.{h,cpp}`, `game/Config/DefaultOnlineSubsystem.ini`, `tests/qualification/coop_join.py`.

- [ ] Implement LAN/direct host-join first, then EOS session discovery/invite/P2P relay through an isolated adapter. Epic/EOS account, credentials, terms and privacy configuration are external prerequisites; do not fake a working online mode without them.
- [ ] Require an explicit online login/consent only for internet co-op. Offline Career/Sandbox/Training launch without the SDK/service/account and do not contact it by default.
- [ ] Verify version and required physics-pack hash agreement before joining. Optional licensed render packs need not match; use admitted neutral visuals where permitted. Do not transmit restricted vendor masters to peers.
- [ ] Implement encrypted platform transport, bounded messages/rate limits, join timeout/cancel, private invite policy and clear connection failure. Never ship server secrets or enable arbitrary remote console commands.
- [ ] Register `coop-join`; prove LAN and authorized internet join/reconnect across Windows, Mac and Linux with four participants. Commit deployment/configuration documentation without secrets.

## C03 — Shared time, collaboration and reconnect

**Create:** `game/Source/CanopyFoundry/Coop/{TimeConsent,SharedConstruction,ReconnectState}.{h,cpp}`, `tests/qualification/coop_play.py`.

- [ ] Define host-governed pause and unanimous fast-forward consent. A player doing physical work can block time-lapse; switching back safely restores interactions without losing reservations.
- [ ] Add shared construction previews with per-player ownership and authoritative reservations; commit/reject one transaction at a time. Collaborators see why an operation is waiting or rejected.
- [ ] Reconnect with a fresh bounded host snapshot and receipt window; pending client commands are matched by ID, not blindly replayed as new purchases.
- [ ] Host exit persists the authoritative save and ends the session clearly. Automatic host migration is excluded from the first co-op release; do not pretend a guest save is authoritative.
- [ ] Register `coop-first-shipment`; four players build, grow, diagnose, harvest and dispatch together, including one disconnect and time-speed change. Commit state/latency/UX evidence.

## C04 — Cooperative release qualification

**Create:** `tests/qualification/coop_soak.py`, `benchmarks/network/coop-conditions.json`, `docs/operations/coop-support.md` during implementation.

- [ ] Test Windows host with Mac/Linux clients and rotate host platforms. Include 250 ms round-trip latency, jitter, 3% loss, duplicate/reordered commands and a two-hour gameplay soak using the actual packaged transport.
- [ ] Verify one host outcome, bounded queues/bandwidth, no economy duplication, compatible old-save loading and playable render/input pacing under faults.
- [ ] Run shader/content/rightful-pack checks on each target; public co-op packs include no source CAD and no unlicensed game asset transfer.
- [ ] Verify offline single-player still starts and completes a cycle with EOS unreachable or intentionally disabled. Confirm consent, privacy notice and diagnostics redact personal tokens.
- [ ] Register `coop-release`; ship only after all host/client matrix cases pass. Publish known limits, rollback/migration policy and service incident behavior. Co-op performance and platform claims refer to this evidence, not to the earlier single-player tests.
