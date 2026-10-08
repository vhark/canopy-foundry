# Grownetics: Canopy Foundry

Build your growing operation, master its environment, and scale from your first crop to an industrial growing business.

**Status: product specification and implementation plans. No executable game is included yet.** This is a new game repository, not a rename or fork of the existing Grownetics Sim training study. The title is a working commercial name, pending trademark/domain clearance.

## License

Original project code, documentation and project-owned content are available under [PolyForm Noncommercial 1.0.0](LICENSE), unless a file has its own license. Noncommercial use, modification and redistribution are permitted under those terms; commercial rights require a separate agreement with the relevant copyright holders. This is **source-available**, not OSI open source, because commercial use is restricted. Unreal Engine, third-party dependencies, assets and trademarks retain their own terms; the project license does not relicense them.

Active implementation and its qualification evidence are on [`feat/canopy-foundry-implementation`](https://github.com/vhark/canopy-foundry/tree/feat/canopy-foundry-implementation). Public source availability is not a claim that a packaged game or commercial release is ready.

## Product decisions

- First-person work, spatial construction, operations management and business progression in one coherent facility.
- Unreal Engine **5.8.1** presentation baseline; engine-independent **C++20** simulation; offline **Python 3.12 / GrowBIM / OpenCEA / Blender / Bonsai / IfcOpenShell** authoring and asset tooling.
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

The master plan links the detailed subsystem plans. Every requirement has an implementation owner/task and an observable acceptance gate. Commands in those plans describe future implementation checks; they have **not** been run against a nonexistent game.

## Repository policy

Public source repository. Do not commit vendor source CAD, confidential facility models, player data, Unreal Engine source/binaries or credentials. Approved original Unreal content uses Git LFS; restricted source masters and engine/build artifacts remain in access-controlled storage. GitHub Actions logs and artifacts in this repository must be safe for public readers, even for owner-triggered workflows. See [asset governance](docs/bim-and-asset-pipeline.md) and [licensing boundaries](docs/licensing.md).

Existing OpenCEA/GrowBIM contracts remain upstream; their schemas are not copied into a competing authority here. The old Grownetics Sim repository remains intact as evidence of its narrow authoring/replay qualification, not proof of this game's performance or crop accuracy.
